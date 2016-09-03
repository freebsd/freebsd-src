/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2003 Peter Wemm
 * All rights reserved.
 * Copyright (c) 2005-2010 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 * Copyright (c) 2014 Andrew Turner
 * All rights reserved.
 * Copyright (c) 2014-2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from:	@(#)pmap.c	7.7 (Berkeley)	5/12/91
 */
/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jake Burkholder,
 * Safeport Network Services, and Network Associates Laboratories, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
__FBSDID("$FreeBSD$");

/*
 *	Manages physical address maps.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/vmem.h>
#include <sys/vmmeter.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/_unrhdr.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/uma.h>

#include <machine/machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#define	NL0PG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL1PG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL2PG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL3PG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	NUL0E		L0_ENTRIES
#define	NUL1E		(NUL0E * NL1PG)
#define	NUL2E		(NUL1E * NL2PG)

#if !defined(DIAGNOSTIC)
#ifdef __GNUC_GNU_INLINE__
#define PMAP_INLINE	__attribute__((__gnu_inline__)) inline
#else
#define PMAP_INLINE	extern inline
#endif
#else
#define PMAP_INLINE
#endif

/*
 * These are configured by the mair_el1 register. This is set up in locore.S
 */
#define	DEVICE_MEMORY	0
#define	UNCACHED_MEMORY	1
#define	CACHED_MEMORY	2


#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

#define	pmap_l2_pindex(v)	((v) >> L2_SHIFT)
#define	pa_to_pvh(pa)		(&pv_table[pmap_l2_pindex(pa)])

#define	NPV_LIST_LOCKS	MAXCPU

#define	PHYS_TO_PV_LIST_LOCK(pa)	\
			(&pv_list_locks[pa_index(pa) % NPV_LIST_LOCKS])

#define	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa)	do {	\
	struct rwlock **_lockp = (lockp);		\
	struct rwlock *_new_lock;			\
							\
	_new_lock = PHYS_TO_PV_LIST_LOCK(pa);		\
	if (_new_lock != *_lockp) {			\
		if (*_lockp != NULL)			\
			rw_wunlock(*_lockp);		\
		*_lockp = _new_lock;			\
		rw_wlock(*_lockp);			\
	}						\
} while (0)

#define	CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m)	\
			CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, VM_PAGE_TO_PHYS(m))

#define	RELEASE_PV_LIST_LOCK(lockp)		do {	\
	struct rwlock **_lockp = (lockp);		\
							\
	if (*_lockp != NULL) {				\
		rw_wunlock(*_lockp);			\
		*_lockp = NULL;				\
	}						\
} while (0)

#define	VM_PAGE_TO_PV_LIST_LOCK(m)	\
			PHYS_TO_PV_LIST_LOCK(VM_PAGE_TO_PHYS(m))

struct pmap kernel_pmap_store;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t kernel_vm_end = 0;

struct msgbuf *msgbufp = NULL;

/*
 * Data for the pv entry allocation mechanism.
 * Updates to pv_invl_gen are protected by the pv_list_locks[]
 * elements, but reads are not.
 */
static struct md_page *pv_table;
static struct md_page pv_dummy;

vm_paddr_t dmap_phys_base;	/* The start of the dmap region */
vm_paddr_t dmap_phys_max;	/* The limit of the dmap region */
vm_offset_t dmap_max_addr;	/* The virtual address limit of the dmap */

/* This code assumes all L1 DMAP entries will be used */
CTASSERT((DMAP_MIN_ADDRESS  & ~L0_OFFSET) == DMAP_MIN_ADDRESS);
CTASSERT((DMAP_MAX_ADDRESS  & ~L0_OFFSET) == DMAP_MAX_ADDRESS);

#define	DMAP_TABLES	((DMAP_MAX_ADDRESS - DMAP_MIN_ADDRESS) >> L0_SHIFT)
extern pt_entry_t pagetable_dmap[];

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

static int superpages_enabled = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, superpages_enabled,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &superpages_enabled, 0,
    "Are large page mappings enabled?");

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static struct mtx pv_chunks_mutex;
static struct rwlock pv_list_locks[NPV_LIST_LOCKS];

static void	free_pv_chunk(struct pv_chunk *pc);
static void	free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t pmap, struct rwlock **lockp);
static vm_page_t reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp);
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
		    vm_offset_t va);

static int pmap_change_attr(vm_offset_t va, vm_size_t size, int mode);
static int pmap_change_attr_locked(vm_offset_t va, vm_size_t size, int mode);
static pt_entry_t *pmap_demote_l1(pmap_t pmap, pt_entry_t *l1, vm_offset_t va);
static pt_entry_t *pmap_demote_l2_locked(pmap_t pmap, pt_entry_t *l2,
    vm_offset_t va, struct rwlock **lockp);
static pt_entry_t *pmap_demote_l2(pmap_t pmap, pt_entry_t *l2, vm_offset_t va);
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp);
static int pmap_remove_l3(pmap_t pmap, pt_entry_t *l3, vm_offset_t sva,
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m, struct rwlock **lockp);

static vm_page_t _pmap_alloc_l3(pmap_t pmap, vm_pindex_t ptepindex,
		struct rwlock **lockp);

static void _pmap_unwire_l3(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free);
static int pmap_unuse_l3(pmap_t, vm_offset_t, pd_entry_t, struct spglist *);

/*
 * These load the old table data and store the new value.
 * They need to be atomic as the System MMU may write to the table at
 * the same time as the CPU.
 */
#define	pmap_load_store(table, entry) atomic_swap_64(table, entry)
#define	pmap_set(table, mask) atomic_set_64(table, mask)
#define	pmap_load_clear(table) atomic_swap_64(table, 0)
#define	pmap_load(table) (*table)

/********************/
/* Inline functions */
/********************/

static __inline void
pagecopy(void *s, void *d)
{

	memcpy(d, s, PAGE_SIZE);
}

#define	pmap_l0_index(va)	(((va) >> L0_SHIFT) & L0_ADDR_MASK)
#define	pmap_l1_index(va)	(((va) >> L1_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l2_index(va)	(((va) >> L2_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l3_index(va)	(((va) >> L3_SHIFT) & Ln_ADDR_MASK)

static __inline pd_entry_t *
pmap_l0(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_l0[pmap_l0_index(va)]);
}

static __inline pd_entry_t *
pmap_l0_to_l1(pd_entry_t *l0, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = (pd_entry_t *)PHYS_TO_DMAP(pmap_load(l0) & ~ATTR_MASK);
	return (&l1[pmap_l1_index(va)]);
}

static __inline pd_entry_t *
pmap_l1(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l0;

	l0 = pmap_l0(pmap, va);
	if ((pmap_load(l0) & ATTR_DESCR_MASK) != L0_TABLE)
		return (NULL);

	return (pmap_l0_to_l1(l0, va));
}

static __inline pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = (pd_entry_t *)PHYS_TO_DMAP(pmap_load(l1) & ~ATTR_MASK);
	return (&l2[pmap_l2_index(va)]);
}

static __inline pd_entry_t *
pmap_l2(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = pmap_l1(pmap, va);
	if ((pmap_load(l1) & ATTR_DESCR_MASK) != L1_TABLE)
		return (NULL);

	return (pmap_l1_to_l2(l1, va));
}

static __inline pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
	pt_entry_t *l3;

	l3 = (pd_entry_t *)PHYS_TO_DMAP(pmap_load(l2) & ~ATTR_MASK);
	return (&l3[pmap_l3_index(va)]);
}

/*
 * Returns the lowest valid pde for a given virtual address.
 * The next level may or may not point to a valid page or block.
 */
static __inline pd_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t va, int *level)
{
	pd_entry_t *l0, *l1, *l2, desc;

	l0 = pmap_l0(pmap, va);
	desc = pmap_load(l0) & ATTR_DESCR_MASK;
	if (desc != L0_TABLE) {
		*level = -1;
		return (NULL);
	}

	l1 = pmap_l0_to_l1(l0, va);
	desc = pmap_load(l1) & ATTR_DESCR_MASK;
	if (desc != L1_TABLE) {
		*level = 0;
		return (l0);
	}

	l2 = pmap_l1_to_l2(l1, va);
	desc = pmap_load(l2) & ATTR_DESCR_MASK;
	if (desc != L2_TABLE) {
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
pmap_pte(pmap_t pmap, vm_offset_t va, int *level)
{
	pd_entry_t *l1, *l2, desc;
	pt_entry_t *l3;

	l1 = pmap_l1(pmap, va);
	if (l1 == NULL) {
		*level = 0;
		return (NULL);
	}
	desc = pmap_load(l1) & ATTR_DESCR_MASK;
	if (desc == L1_BLOCK) {
		*level = 1;
		return (l1);
	}

	if (desc != L1_TABLE) {
		*level = 1;
		return (NULL);
	}

	l2 = pmap_l1_to_l2(l1, va);
	desc = pmap_load(l2) & ATTR_DESCR_MASK;
	if (desc == L2_BLOCK) {
		*level = 2;
		return (l2);
	}

	if (desc != L2_TABLE) {
		*level = 2;
		return (NULL);
	}

	*level = 3;
	l3 = pmap_l2_to_l3(l2, va);
	if ((pmap_load(l3) & ATTR_DESCR_MASK) != L3_PAGE)
		return (NULL);

	return (l3);
}

static inline bool
pmap_superpages_enabled(void)
{

	return (superpages_enabled != 0);
}

bool
pmap_get_tables(pmap_t pmap, vm_offset_t va, pd_entry_t **l0, pd_entry_t **l1,
    pd_entry_t **l2, pt_entry_t **l3)
{
	pd_entry_t *l0p, *l1p, *l2p;

	if (pmap->pm_l0 == NULL)
		return (false);

	l0p = pmap_l0(pmap, va);
	*l0 = l0p;

	if ((pmap_load(l0p) & ATTR_DESCR_MASK) != L0_TABLE)
		return (false);

	l1p = pmap_l0_to_l1(l0p, va);
	*l1 = l1p;

	if ((pmap_load(l1p) & ATTR_DESCR_MASK) == L1_BLOCK) {
		*l2 = NULL;
		*l3 = NULL;
		return (true);
	}

	if ((pmap_load(l1p) & ATTR_DESCR_MASK) != L1_TABLE)
		return (false);

	l2p = pmap_l1_to_l2(l1p, va);
	*l2 = l2p;

	if ((pmap_load(l2p) & ATTR_DESCR_MASK) == L2_BLOCK) {
		*l3 = NULL;
		return (true);
	}

	*l3 = pmap_l2_to_l3(l2p, va);

	return (true);
}

static __inline int
pmap_is_current(pmap_t pmap)
{

	return ((pmap == pmap_kernel()) ||
	    (pmap == curthread->td_proc->p_vmspace->vm_map.pmap));
}

static __inline int
pmap_l3_valid(pt_entry_t l3)
{

	return ((l3 & ATTR_DESCR_MASK) == L3_PAGE);
}


/* Is a level 1 or 2entry a valid block and cacheable */
CTASSERT(L1_BLOCK == L2_BLOCK);
static __inline int
pmap_pte_valid_cacheable(pt_entry_t pte)
{

	return (((pte & ATTR_DESCR_MASK) == L1_BLOCK) &&
	    ((pte & ATTR_IDX_MASK) == ATTR_IDX(CACHED_MEMORY)));
}

static __inline int
pmap_l3_valid_cacheable(pt_entry_t l3)
{

	return (((l3 & ATTR_DESCR_MASK) == L3_PAGE) &&
	    ((l3 & ATTR_IDX_MASK) == ATTR_IDX(CACHED_MEMORY)));
}

#define	PTE_SYNC(pte)	cpu_dcache_wb_range((vm_offset_t)pte, sizeof(*pte))

/*
 * Checks if the page is dirty. We currently lack proper tracking of this on
 * arm64 so for now assume is a page mapped as rw was accessed it is.
 */
static inline int
pmap_page_dirty(pt_entry_t pte)
{

	return ((pte & (ATTR_AF | ATTR_AP_RW_BIT)) ==
	    (ATTR_AF | ATTR_AP(ATTR_AP_RW)));
}

static __inline void
pmap_resident_count_inc(pmap_t pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pmap->pm_stats.resident_count += count;
}

static __inline void
pmap_resident_count_dec(pmap_t pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(pmap->pm_stats.resident_count >= count,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->pm_stats.resident_count, count));
	pmap->pm_stats.resident_count -= count;
}

static pt_entry_t *
pmap_early_page_idx(vm_offset_t l1pt, vm_offset_t va, u_int *l1_slot,
    u_int *l2_slot)
{
	pt_entry_t *l2;
	pd_entry_t *l1;

	l1 = (pd_entry_t *)l1pt;
	*l1_slot = (va >> L1_SHIFT) & Ln_ADDR_MASK;

	/* Check locore has used a table L1 map */
	KASSERT((l1[*l1_slot] & ATTR_DESCR_MASK) == L1_TABLE,
	   ("Invalid bootstrap L1 table"));
	/* Find the address of the L2 table */
	l2 = (pt_entry_t *)init_pt_va;
	*l2_slot = pmap_l2_index(va);

	return (l2);
}

static vm_paddr_t
pmap_early_vtophys(vm_offset_t l1pt, vm_offset_t va)
{
	u_int l1_slot, l2_slot;
	pt_entry_t *l2;

	l2 = pmap_early_page_idx(l1pt, va, &l1_slot, &l2_slot);

	return ((l2[l2_slot] & ~ATTR_MASK) + (va & L2_OFFSET));
}

static void
pmap_bootstrap_dmap(vm_offset_t kern_l1, vm_paddr_t min_pa, vm_paddr_t max_pa)
{
	vm_offset_t va;
	vm_paddr_t pa;
	u_int l1_slot;

	pa = dmap_phys_base = min_pa & ~L1_OFFSET;
	va = DMAP_MIN_ADDRESS;
	for (; va < DMAP_MAX_ADDRESS && pa < max_pa;
	    pa += L1_SIZE, va += L1_SIZE, l1_slot++) {
		l1_slot = ((va - DMAP_MIN_ADDRESS) >> L1_SHIFT);

		pmap_load_store(&pagetable_dmap[l1_slot],
		    (pa & ~L1_OFFSET) | ATTR_DEFAULT |
		    ATTR_IDX(CACHED_MEMORY) | L1_BLOCK);
	}

	/* Set the upper limit of the DMAP region */
	dmap_phys_max = pa;
	dmap_max_addr = va;

	cpu_dcache_wb_range((vm_offset_t)pagetable_dmap,
	    PAGE_SIZE * DMAP_TABLES);
	cpu_tlb_flushID();
}

static vm_offset_t
pmap_bootstrap_l2(vm_offset_t l1pt, vm_offset_t va, vm_offset_t l2_start)
{
	vm_offset_t l2pt;
	vm_paddr_t pa;
	pd_entry_t *l1;
	u_int l1_slot;

	KASSERT((va & L1_OFFSET) == 0, ("Invalid virtual address"));

	l1 = (pd_entry_t *)l1pt;
	l1_slot = pmap_l1_index(va);
	l2pt = l2_start;

	for (; va < VM_MAX_KERNEL_ADDRESS; l1_slot++, va += L1_SIZE) {
		KASSERT(l1_slot < Ln_ENTRIES, ("Invalid L1 index"));

		pa = pmap_early_vtophys(l1pt, l2pt);
		pmap_load_store(&l1[l1_slot],
		    (pa & ~Ln_TABLE_MASK) | L1_TABLE);
		l2pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l2_start, 0, l2pt - l2_start);
	cpu_dcache_wb_range(l2_start, l2pt - l2_start);

	/* Flush the l1 table to ram */
	cpu_dcache_wb_range((vm_offset_t)l1, PAGE_SIZE);

	return l2pt;
}

static vm_offset_t
pmap_bootstrap_l3(vm_offset_t l1pt, vm_offset_t va, vm_offset_t l3_start)
{
	vm_offset_t l2pt, l3pt;
	vm_paddr_t pa;
	pd_entry_t *l2;
	u_int l2_slot;

	KASSERT((va & L2_OFFSET) == 0, ("Invalid virtual address"));

	l2 = pmap_l2(kernel_pmap, va);
	l2 = (pd_entry_t *)rounddown2((uintptr_t)l2, PAGE_SIZE);
	l2pt = (vm_offset_t)l2;
	l2_slot = pmap_l2_index(va);
	l3pt = l3_start;

	for (; va < VM_MAX_KERNEL_ADDRESS; l2_slot++, va += L2_SIZE) {
		KASSERT(l2_slot < Ln_ENTRIES, ("Invalid L2 index"));

		pa = pmap_early_vtophys(l1pt, l3pt);
		pmap_load_store(&l2[l2_slot],
		    (pa & ~Ln_TABLE_MASK) | L2_TABLE);
		l3pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l3_start, 0, l3pt - l3_start);
	cpu_dcache_wb_range(l3_start, l3pt - l3_start);

	cpu_dcache_wb_range((vm_offset_t)l2, PAGE_SIZE);

	return l3pt;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t l0pt, vm_offset_t l1pt, vm_paddr_t kernstart,
    vm_size_t kernlen)
{
	u_int l1_slot, l2_slot, avail_slot, map_slot, used_map_slot;
	uint64_t kern_delta;
	pt_entry_t *l2;
	vm_offset_t va, freemempos;
	vm_offset_t dpcpu, msgbufpv;
	vm_paddr_t pa, max_pa, min_pa;
	int i;

	kern_delta = KERNBASE - kernstart;
	physmem = 0;

	printf("pmap_bootstrap %lx %lx %lx\n", l1pt, kernstart, kernlen);
	printf("%lx\n", l1pt);
	printf("%lx\n", (KERNBASE >> L1_SHIFT) & Ln_ADDR_MASK);

	/* Set this early so we can use the pagetable walking functions */
	kernel_pmap_store.pm_l0 = (pd_entry_t *)l0pt;
	PMAP_LOCK_INIT(kernel_pmap);

	/* Assume the address we were loaded to is a valid physical address */
	min_pa = max_pa = KERNBASE - kern_delta;

	/*
	 * Find the minimum physical address. physmap is sorted,
	 * but may contain empty ranges.
	 */
	for (i = 0; i < (physmap_idx * 2); i += 2) {
		if (physmap[i] == physmap[i + 1])
			continue;
		if (physmap[i] <= min_pa)
			min_pa = physmap[i];
		if (physmap[i + 1] > max_pa)
			max_pa = physmap[i + 1];
	}

	/* Create a direct map region early so we can use it for pa -> va */
	pmap_bootstrap_dmap(l1pt, min_pa, max_pa);

	va = KERNBASE;
	pa = KERNBASE - kern_delta;

	/*
	 * Start to initialise phys_avail by copying from physmap
	 * up to the physical address KERNBASE points at.
	 */
	map_slot = avail_slot = 0;
	for (; map_slot < (physmap_idx * 2) &&
	    avail_slot < (PHYS_AVAIL_SIZE - 2); map_slot += 2) {
		if (physmap[map_slot] == physmap[map_slot + 1])
			continue;

		if (physmap[map_slot] <= pa &&
		    physmap[map_slot + 1] > pa)
			break;

		phys_avail[avail_slot] = physmap[map_slot];
		phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;
		avail_slot += 2;
	}

	/* Add the memory before the kernel */
	if (physmap[avail_slot] < pa && avail_slot < (PHYS_AVAIL_SIZE - 2)) {
		phys_avail[avail_slot] = physmap[map_slot];
		phys_avail[avail_slot + 1] = pa;
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;
		avail_slot += 2;
	}
	used_map_slot = map_slot;

	/*
	 * Read the page table to find out what is already mapped.
	 * This assumes we have mapped a block of memory from KERNBASE
	 * using a single L1 entry.
	 */
	l2 = pmap_early_page_idx(l1pt, KERNBASE, &l1_slot, &l2_slot);

	/* Sanity check the index, KERNBASE should be the first VA */
	KASSERT(l2_slot == 0, ("The L2 index is non-zero"));

	/* Find how many pages we have mapped */
	for (; l2_slot < Ln_ENTRIES; l2_slot++) {
		if ((l2[l2_slot] & ATTR_DESCR_MASK) == 0)
			break;

		/* Check locore used L2 blocks */
		KASSERT((l2[l2_slot] & ATTR_DESCR_MASK) == L2_BLOCK,
		    ("Invalid bootstrap L2 table"));
		KASSERT((l2[l2_slot] & ~ATTR_MASK) == pa,
		    ("Incorrect PA in L2 table"));

		va += L2_SIZE;
		pa += L2_SIZE;
	}

	va = roundup2(va, L1_SIZE);

	freemempos = KERNBASE + kernlen;
	freemempos = roundup2(freemempos, PAGE_SIZE);
	/* Create the l2 tables up to VM_MAX_KERNEL_ADDRESS */
	freemempos = pmap_bootstrap_l2(l1pt, va, freemempos);
	/* And the l3 tables for the early devmap */
	freemempos = pmap_bootstrap_l3(l1pt,
	    VM_MAX_KERNEL_ADDRESS - L2_SIZE, freemempos);

	cpu_tlb_flushID();

#define alloc_pages(var, np)						\
	(var) = freemempos;						\
	freemempos += (np * PAGE_SIZE);					\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	/* Allocate dynamic per-cpu area. */
	alloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu, 0);

	/* Allocate memory for the msgbuf, e.g. for /sbin/dmesg */
	alloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);
	msgbufp = (void *)msgbufpv;

	virtual_avail = roundup2(freemempos, L1_SIZE);
	virtual_end = VM_MAX_KERNEL_ADDRESS - L2_SIZE;
	kernel_vm_end = virtual_avail;

	pa = pmap_early_vtophys(l1pt, freemempos);

	/* Finish initialising physmap */
	map_slot = used_map_slot;
	for (; avail_slot < (PHYS_AVAIL_SIZE - 2) &&
	    map_slot < (physmap_idx * 2); map_slot += 2) {
		if (physmap[map_slot] == physmap[map_slot + 1])
			continue;

		/* Have we used the current range? */
		if (physmap[map_slot + 1] <= pa)
			continue;

		/* Do we need to split the entry? */
		if (physmap[map_slot] < pa) {
			phys_avail[avail_slot] = pa;
			phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		} else {
			phys_avail[avail_slot] = physmap[map_slot];
			phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		}
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;

		avail_slot += 2;
	}
	phys_avail[avail_slot] = 0;
	phys_avail[avail_slot + 1] = 0;

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".
	 */
	Maxmem = atop(phys_avail[avail_slot - 1]);

	cpu_tlb_flushID();
}

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_memattr = VM_MEMATTR_WRITE_BACK;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	vm_size_t s;
	int i, pv_npg;

	/*
	 * Are large page mappings enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.superpages_enabled", &superpages_enabled);

	/*
	 * Initialize the pv chunk list mutex.
	 */
	mtx_init(&pv_chunks_mutex, "pmap pv chunk list", NULL, MTX_DEF);

	/*
	 * Initialize the pool of pv list locks.
	 */
	for (i = 0; i < NPV_LIST_LOCKS; i++)
		rw_init(&pv_list_locks[i], "pmap pv list");

	/*
	 * Calculate the size of the pv head table for superpages.
	 */
	pv_npg = howmany(vm_phys_segs[vm_phys_nsegs - 1].end, L2_SIZE);

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)(pv_npg * sizeof(struct md_page));
	s = round_page(s);
	pv_table = (struct md_page *)kmem_malloc(kernel_arena, s,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);
	TAILQ_INIT(&pv_dummy.pv_list);
}

static SYSCTL_NODE(_vm_pmap, OID_AUTO, l2, CTLFLAG_RD, 0,
    "2MB page mapping counters");

static u_long pmap_l2_demotions;
SYSCTL_ULONG(_vm_pmap_l2, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_l2_demotions, 0, "2MB page demotions");

static u_long pmap_l2_p_failures;
SYSCTL_ULONG(_vm_pmap_l2, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_l2_p_failures, 0, "2MB page promotion failures");

static u_long pmap_l2_promotions;
SYSCTL_ULONG(_vm_pmap_l2, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_l2_promotions, 0, "2MB page promotions");

/*
 * Invalidate a single TLB entry.
 */
PMAP_INLINE void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{

	sched_pin();
	__asm __volatile(
	    "dsb  ishst		\n"
	    "tlbi vaae1is, %0	\n"
	    "dsb  ish		\n"
	    "isb		\n"
	    : : "r"(va >> PAGE_SHIFT));
	sched_unpin();
}

PMAP_INLINE void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	sched_pin();
	dsb(ishst);
	for (addr = sva; addr < eva; addr += PAGE_SIZE) {
		__asm __volatile(
		    "tlbi vaae1is, %0" : : "r"(addr >> PAGE_SHIFT));
	}
	__asm __volatile(
	    "dsb  ish	\n"
	    "isb	\n");
	sched_unpin();
}

PMAP_INLINE void
pmap_invalidate_all(pmap_t pmap)
{

	sched_pin();
	__asm __volatile(
	    "dsb  ishst		\n"
	    "tlbi vmalle1is	\n"
	    "dsb  ish		\n"
	    "isb		\n");
	sched_unpin();
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *pte, tpte;
	vm_paddr_t pa;
	int lvl;

	pa = 0;
	PMAP_LOCK(pmap);
	/*
	 * Find the block or page map for this virtual address. pmap_pte
	 * will return either a valid block/page entry, or NULL.
	 */
	pte = pmap_pte(pmap, va, &lvl);
	if (pte != NULL) {
		tpte = pmap_load(pte);
		pa = tpte & ~ATTR_MASK;
		switch(lvl) {
		case 1:
			KASSERT((tpte & ATTR_DESCR_MASK) == L1_BLOCK,
			    ("pmap_extract: Invalid L1 pte found: %lx",
			    tpte & ATTR_DESCR_MASK));
			pa |= (va & L1_OFFSET);
			break;
		case 2:
			KASSERT((tpte & ATTR_DESCR_MASK) == L2_BLOCK,
			    ("pmap_extract: Invalid L2 pte found: %lx",
			    tpte & ATTR_DESCR_MASK));
			pa |= (va & L2_OFFSET);
			break;
		case 3:
			KASSERT((tpte & ATTR_DESCR_MASK) == L3_PAGE,
			    ("pmap_extract: Invalid L3 pte found: %lx",
			    tpte & ATTR_DESCR_MASK));
			pa |= (va & L3_OFFSET);
			break;
		}
	}
	PMAP_UNLOCK(pmap);
	return (pa);
}

/*
 *	Routine:	pmap_extract_and_hold
 *	Function:
 *		Atomically extract and hold the physical page
 *		with the given pmap and virtual address pair
 *		if that mapping permits the given protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	pt_entry_t *pte, tpte;
	vm_offset_t off;
	vm_paddr_t pa;
	vm_page_t m;
	int lvl;

	pa = 0;
	m = NULL;
	PMAP_LOCK(pmap);
retry:
	pte = pmap_pte(pmap, va, &lvl);
	if (pte != NULL) {
		tpte = pmap_load(pte);

		KASSERT(lvl > 0 && lvl <= 3,
		    ("pmap_extract_and_hold: Invalid level %d", lvl));
		CTASSERT(L1_BLOCK == L2_BLOCK);
		KASSERT((lvl == 3 && (tpte & ATTR_DESCR_MASK) == L3_PAGE) ||
		    (lvl < 3 && (tpte & ATTR_DESCR_MASK) == L1_BLOCK),
		    ("pmap_extract_and_hold: Invalid pte at L%d: %lx", lvl,
		     tpte & ATTR_DESCR_MASK));
		if (((tpte & ATTR_AP_RW_BIT) == ATTR_AP(ATTR_AP_RW)) ||
		    ((prot & VM_PROT_WRITE) == 0)) {
			switch(lvl) {
			case 1:
				off = va & L1_OFFSET;
				break;
			case 2:
				off = va & L2_OFFSET;
				break;
			case 3:
			default:
				off = 0;
			}
			if (vm_page_pa_tryrelock(pmap,
			    (tpte & ~ATTR_MASK) | off, &pa))
				goto retry;
			m = PHYS_TO_VM_PAGE((tpte & ~ATTR_MASK) | off);
			vm_page_hold(m);
		}
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	pt_entry_t *pte, tpte;
	vm_paddr_t pa;
	int lvl;

	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS) {
		pa = DMAP_TO_PHYS(va);
	} else {
		pa = 0;
		pte = pmap_pte(kernel_pmap, va, &lvl);
		if (pte != NULL) {
			tpte = pmap_load(pte);
			pa = tpte & ~ATTR_MASK;
			switch(lvl) {
			case 1:
				KASSERT((tpte & ATTR_DESCR_MASK) == L1_BLOCK,
				    ("pmap_kextract: Invalid L1 pte found: %lx",
				    tpte & ATTR_DESCR_MASK));
				pa |= (va & L1_OFFSET);
				break;
			case 2:
				KASSERT((tpte & ATTR_DESCR_MASK) == L2_BLOCK,
				    ("pmap_kextract: Invalid L2 pte found: %lx",
				    tpte & ATTR_DESCR_MASK));
				pa |= (va & L2_OFFSET);
				break;
			case 3:
				KASSERT((tpte & ATTR_DESCR_MASK) == L3_PAGE,
				    ("pmap_kextract: Invalid L3 pte found: %lx",
				    tpte & ATTR_DESCR_MASK));
				pa |= (va & L3_OFFSET);
				break;
			}
		}
	}
	return (pa);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

static void
pmap_kenter(vm_offset_t sva, vm_size_t size, vm_paddr_t pa, int mode)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_offset_t va;
	int lvl;

	KASSERT((pa & L3_OFFSET) == 0,
	   ("pmap_kenter: Invalid physical address"));
	KASSERT((sva & L3_OFFSET) == 0,
	   ("pmap_kenter: Invalid virtual address"));
	KASSERT((size & PAGE_MASK) == 0,
	    ("pmap_kenter: Mapping is not page-sized"));

	va = sva;
	while (size != 0) {
		pde = pmap_pde(kernel_pmap, va, &lvl);
		KASSERT(pde != NULL,
		    ("pmap_kenter: Invalid page entry, va: 0x%lx", va));
		KASSERT(lvl == 2, ("pmap_kenter: Invalid level %d", lvl));

		pte = pmap_l2_to_l3(pde, va);
		pmap_load_store(pte, (pa & ~L3_OFFSET) | ATTR_DEFAULT |
		    ATTR_IDX(mode) | L3_PAGE);
		PTE_SYNC(pte);

		va += PAGE_SIZE;
		pa += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

void
pmap_kenter_device(vm_offset_t sva, vm_size_t size, vm_paddr_t pa)
{

	pmap_kenter(sva, size, pa, DEVICE_MEMORY);
}

/*
 * Remove a page from the kernel pagetables.
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;
	int lvl;

	pte = pmap_pte(kernel_pmap, va, &lvl);
	KASSERT(pte != NULL, ("pmap_kremove: Invalid address"));
	KASSERT(lvl == 3, ("pmap_kremove: Invalid pte level %d", lvl));

	if (pmap_l3_valid_cacheable(pmap_load(pte)))
		cpu_dcache_wb_range(va, L3_SIZE);
	pmap_load_clear(pte);
	PTE_SYNC(pte);
	pmap_invalidate_page(kernel_pmap, va);
}

void
pmap_kremove_device(vm_offset_t sva, vm_size_t size)
{
	pt_entry_t *pte;
	vm_offset_t va;
	int lvl;

	KASSERT((sva & L3_OFFSET) == 0,
	   ("pmap_kremove_device: Invalid virtual address"));
	KASSERT((size & PAGE_MASK) == 0,
	    ("pmap_kremove_device: Mapping is not page-sized"));

	va = sva;
	while (size != 0) {
		pte = pmap_pte(kernel_pmap, va, &lvl);
		KASSERT(pte != NULL, ("Invalid page table, va: 0x%lx", va));
		KASSERT(lvl == 3,
		    ("Invalid device pagetable level: %d != 3", lvl));
		pmap_load_clear(pte);
		PTE_SYNC(pte);

		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	The value passed in '*virt' is a suggested virtual address for
 *	the mapping. Architectures which can support a direct-mapped
 *	physical to virtual region can return the appropriate address
 *	within that region, leaving '*virt' unchanged. Other
 *	architectures should map the pages starting at '*virt' and
 *	update '*virt' with the first usable address after the mapped
 *	region.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	return PHYS_TO_DMAP(start);
}


/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{
	pd_entry_t *pde;
	pt_entry_t *pte, pa;
	vm_offset_t va;
	vm_page_t m;
	int i, lvl;

	va = sva;
	for (i = 0; i < count; i++) {
		pde = pmap_pde(kernel_pmap, va, &lvl);
		KASSERT(pde != NULL,
		    ("pmap_qenter: Invalid page entry, va: 0x%lx", va));
		KASSERT(lvl == 2,
		    ("pmap_qenter: Invalid level %d", lvl));

		m = ma[i];
		pa = VM_PAGE_TO_PHYS(m) | ATTR_DEFAULT | ATTR_AP(ATTR_AP_RW) |
		    ATTR_IDX(m->md.pv_memattr) | L3_PAGE;
		pte = pmap_l2_to_l3(pde, va);
		pmap_load_store(pte, pa);
		PTE_SYNC(pte);

		va += L3_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/*
 * This routine tears out page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	pt_entry_t *pte;
	vm_offset_t va;
	int lvl;

	KASSERT(sva >= VM_MIN_KERNEL_ADDRESS, ("usermode va %lx", sva));

	va = sva;
	while (count-- > 0) {
		pte = pmap_pte(kernel_pmap, va, &lvl);
		KASSERT(lvl == 3,
		    ("Invalid device pagetable level: %d != 3", lvl));
		if (pte != NULL) {
			if (pmap_l3_valid_cacheable(pmap_load(pte)))
				cpu_dcache_wb_range(va, L3_SIZE);
			pmap_load_clear(pte);
			PTE_SYNC(pte);
		}

		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/
static __inline void
pmap_free_zero_pages(struct spglist *free)
{
	vm_page_t m;

	while ((m = SLIST_FIRST(free)) != NULL) {
		SLIST_REMOVE_HEAD(free, plinks.s.ss);
		/* Preserve the page's PG_ZERO setting. */
		vm_page_free_toq(m);
	}
}

/*
 * Schedule the specified unused page table page to be freed.  Specifically,
 * add the page to the specified list of pages that will be released to the
 * physical memory manager after the TLB has been updated.
 */
static __inline void
pmap_add_delayed_free_list(vm_page_t m, struct spglist *free,
    boolean_t set_PG_ZERO)
{

	if (set_PG_ZERO)
		m->flags |= PG_ZERO;
	else
		m->flags &= ~PG_ZERO;
	SLIST_INSERT_HEAD(free, m, plinks.s.ss);
}

/*
 * Decrements a page table page's wire count, which is used to record the
 * number of valid page table entries within the page.  If the wire count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static inline boolean_t
pmap_unwire_l3(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	--m->wire_count;
	if (m->wire_count == 0) {
		_pmap_unwire_l3(pmap, va, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_pmap_unwire_l3(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * unmap the page table page
	 */
	if (m->pindex >= (NUL2E + NUL1E)) {
		/* l1 page */
		pd_entry_t *l0;

		l0 = pmap_l0(pmap, va);
		pmap_load_clear(l0);
		PTE_SYNC(l0);
	} else if (m->pindex >= NUL2E) {
		/* l2 page */
		pd_entry_t *l1;

		l1 = pmap_l1(pmap, va);
		pmap_load_clear(l1);
		PTE_SYNC(l1);
	} else {
		/* l3 page */
		pd_entry_t *l2;

		l2 = pmap_l2(pmap, va);
		pmap_load_clear(l2);
		PTE_SYNC(l2);
	}
	pmap_resident_count_dec(pmap, 1);
	if (m->pindex < NUL2E) {
		/* We just released an l3, unhold the matching l2 */
		pd_entry_t *l1, tl1;
		vm_page_t l2pg;

		l1 = pmap_l1(pmap, va);
		tl1 = pmap_load(l1);
		l2pg = PHYS_TO_VM_PAGE(tl1 & ~ATTR_MASK);
		pmap_unwire_l3(pmap, va, l2pg, free);
	} else if (m->pindex < (NUL2E + NUL1E)) {
		/* We just released an l2, unhold the matching l1 */
		pd_entry_t *l0, tl0;
		vm_page_t l1pg;

		l0 = pmap_l0(pmap, va);
		tl0 = pmap_load(l0);
		l1pg = PHYS_TO_VM_PAGE(tl0 & ~ATTR_MASK);
		pmap_unwire_l3(pmap, va, l1pg, free);
	}
	pmap_invalidate_page(pmap, va);

	/*
	 * This is a release store so that the ordinary store unmapping
	 * the page table page is globally performed before TLB shoot-
	 * down is begun.
	 */
	atomic_subtract_rel_int(&vm_cnt.v_wire_count, 1);

	/*
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done
	 */
	pmap_add_delayed_free_list(m, free, TRUE);
}

/*
 * After removing an l3 entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_l3(pmap_t pmap, vm_offset_t va, pd_entry_t ptepde,
    struct spglist *free)
{
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	KASSERT(ptepde != 0, ("pmap_unuse_pt: ptepde != 0"));
	mpte = PHYS_TO_VM_PAGE(ptepde & ~ATTR_MASK);
	return (pmap_unwire_l3(pmap, va, mpte, free));
}

void
pmap_pinit0(pmap_t pmap)
{

	PMAP_LOCK_INIT(pmap);
	bzero(&pmap->pm_stats, sizeof(pmap->pm_stats));
	pmap->pm_l0 = kernel_pmap->pm_l0;
	pmap->pm_root.rt_root = 0;
}

int
pmap_pinit(pmap_t pmap)
{
	vm_paddr_t l0phys;
	vm_page_t l0pt;

	/*
	 * allocate the l0 page
	 */
	while ((l0pt = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL)
		VM_WAIT;

	l0phys = VM_PAGE_TO_PHYS(l0pt);
	pmap->pm_l0 = (pd_entry_t *)PHYS_TO_DMAP(l0phys);

	if ((l0pt->flags & PG_ZERO) == 0)
		pagezero(pmap->pm_l0);

	pmap->pm_root.rt_root = 0;
	bzero(&pmap->pm_stats, sizeof(pmap->pm_stats));

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
_pmap_alloc_l3(pmap_t pmap, vm_pindex_t ptepindex, struct rwlock **lockp)
{
	vm_page_t m, l1pg, l2pg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, VM_ALLOC_NOOBJ |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if (lockp != NULL) {
			RELEASE_PV_LIST_LOCK(lockp);
			PMAP_UNLOCK(pmap);
			VM_WAIT;
			PMAP_LOCK(pmap);
		}

		/*
		 * Indicate the need to retry.  While waiting, the page table
		 * page may have been allocated.
		 */
		return (NULL);
	}
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	if (ptepindex >= (NUL2E + NUL1E)) {
		pd_entry_t *l0;
		vm_pindex_t l0index;

		l0index = ptepindex - (NUL2E + NUL1E);
		l0 = &pmap->pm_l0[l0index];
		pmap_load_store(l0, VM_PAGE_TO_PHYS(m) | L0_TABLE);
		PTE_SYNC(l0);
	} else if (ptepindex >= NUL2E) {
		vm_pindex_t l0index, l1index;
		pd_entry_t *l0, *l1;
		pd_entry_t tl0;

		l1index = ptepindex - NUL2E;
		l0index = l1index >> L0_ENTRIES_SHIFT;

		l0 = &pmap->pm_l0[l0index];
		tl0 = pmap_load(l0);
		if (tl0 == 0) {
			/* recurse for allocating page dir */
			if (_pmap_alloc_l3(pmap, NUL2E + NUL1E + l0index,
			    lockp) == NULL) {
				--m->wire_count;
				/* XXX: release mem barrier? */
				atomic_subtract_int(&vm_cnt.v_wire_count, 1);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			l1pg = PHYS_TO_VM_PAGE(tl0 & ~ATTR_MASK);
			l1pg->wire_count++;
		}

		l1 = (pd_entry_t *)PHYS_TO_DMAP(pmap_load(l0) & ~ATTR_MASK);
		l1 = &l1[ptepindex & Ln_ADDR_MASK];
		pmap_load_store(l1, VM_PAGE_TO_PHYS(m) | L1_TABLE);
		PTE_SYNC(l1);
	} else {
		vm_pindex_t l0index, l1index;
		pd_entry_t *l0, *l1, *l2;
		pd_entry_t tl0, tl1;

		l1index = ptepindex >> Ln_ENTRIES_SHIFT;
		l0index = l1index >> L0_ENTRIES_SHIFT;

		l0 = &pmap->pm_l0[l0index];
		tl0 = pmap_load(l0);
		if (tl0 == 0) {
			/* recurse for allocating page dir */
			if (_pmap_alloc_l3(pmap, NUL2E + l1index,
			    lockp) == NULL) {
				--m->wire_count;
				atomic_subtract_int(&vm_cnt.v_wire_count, 1);
				vm_page_free_zero(m);
				return (NULL);
			}
			tl0 = pmap_load(l0);
			l1 = (pd_entry_t *)PHYS_TO_DMAP(tl0 & ~ATTR_MASK);
			l1 = &l1[l1index & Ln_ADDR_MASK];
		} else {
			l1 = (pd_entry_t *)PHYS_TO_DMAP(tl0 & ~ATTR_MASK);
			l1 = &l1[l1index & Ln_ADDR_MASK];
			tl1 = pmap_load(l1);
			if (tl1 == 0) {
				/* recurse for allocating page dir */
				if (_pmap_alloc_l3(pmap, NUL2E + l1index,
				    lockp) == NULL) {
					--m->wire_count;
					/* XXX: release mem barrier? */
					atomic_subtract_int(
					    &vm_cnt.v_wire_count, 1);
					vm_page_free_zero(m);
					return (NULL);
				}
			} else {
				l2pg = PHYS_TO_VM_PAGE(tl1 & ~ATTR_MASK);
				l2pg->wire_count++;
			}
		}

		l2 = (pd_entry_t *)PHYS_TO_DMAP(pmap_load(l1) & ~ATTR_MASK);
		l2 = &l2[ptepindex & Ln_ADDR_MASK];
		pmap_load_store(l2, VM_PAGE_TO_PHYS(m) | L2_TABLE);
		PTE_SYNC(l2);
	}

	pmap_resident_count_inc(pmap, 1);

	return (m);
}

static vm_page_t
pmap_alloc_l3(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t ptepindex;
	pd_entry_t *pde, tpde;
#ifdef INVARIANTS
	pt_entry_t *pte;
#endif
	vm_page_t m;
	int lvl;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_l2_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	pde = pmap_pde(pmap, va, &lvl);

	/*
	 * If the page table page is mapped, we just increment the hold count,
	 * and activate it. If we get a level 2 pde it will point to a level 3
	 * table.
	 */
	switch (lvl) {
	case -1:
		break;
	case 0:
#ifdef INVARIANTS
		pte = pmap_l0_to_l1(pde, va);
		KASSERT(pmap_load(pte) == 0,
		    ("pmap_alloc_l3: TODO: l0 superpages"));
#endif
		break;
	case 1:
#ifdef INVARIANTS
		pte = pmap_l1_to_l2(pde, va);
		KASSERT(pmap_load(pte) == 0,
		    ("pmap_alloc_l3: TODO: l1 superpages"));
#endif
		break;
	case 2:
		tpde = pmap_load(pde);
		if (tpde != 0) {
			m = PHYS_TO_VM_PAGE(tpde & ~ATTR_MASK);
			m->wire_count++;
			return (m);
		}
		break;
	default:
		panic("pmap_alloc_l3: Invalid level %d", lvl);
	}

	/*
	 * Here if the pte page isn't mapped, or if it has been deallocated.
	 */
	m = _pmap_alloc_l3(pmap, ptepindex, lockp);
	if (m == NULL && lockp != NULL)
		goto retry;

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
pmap_release(pmap_t pmap)
{
	vm_page_t m;

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
	KASSERT(vm_radix_is_empty(&pmap->pm_root),
	    ("pmap_release: pmap has reserved page table page(s)"));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pmap->pm_l0));

	m->wire_count--;
	atomic_subtract_int(&vm_cnt.v_wire_count, 1);
	vm_page_free_zero(m);
}

static int
kvm_size(SYSCTL_HANDLER_ARGS)
{
	unsigned long ksize = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;

	return sysctl_handle_long(oidp, &ksize, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_size, CTLTYPE_LONG|CTLFLAG_RD,
    0, 0, kvm_size, "LU", "Size of KVM");

static int
kvm_free(SYSCTL_HANDLER_ARGS)
{
	unsigned long kfree = VM_MAX_KERNEL_ADDRESS - kernel_vm_end;

	return sysctl_handle_long(oidp, &kfree, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_free, CTLTYPE_LONG|CTLFLAG_RD,
    0, 0, kvm_free, "LU", "Amount of KVM free");

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_paddr_t paddr;
	vm_page_t nkpg;
	pd_entry_t *l0, *l1, *l2;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);

	addr = roundup2(addr, L2_SIZE);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		l0 = pmap_l0(kernel_pmap, kernel_vm_end);
		KASSERT(pmap_load(l0) != 0,
		    ("pmap_growkernel: No level 0 kernel entry"));

		l1 = pmap_l0_to_l1(l0, kernel_vm_end);
		if (pmap_load(l1) == 0) {
			/* We need a new PDP entry */
			nkpg = vm_page_alloc(NULL, kernel_vm_end >> L1_SHIFT,
			    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ |
			    VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (nkpg == NULL)
				panic("pmap_growkernel: no memory to grow kernel");
			if ((nkpg->flags & PG_ZERO) == 0)
				pmap_zero_page(nkpg);
			paddr = VM_PAGE_TO_PHYS(nkpg);
			pmap_load_store(l1, paddr | L1_TABLE);
			PTE_SYNC(l1);
			continue; /* try again */
		}
		l2 = pmap_l1_to_l2(l1, kernel_vm_end);
		if ((pmap_load(l2) & ATTR_AF) != 0) {
			kernel_vm_end = (kernel_vm_end + L2_SIZE) & ~L2_OFFSET;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
			continue;
		}

		nkpg = vm_page_alloc(NULL, kernel_vm_end >> L2_SHIFT,
		    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		if (nkpg == NULL)
			panic("pmap_growkernel: no memory to grow kernel");
		if ((nkpg->flags & PG_ZERO) == 0)
			pmap_zero_page(nkpg);
		paddr = VM_PAGE_TO_PHYS(nkpg);
		pmap_load_store(l2, paddr | L2_TABLE);
		PTE_SYNC(l2);
		pmap_invalidate_page(kernel_pmap, kernel_vm_end);

		kernel_vm_end = (kernel_vm_end + L2_SIZE) & ~L2_OFFSET;
		if (kernel_vm_end - 1 >= kernel_map->max_offset) {
			kernel_vm_end = kernel_map->max_offset;
			break;
		}
	}
}


/***************************************************
 * page management routines.
 ***************************************************/

CTASSERT(sizeof(struct pv_chunk) == PAGE_SIZE);
CTASSERT(_NPCM == 3);
CTASSERT(_NPCPV == 168);

static __inline struct pv_chunk *
pv_to_chunk(pv_entry_t pv)
{

	return ((struct pv_chunk *)((uintptr_t)pv & ~(uintptr_t)PAGE_MASK));
}

#define PV_PMAP(pv) (pv_to_chunk(pv)->pc_pmap)

#define	PC_FREE0	0xfffffffffffffffful
#define	PC_FREE1	0xfffffffffffffffful
#define	PC_FREE2	0x000000fffffffffful

static const uint64_t pc_freemask[_NPCM] = { PC_FREE0, PC_FREE1, PC_FREE2 };

#if 0
#ifdef PV_STATS
static int pc_chunk_count, pc_chunk_allocs, pc_chunk_frees, pc_chunk_tryfail;

SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_count, CTLFLAG_RD, &pc_chunk_count, 0,
	"Current number of pv entry chunks");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_allocs, CTLFLAG_RD, &pc_chunk_allocs, 0,
	"Current number of pv entry chunks allocated");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_frees, CTLFLAG_RD, &pc_chunk_frees, 0,
	"Current number of pv entry chunks frees");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_tryfail, CTLFLAG_RD, &pc_chunk_tryfail, 0,
	"Number of times tried to get a chunk page but failed.");

static long pv_entry_frees, pv_entry_allocs, pv_entry_count;
static int pv_entry_spare;

SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_frees, CTLFLAG_RD, &pv_entry_frees, 0,
	"Current number of pv entry frees");
SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_allocs, CTLFLAG_RD, &pv_entry_allocs, 0,
	"Current number of pv entry allocs");
SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_count, CTLFLAG_RD, &pv_entry_count, 0,
	"Current number of pv entries");
SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_spare, CTLFLAG_RD, &pv_entry_spare, 0,
	"Current number of spare pv entries");
#endif
#endif /* 0 */

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 *
 * Returns NULL if PV entries were reclaimed from the specified pmap.
 *
 * We do not, however, unmap 2mpages because subsequent accesses will
 * allocate per-page pv entries until repromotion occurs, thereby
 * exacerbating the shortage of free pv entries.
 */
static vm_page_t
reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp)
{

	panic("ARM64TODO: reclaim_pv_chunk");
}

/*
 * free the pv_entry back to the free list
 */
static void
free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int idx, field, bit;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(atomic_add_long(&pv_entry_frees, 1));
	PV_STAT(atomic_add_int(&pv_entry_spare, 1));
	PV_STAT(atomic_subtract_long(&pv_entry_count, 1));
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / 64;
	bit = idx % 64;
	pc->pc_map[field] |= 1ul << bit;
	if (pc->pc_map[0] != PC_FREE0 || pc->pc_map[1] != PC_FREE1 ||
	    pc->pc_map[2] != PC_FREE2) {
		/* 98% of the time, pc is already at the head of the list. */
		if (__predict_false(pc != TAILQ_FIRST(&pmap->pm_pvchunk))) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		}
		return;
	}
	TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
	free_pv_chunk(pc);
}

static void
free_pv_chunk(struct pv_chunk *pc)
{
	vm_page_t m;

	mtx_lock(&pv_chunks_mutex);
 	TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
	mtx_unlock(&pv_chunks_mutex);
	PV_STAT(atomic_subtract_int(&pv_entry_spare, _NPCPV));
	PV_STAT(atomic_subtract_int(&pc_chunk_count, 1));
	PV_STAT(atomic_add_int(&pc_chunk_frees, 1));
	/* entire chunk is free, return it */
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
	dump_drop_page(m->phys_addr);
	vm_page_unwire(m, PQ_NONE);
	vm_page_free(m);
}

/*
 * Returns a new PV entry, allocating a new PV chunk from the system when
 * needed.  If this PV chunk allocation fails and a PV list lock pointer was
 * given, a PV chunk is reclaimed from an arbitrary pmap.  Otherwise, NULL is
 * returned.
 *
 * The given PV list lock may be released.
 */
static pv_entry_t
get_pv_entry(pmap_t pmap, struct rwlock **lockp)
{
	int bit, field;
	pv_entry_t pv;
	struct pv_chunk *pc;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(atomic_add_long(&pv_entry_allocs, 1));
retry:
	pc = TAILQ_FIRST(&pmap->pm_pvchunk);
	if (pc != NULL) {
		for (field = 0; field < _NPCM; field++) {
			if (pc->pc_map[field]) {
				bit = ffsl(pc->pc_map[field]) - 1;
				break;
			}
		}
		if (field < _NPCM) {
			pv = &pc->pc_pventry[field * 64 + bit];
			pc->pc_map[field] &= ~(1ul << bit);
			/* If this was the last item, move it to tail */
			if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0 &&
			    pc->pc_map[2] == 0) {
				TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
				TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc,
				    pc_list);
			}
			PV_STAT(atomic_add_long(&pv_entry_count, 1));
			PV_STAT(atomic_subtract_int(&pv_entry_spare, 1));
			return (pv);
		}
	}
	/* No free items, allocate another chunk */
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ |
	    VM_ALLOC_WIRED);
	if (m == NULL) {
		if (lockp == NULL) {
			PV_STAT(pc_chunk_tryfail++);
			return (NULL);
		}
		m = reclaim_pv_chunk(pmap, lockp);
		if (m == NULL)
			goto retry;
	}
	PV_STAT(atomic_add_int(&pc_chunk_count, 1));
	PV_STAT(atomic_add_int(&pc_chunk_allocs, 1));
	dump_add_page(m->phys_addr);
	pc = (void *)PHYS_TO_DMAP(m->phys_addr);
	pc->pc_pmap = pmap;
	pc->pc_map[0] = PC_FREE0 & ~1ul;	/* preallocated bit 0 */
	pc->pc_map[1] = PC_FREE1;
	pc->pc_map[2] = PC_FREE2;
	mtx_lock(&pv_chunks_mutex);
	TAILQ_INSERT_TAIL(&pv_chunks, pc, pc_lru);
	mtx_unlock(&pv_chunks_mutex);
	pv = &pc->pc_pventry[0];
	TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
	PV_STAT(atomic_add_long(&pv_entry_count, 1));
	PV_STAT(atomic_add_int(&pv_entry_spare, _NPCPV - 1));
	return (pv);
}

/*
 * Ensure that the number of spare PV entries in the specified pmap meets or
 * exceeds the given count, "needed".
 *
 * The given PV list lock may be released.
 */
static void
reserve_pv_entries(pmap_t pmap, int needed, struct rwlock **lockp)
{
	struct pch new_tail;
	struct pv_chunk *pc;
	int avail, free;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reserve_pv_entries: lockp is NULL"));

	/*
	 * Newly allocated PV chunks must be stored in a private list until
	 * the required number of PV chunks have been allocated.  Otherwise,
	 * reclaim_pv_chunk() could recycle one of these chunks.  In
	 * contrast, these chunks must be added to the pmap upon allocation.
	 */
	TAILQ_INIT(&new_tail);
retry:
	avail = 0;
	TAILQ_FOREACH(pc, &pmap->pm_pvchunk, pc_list) {
		bit_count((bitstr_t *)pc->pc_map, 0,
		    sizeof(pc->pc_map) * NBBY, &free);
		if (free == 0)
			break;
		avail += free;
		if (avail >= needed)
			break;
	}
	for (; avail < needed; avail += _NPCPV) {
		m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ |
		    VM_ALLOC_WIRED);
		if (m == NULL) {
			m = reclaim_pv_chunk(pmap, lockp);
			if (m == NULL)
				goto retry;
		}
		PV_STAT(atomic_add_int(&pc_chunk_count, 1));
		PV_STAT(atomic_add_int(&pc_chunk_allocs, 1));
		dump_add_page(m->phys_addr);
		pc = (void *)PHYS_TO_DMAP(m->phys_addr);
		pc->pc_pmap = pmap;
		pc->pc_map[0] = PC_FREE0;
		pc->pc_map[1] = PC_FREE1;
		pc->pc_map[2] = PC_FREE2;
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
		PV_STAT(atomic_add_int(&pv_entry_spare, _NPCPV));
	}
	if (!TAILQ_EMPTY(&new_tail)) {
		mtx_lock(&pv_chunks_mutex);
		TAILQ_CONCAT(&pv_chunks, &new_tail, pc_lru);
		mtx_unlock(&pv_chunks_mutex);
	}
}

/*
 * First find and then remove the pv entry for the specified pmap and virtual
 * address from the specified pv list.  Returns the pv entry if found and NULL
 * otherwise.  This operation can be performed on pv lists for either 4KB or
 * 2MB page mappings.
 */
static __inline pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		if (pmap == PV_PMAP(pv) && va == pv->pv_va) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			pvh->pv_gen++;
			break;
		}
	}
	return (pv);
}

/*
 * After demotion from a 2MB page mapping to 512 4KB page mappings,
 * destroy the pv entry for the 2MB page mapping and reinstantiate the pv
 * entries for each of the 4KB page mappings.
 */
static void
pmap_pv_demote_l2(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	struct pv_chunk *pc;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;
	int bit, field;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((pa & L2_OFFSET) == 0,
	    ("pmap_pv_demote_l2: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the 2mpage's pv entry for this mapping to the first
	 * page's pv list.  Once this transfer begins, the pv list lock
	 * must not be released until the last pv entry is reinstantiated.
	 */
	pvh = pa_to_pvh(pa);
	va = va & ~L2_OFFSET;
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_l2: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
	m->md.pv_gen++;
	/* Instantiate the remaining Ln_ENTRIES - 1 pv entries. */
	PV_STAT(atomic_add_long(&pv_entry_allocs, Ln_ENTRIES - 1));
	va_last = va + L2_SIZE - PAGE_SIZE;
	for (;;) {
		pc = TAILQ_FIRST(&pmap->pm_pvchunk);
		KASSERT(pc->pc_map[0] != 0 || pc->pc_map[1] != 0 ||
		    pc->pc_map[2] != 0, ("pmap_pv_demote_l2: missing spare"));
		for (field = 0; field < _NPCM; field++) {
			while (pc->pc_map[field]) {
				bit = ffsl(pc->pc_map[field]) - 1;
				pc->pc_map[field] &= ~(1ul << bit);
				pv = &pc->pc_pventry[field * 64 + bit];
				va += PAGE_SIZE;
				pv->pv_va = va;
				m++;
				KASSERT((m->oflags & VPO_UNMANAGED) == 0,
			    ("pmap_pv_demote_l2: page %p is not managed", m));
				TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
				m->md.pv_gen++;
				if (va == va_last)
					goto out;
			}
		}
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
out:
	if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0 && pc->pc_map[2] == 0) {
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
	PV_STAT(atomic_add_long(&pv_entry_count, Ln_ENTRIES - 1));
	PV_STAT(atomic_subtract_int(&pv_entry_spare, Ln_ENTRIES - 1));
}

/*
 * First find and then destroy the pv entry for the specified pmap and virtual
 * address.  This operation can be performed on pv lists for either 4KB or 2MB
 * page mappings.
 */
static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found"));
	free_pv_entry(pmap, pv);
}

/*
 * Conditionally create the PV entry for a 4KB page mapping if the required
 * memory can be allocated without resorting to reclamation.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct rwlock **lockp)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, NULL)) != NULL) {
		pv->pv_va = va;
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * pmap_remove_l3: do the things to unmap a page in a process
 */
static int
pmap_remove_l3(pmap_t pmap, pt_entry_t *l3, vm_offset_t va,
    pd_entry_t l2e, struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pt_entry_t old_l3;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pmap_is_current(pmap) && pmap_l3_valid_cacheable(pmap_load(l3)))
		cpu_dcache_wb_range(va, L3_SIZE);
	old_l3 = pmap_load_clear(l3);
	PTE_SYNC(l3);
	pmap_invalidate_page(pmap, va);
	if (old_l3 & ATTR_SW_WIRED)
		pmap->pm_stats.wired_count -= 1;
	pmap_resident_count_dec(pmap, 1);
	if (old_l3 & ATTR_SW_MANAGED) {
		m = PHYS_TO_VM_PAGE(old_l3 & ~ATTR_MASK);
		if (pmap_page_dirty(old_l3))
			vm_page_dirty(m);
		if (old_l3 & ATTR_AF)
			vm_page_aflag_set(m, PGA_REFERENCED);
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		pmap_pvh_free(&m->md, pmap, va);
		if (TAILQ_EMPTY(&m->md.pv_list) &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
			if (TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
	}
	return (pmap_unuse_l3(pmap, va, l2e, free));
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct rwlock *lock;
	vm_offset_t va, va_next;
	pd_entry_t *l0, *l1, *l2;
	pt_entry_t l3_paddr, *l3;
	struct spglist free;
	int anyvalid;

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = 0;
	SLIST_INIT(&free);

	PMAP_LOCK(pmap);

	lock = NULL;
	for (; sva < eva; sva = va_next) {

		if (pmap->pm_stats.resident_count == 0)
			break;

		l0 = pmap_l0(pmap, sva);
		if (pmap_load(l0) == 0) {
			va_next = (sva + L0_SIZE) & ~L0_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		l1 = pmap_l0_to_l1(l0, sva);
		if (pmap_load(l1) == 0) {
			va_next = (sva + L1_SIZE) & ~L1_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		/*
		 * Calculate index for next page table.
		 */
		va_next = (sva + L2_SIZE) & ~L2_OFFSET;
		if (va_next < sva)
			va_next = eva;

		l2 = pmap_l1_to_l2(l1, sva);
		if (l2 == NULL)
			continue;

		l3_paddr = pmap_load(l2);

		if ((l3_paddr & ATTR_DESCR_MASK) == L2_BLOCK) {
			/* TODO: Add pmap_remove_l2 */
			if (pmap_demote_l2_locked(pmap, l2, sva & ~L2_OFFSET,
			    &lock) == NULL)
				continue;
			l3_paddr = pmap_load(l2);
		}

		/*
		 * Weed out invalid mappings.
		 */
		if ((l3_paddr & ATTR_DESCR_MASK) != L2_TABLE)
			continue;

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (l3 = pmap_l2_to_l3(l2, sva); sva != va_next; l3++,
		    sva += L3_SIZE) {
			if (l3 == NULL)
				panic("l3 == NULL");
			if (pmap_load(l3) == 0) {
				if (va != va_next) {
					pmap_invalidate_range(pmap, va, sva);
					va = va_next;
				}
				continue;
			}
			if (va == va_next)
				va = sva;
			if (pmap_remove_l3(pmap, l3, sva, l3_paddr, &free,
			    &lock)) {
				sva += L3_SIZE;
				break;
			}
		}
		if (va != va_next)
			pmap_invalidate_range(pmap, va, sva);
	}
	if (lock != NULL)
		rw_wunlock(lock);
	if (anyvalid)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(&free);
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 *
 *	Notes:
 *		Original versions of this routine were very
 *		inefficient because they iteratively called
 *		pmap_remove (slow...)
 */

void
pmap_remove_all(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t *pde, tpde;
	pt_entry_t *pte, tpte;
	vm_offset_t va;
	struct spglist free;
	int lvl, pvh_gen, md_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
retry:
	rw_wlock(lock);
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				rw_wunlock(lock);
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		va = pv->pv_va;
		pte = pmap_pte(pmap, va, &lvl);
		KASSERT(pte != NULL,
		    ("pmap_remove_all: no page table entry found"));
		KASSERT(lvl == 2,
		    ("pmap_remove_all: invalid pte level %d", lvl));

		pmap_demote_l2_locked(pmap, pte, va, &lock);
		PMAP_UNLOCK(pmap);
	}
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				rw_wunlock(lock);
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		pmap_resident_count_dec(pmap, 1);

		pde = pmap_pde(pmap, pv->pv_va, &lvl);
		KASSERT(pde != NULL,
		    ("pmap_remove_all: no page directory entry found"));
		KASSERT(lvl == 2,
		    ("pmap_remove_all: invalid pde level %d", lvl));
		tpde = pmap_load(pde);

		pte = pmap_l2_to_l3(pde, pv->pv_va);
		tpte = pmap_load(pte);
		if (pmap_is_current(pmap) &&
		    pmap_l3_valid_cacheable(tpte))
			cpu_dcache_wb_range(pv->pv_va, L3_SIZE);
		pmap_load_clear(pte);
		PTE_SYNC(pte);
		pmap_invalidate_page(pmap, pv->pv_va);
		if (tpte & ATTR_SW_WIRED)
			pmap->pm_stats.wired_count--;
		if ((tpte & ATTR_AF) != 0)
			vm_page_aflag_set(m, PGA_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (pmap_page_dirty(tpte))
			vm_page_dirty(m);
		pmap_unuse_l3(pmap, pv->pv_va, tpde, &free);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(lock);
	pmap_free_zero_pages(&free);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va, va_next;
	pd_entry_t *l0, *l1, *l2;
	pt_entry_t *l3p, l3;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if ((prot & VM_PROT_WRITE) == VM_PROT_WRITE)
		return;

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {

		l0 = pmap_l0(pmap, sva);
		if (pmap_load(l0) == 0) {
			va_next = (sva + L0_SIZE) & ~L0_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		l1 = pmap_l0_to_l1(l0, sva);
		if (pmap_load(l1) == 0) {
			va_next = (sva + L1_SIZE) & ~L1_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + L2_SIZE) & ~L2_OFFSET;
		if (va_next < sva)
			va_next = eva;

		l2 = pmap_l1_to_l2(l1, sva);
		if (pmap_load(l2) == 0)
			continue;

		if ((pmap_load(l2) & ATTR_DESCR_MASK) == L2_BLOCK) {
			l3p = pmap_demote_l2(pmap, l2, sva);
			if (l3p == NULL)
				continue;
		}
		KASSERT((pmap_load(l2) & ATTR_DESCR_MASK) == L2_TABLE,
		    ("pmap_protect: Invalid L2 entry after demotion"));

		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (l3p = pmap_l2_to_l3(l2, sva); sva != va_next; l3p++,
		    sva += L3_SIZE) {
			l3 = pmap_load(l3p);
			if (pmap_l3_valid(l3)) {
				pmap_set(l3p, ATTR_AP(ATTR_AP_RO));
				PTE_SYNC(l3p);
				/* XXX: Use pmap_invalidate_range */
				pmap_invalidate_page(pmap, va);
			}
		}
	}
	PMAP_UNLOCK(pmap);

	/* TODO: Only invalidate entries we are touching */
	pmap_invalidate_all(pmap);
}

/*
 * Inserts the specified page table page into the specified pmap's collection
 * of idle page table pages.  Each of a pmap's page table pages is responsible
 * for mapping a distinct range of virtual addresses.  The pmap's collection is
 * ordered by this virtual address range.
 */
static __inline int
pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_insert(&pmap->pm_root, mpte));
}

/*
 * Looks for a page table page mapping the specified virtual address in the
 * specified pmap's collection of idle page table pages.  Returns NULL if there
 * is no page table page corresponding to the specified virtual address.
 */
static __inline vm_page_t
pmap_lookup_pt_page(pmap_t pmap, vm_offset_t va)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_lookup(&pmap->pm_root, pmap_l2_pindex(va)));
}

/*
 * Removes the specified page table page from the specified pmap's collection
 * of idle page table pages.  The specified page table page must be a member of
 * the pmap's collection.
 */
static __inline void
pmap_remove_pt_page(pmap_t pmap, vm_page_t mpte)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	vm_radix_remove(&pmap->pm_root, mpte->pindex);
}

/*
 * Performs a break-before-make update of a pmap entry. This is needed when
 * either promoting or demoting pages to ensure the TLB doesn't get into an
 * inconsistent state.
 */
static void
pmap_update_entry(pmap_t pmap, pd_entry_t *pte, pd_entry_t newpte,
    vm_offset_t va, vm_size_t size)
{
	register_t intr;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Ensure we don't get switched out with the page table in an
	 * inconsistent state. We also need to ensure no interrupts fire
	 * as they may make use of an address we are about to invalidate.
	 */
	intr = intr_disable();
	critical_enter();

	/* Clear the old mapping */
	pmap_load_clear(pte);
	PTE_SYNC(pte);
	pmap_invalidate_range(pmap, va, va + size);

	/* Create the new mapping */
	pmap_load_store(pte, newpte);
	PTE_SYNC(pte);

	critical_exit();
	intr_restore(intr);
}

/*
 * After promotion from 512 4KB page mappings to a single 2MB page mapping,
 * replace the many pv entries for the 4KB page mappings by a single pv entry
 * for the 2MB page mapping.
 */
static void
pmap_pv_promote_l2(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	KASSERT((pa & L2_OFFSET) == 0,
	    ("pmap_pv_promote_l2: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the first page's pv entry for this mapping to the 2mpage's
	 * pv list.  Aside from avoiding the cost of a call to get_pv_entry(),
	 * a transfer avoids the possibility that get_pv_entry() calls
	 * reclaim_pv_chunk() and that reclaim_pv_chunk() removes one of the
	 * mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = va & ~L2_OFFSET;
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_promote_l2: pv not found"));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
	pvh->pv_gen++;
	/* Free the remaining NPTEPG - 1 pv entries. */
	va_last = va + L2_SIZE - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
}

/*
 * Tries to promote the 512, contiguous 4KB page mappings that are within a
 * single level 2 table entry to a single 2MB page mapping.  For promotion
 * to occur, two conditions must be met: (1) the 4KB page mappings must map
 * aligned, contiguous physical memory and (2) the 4KB page mappings must have
 * identical characteristics.
 */
static void
pmap_promote_l2(pmap_t pmap, pd_entry_t *l2, vm_offset_t va,
    struct rwlock **lockp)
{
	pt_entry_t *firstl3, *l3, newl2, oldl3, pa;
	vm_page_t mpte;
	vm_offset_t sva;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	sva = va & ~L2_OFFSET;
	firstl3 = pmap_l2_to_l3(l2, sva);
	newl2 = pmap_load(firstl3);

	/* Check the alingment is valid */
	if (((newl2 & ~ATTR_MASK) & L2_OFFSET) != 0) {
		atomic_add_long(&pmap_l2_p_failures, 1);
		CTR2(KTR_PMAP, "pmap_promote_l2: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return;
	}

	pa = newl2 + L2_SIZE - PAGE_SIZE;
	for (l3 = firstl3 + NL3PG - 1; l3 > firstl3; l3--) {
		oldl3 = pmap_load(l3);
		if (oldl3 != pa) {
			atomic_add_long(&pmap_l2_p_failures, 1);
			CTR2(KTR_PMAP, "pmap_promote_l2: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return;
		}
		pa -= PAGE_SIZE;
	}

	/*
	 * Save the page table page in its current state until the L2
	 * mapping the superpage is demoted by pmap_demote_l2() or
	 * destroyed by pmap_remove_l3().
	 */
	mpte = PHYS_TO_VM_PAGE(pmap_load(l2) & ~ATTR_MASK);
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("pmap_promote_l2: page table page is out of range"));
	KASSERT(mpte->pindex == pmap_l2_pindex(va),
	    ("pmap_promote_l2: page table page's pindex is wrong"));
	if (pmap_insert_pt_page(pmap, mpte)) {
		atomic_add_long(&pmap_l2_p_failures, 1);
		CTR2(KTR_PMAP,
		    "pmap_promote_l2: failure for va %#lx in pmap %p", va,
		    pmap);
		return;
	}

	if ((newl2 & ATTR_SW_MANAGED) != 0)
		pmap_pv_promote_l2(pmap, va, newl2 & ~ATTR_MASK, lockp);

	newl2 &= ~ATTR_DESCR_MASK;
	newl2 |= L2_BLOCK;

	pmap_update_entry(pmap, l2, newl2, sva, L2_SIZE);

	atomic_add_long(&pmap_l2_promotions, 1);
	CTR2(KTR_PMAP, "pmap_promote_l2: success for va %#lx in pmap %p", va,
		    pmap);
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    u_int flags, int8_t psind __unused)
{
	struct rwlock *lock;
	pd_entry_t *pde;
	pt_entry_t new_l3, orig_l3;
	pt_entry_t *l2, *l3;
	pv_entry_t pv;
	vm_paddr_t opa, pa, l1_pa, l2_pa, l3_pa;
	vm_page_t mpte, om, l1_m, l2_m, l3_m;
	boolean_t nosleep;
	int lvl;

	va = trunc_page(va);
	if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
		VM_OBJECT_ASSERT_LOCKED(m->object);
	pa = VM_PAGE_TO_PHYS(m);
	new_l3 = (pt_entry_t)(pa | ATTR_DEFAULT | ATTR_IDX(m->md.pv_memattr) |
	    L3_PAGE);
	if ((prot & VM_PROT_WRITE) == 0)
		new_l3 |= ATTR_AP(ATTR_AP_RO);
	if ((flags & PMAP_ENTER_WIRED) != 0)
		new_l3 |= ATTR_SW_WIRED;
	if ((va >> 63) == 0)
		new_l3 |= ATTR_AP(ATTR_AP_USER);

	CTR2(KTR_PMAP, "pmap_enter: %.16lx -> %.16lx", va, pa);

	mpte = NULL;

	lock = NULL;
	PMAP_LOCK(pmap);

	pde = pmap_pde(pmap, va, &lvl);
	if (pde != NULL && lvl == 1) {
		l2 = pmap_l1_to_l2(pde, va);
		if ((pmap_load(l2) & ATTR_DESCR_MASK) == L2_BLOCK &&
		    (l3 = pmap_demote_l2_locked(pmap, l2, va & ~L2_OFFSET,
		    &lock)) != NULL) {
			l3 = &l3[pmap_l3_index(va)];
			if (va < VM_MAXUSER_ADDRESS) {
				mpte = PHYS_TO_VM_PAGE(
				    pmap_load(l2) & ~ATTR_MASK);
				mpte->wire_count++;
			}
			goto havel3;
		}
	}

	if (va < VM_MAXUSER_ADDRESS) {
		nosleep = (flags & PMAP_ENTER_NOSLEEP) != 0;
		mpte = pmap_alloc_l3(pmap, va, nosleep ? NULL : &lock);
		if (mpte == NULL && nosleep) {
			CTR0(KTR_PMAP, "pmap_enter: mpte == NULL");
			if (lock != NULL)
				rw_wunlock(lock);
			PMAP_UNLOCK(pmap);
			return (KERN_RESOURCE_SHORTAGE);
		}
		pde = pmap_pde(pmap, va, &lvl);
		KASSERT(pde != NULL,
		    ("pmap_enter: Invalid page entry, va: 0x%lx", va));
		KASSERT(lvl == 2,
		    ("pmap_enter: Invalid level %d", lvl));

		l3 = pmap_l2_to_l3(pde, va);
	} else {
		/*
		 * If we get a level 2 pde it must point to a level 3 entry
		 * otherwise we will need to create the intermediate tables
		 */
		if (lvl < 2) {
			switch(lvl) {
			default:
			case -1:
				/* Get the l0 pde to update */
				pde = pmap_l0(pmap, va);
				KASSERT(pde != NULL, ("..."));

				l1_m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
				    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
				    VM_ALLOC_ZERO);
				if (l1_m == NULL)
					panic("pmap_enter: l1 pte_m == NULL");
				if ((l1_m->flags & PG_ZERO) == 0)
					pmap_zero_page(l1_m);

				l1_pa = VM_PAGE_TO_PHYS(l1_m);
				pmap_load_store(pde, l1_pa | L0_TABLE);
				PTE_SYNC(pde);
				/* FALLTHROUGH */
			case 0:
				/* Get the l1 pde to update */
				pde = pmap_l1_to_l2(pde, va);
				KASSERT(pde != NULL, ("..."));

				l2_m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
				    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
				    VM_ALLOC_ZERO);
				if (l2_m == NULL)
					panic("pmap_enter: l2 pte_m == NULL");
				if ((l2_m->flags & PG_ZERO) == 0)
					pmap_zero_page(l2_m);

				l2_pa = VM_PAGE_TO_PHYS(l2_m);
				pmap_load_store(pde, l2_pa | L1_TABLE);
				PTE_SYNC(pde);
				/* FALLTHROUGH */
			case 1:
				/* Get the l2 pde to update */
				pde = pmap_l1_to_l2(pde, va);

				l3_m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
				    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
				    VM_ALLOC_ZERO);
				if (l3_m == NULL)
					panic("pmap_enter: l3 pte_m == NULL");
				if ((l3_m->flags & PG_ZERO) == 0)
					pmap_zero_page(l3_m);

				l3_pa = VM_PAGE_TO_PHYS(l3_m);
				pmap_load_store(pde, l3_pa | L2_TABLE);
				PTE_SYNC(pde);
				break;
			}
		}
		l3 = pmap_l2_to_l3(pde, va);
		pmap_invalidate_page(pmap, va);
	}
havel3:

	om = NULL;
	orig_l3 = pmap_load(l3);
	opa = orig_l3 & ~ATTR_MASK;

	/*
	 * Is the specified virtual address already mapped?
	 */
	if (pmap_l3_valid(orig_l3)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if ((flags & PMAP_ENTER_WIRED) != 0 &&
		    (orig_l3 & ATTR_SW_WIRED) == 0)
			pmap->pm_stats.wired_count++;
		else if ((flags & PMAP_ENTER_WIRED) == 0 &&
		    (orig_l3 & ATTR_SW_WIRED) != 0)
			pmap->pm_stats.wired_count--;

		/*
		 * Remove the extra PT page reference.
		 */
		if (mpte != NULL) {
			mpte->wire_count--;
			KASSERT(mpte->wire_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			     " va: 0x%lx", va));
		}

		/*
		 * Has the physical page changed?
		 */
		if (opa == pa) {
			/*
			 * No, might be a protection or wiring change.
			 */
			if ((orig_l3 & ATTR_SW_MANAGED) != 0) {
				new_l3 |= ATTR_SW_MANAGED;
				if ((new_l3 & ATTR_AP(ATTR_AP_RW)) ==
				    ATTR_AP(ATTR_AP_RW)) {
					vm_page_aflag_set(m, PGA_WRITEABLE);
				}
			}
			goto validate;
		}

		/* Flush the cache, there might be uncommitted data in it */
		if (pmap_is_current(pmap) && pmap_l3_valid_cacheable(orig_l3))
			cpu_dcache_wb_range(va, L3_SIZE);
	} else {
		/*
		 * Increment the counters.
		 */
		if ((new_l3 & ATTR_SW_WIRED) != 0)
			pmap->pm_stats.wired_count++;
		pmap_resident_count_inc(pmap, 1);
	}
	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		new_l3 |= ATTR_SW_MANAGED;
		pv = get_pv_entry(pmap, &lock);
		pv->pv_va = va;
		CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, pa);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		if ((new_l3 & ATTR_AP_RW_BIT) == ATTR_AP(ATTR_AP_RW))
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}

	/*
	 * Update the L3 entry.
	 */
	if (orig_l3 != 0) {
validate:
		orig_l3 = pmap_load(l3);
		opa = orig_l3 & ~ATTR_MASK;

		if (opa != pa) {
			pmap_update_entry(pmap, l3, new_l3, va, PAGE_SIZE);
			if ((orig_l3 & ATTR_SW_MANAGED) != 0) {
				om = PHYS_TO_VM_PAGE(opa);
				if (pmap_page_dirty(orig_l3))
					vm_page_dirty(om);
				if ((orig_l3 & ATTR_AF) != 0)
					vm_page_aflag_set(om, PGA_REFERENCED);
				CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, opa);
				pmap_pvh_free(&om->md, pmap, va);
				if ((om->aflags & PGA_WRITEABLE) != 0 &&
				    TAILQ_EMPTY(&om->md.pv_list) &&
				    ((om->flags & PG_FICTITIOUS) != 0 ||
				    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
					vm_page_aflag_clear(om, PGA_WRITEABLE);
			}
		} else {
			pmap_load_store(l3, new_l3);
			PTE_SYNC(l3);
			pmap_invalidate_page(pmap, va);
			if (pmap_page_dirty(orig_l3) &&
			    (orig_l3 & ATTR_SW_MANAGED) != 0)
				vm_page_dirty(m);
		}
	} else {
		pmap_load_store(l3, new_l3);
	}

	PTE_SYNC(l3);
	pmap_invalidate_page(pmap, va);

	if (pmap != pmap_kernel()) {
		if (pmap == &curproc->p_vmspace->vm_pmap)
		    cpu_icache_sync_range(va, PAGE_SIZE);

		if ((mpte == NULL || mpte->wire_count == NL3PG) &&
		    pmap_superpages_enabled() &&
		    (m->flags & PG_FICTITIOUS) == 0 &&
		    vm_reserv_level_iffullpop(m) == 0) {
			pmap_promote_l2(pmap, pde, va, &lock);
		}
	}

	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
	return (KERN_SUCCESS);
}

/*
 * Maps a sequence of resident pages belonging to the same object.
 * The sequence begins with the given page m_start.  This page is
 * mapped at the given virtual address start.  Each subsequent page is
 * mapped at a virtual address that is offset from start by the same
 * amount as the page is offset from m_start within the object.  The
 * last page in the sequence is the page with the largest offset from
 * m_start that can be mapped at a virtual address less than the given
 * virtual address end.  Not every virtual page between start and end
 * is mapped; only those for which a resident page exists with the
 * corresponding offset from m_start are mapped.
 */
void
pmap_enter_object(pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	struct rwlock *lock;
	vm_offset_t va;
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	lock = NULL;
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		mpte = pmap_enter_quick_locked(pmap, va, m, prot, mpte, &lock);
		m = TAILQ_NEXT(m, listq);
	}
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * but is *MUCH* faster than pmap_enter...
 */

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	struct rwlock *lock;

	lock = NULL;
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(pmap, va, m, prot, NULL, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
}

static vm_page_t
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp)
{
	struct spglist free;
	pd_entry_t *pde;
	pt_entry_t *l2, *l3;
	vm_paddr_t pa;
	int lvl;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	CTR2(KTR_PMAP, "pmap_enter_quick_locked: %p %lx", pmap, va);
	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		vm_pindex_t l2pindex;

		/*
		 * Calculate pagetable page index
		 */
		l2pindex = pmap_l2_pindex(va);
		if (mpte && (mpte->pindex == l2pindex)) {
			mpte->wire_count++;
		} else {
			/*
			 * Get the l2 entry
			 */
			pde = pmap_pde(pmap, va, &lvl);

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.  Otherwise, we
			 * attempt to allocate a page table page.  If this
			 * attempt fails, we don't retry.  Instead, we give up.
			 */
			if (lvl == 1) {
				l2 = pmap_l1_to_l2(pde, va);
				if ((pmap_load(l2) & ATTR_DESCR_MASK) ==
				    L2_BLOCK)
					return (NULL);
			}
			if (lvl == 2 && pmap_load(pde) != 0) {
				mpte =
				    PHYS_TO_VM_PAGE(pmap_load(pde) & ~ATTR_MASK);
				mpte->wire_count++;
			} else {
				/*
				 * Pass NULL instead of the PV list lock
				 * pointer, because we don't intend to sleep.
				 */
				mpte = _pmap_alloc_l3(pmap, l2pindex, NULL);
				if (mpte == NULL)
					return (mpte);
			}
		}
		l3 = (pt_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mpte));
		l3 = &l3[pmap_l3_index(va)];
	} else {
		mpte = NULL;
		pde = pmap_pde(kernel_pmap, va, &lvl);
		KASSERT(pde != NULL,
		    ("pmap_enter_quick_locked: Invalid page entry, va: 0x%lx",
		     va));
		KASSERT(lvl == 2,
		    ("pmap_enter_quick_locked: Invalid level %d", lvl));
		l3 = pmap_l2_to_l3(pde, va);
	}

	if (pmap_load(l3) != 0) {
		if (mpte != NULL) {
			mpte->wire_count--;
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0 &&
	    !pmap_try_insert_pv_entry(pmap, va, m, lockp)) {
		if (mpte != NULL) {
			SLIST_INIT(&free);
			if (pmap_unwire_l3(pmap, va, mpte, &free)) {
				pmap_invalidate_page(pmap, va);
				pmap_free_zero_pages(&free);
			}
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Increment counters
	 */
	pmap_resident_count_inc(pmap, 1);

	pa = VM_PAGE_TO_PHYS(m) | ATTR_DEFAULT | ATTR_IDX(m->md.pv_memattr) |
	    ATTR_AP(ATTR_AP_RO) | L3_PAGE;

	/*
	 * Now validate mapping with RO protection
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0)
		pa |= ATTR_SW_MANAGED;
	pmap_load_store(l3, pa);
	PTE_SYNC(l3);
	pmap_invalidate_page(pmap, va);
	return (mpte);
}

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));
}

/*
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	The wired attribute of the page table entry is not a hardware feature,
 *	so there is no need to invalidate any TLB entries.
 */
void
pmap_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t va_next;
	pd_entry_t *l0, *l1, *l2;
	pt_entry_t *l3;

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		l0 = pmap_l0(pmap, sva);
		if (pmap_load(l0) == 0) {
			va_next = (sva + L0_SIZE) & ~L0_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		l1 = pmap_l0_to_l1(l0, sva);
		if (pmap_load(l1) == 0) {
			va_next = (sva + L1_SIZE) & ~L1_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + L2_SIZE) & ~L2_OFFSET;
		if (va_next < sva)
			va_next = eva;

		l2 = pmap_l1_to_l2(l1, sva);
		if (pmap_load(l2) == 0)
			continue;

		if ((pmap_load(l2) & ATTR_DESCR_MASK) == L2_BLOCK) {
			l3 = pmap_demote_l2(pmap, l2, sva);
			if (l3 == NULL)
				continue;
		}
		KASSERT((pmap_load(l2) & ATTR_DESCR_MASK) == L2_TABLE,
		    ("pmap_unwire: Invalid l2 entry after demotion"));

		if (va_next > eva)
			va_next = eva;
		for (l3 = pmap_l2_to_l3(l2, sva); sva != va_next; l3++,
		    sva += L3_SIZE) {
			if (pmap_load(l3) == 0)
				continue;
			if ((pmap_load(l3) & ATTR_SW_WIRED) == 0)
				panic("pmap_unwire: l3 %#jx is missing "
				    "ATTR_SW_WIRED", (uintmax_t)pmap_load(l3));

			/*
			 * PG_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PG_W, another processor
			 * could be setting PG_M and/or PG_A concurrently.
			 */
			atomic_clear_long(l3, ATTR_SW_WIRED);
			pmap->pm_stats.wired_count--;
		}
	}
	PMAP_UNLOCK(pmap);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr, vm_size_t len,
    vm_offset_t src_addr)
{
}

/*
 *	pmap_zero_page zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	pagezero((void *)va);
}

/*
 *	pmap_zero_page_area zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.
 *
 *	off and size may not cover an area beyond a single hardware page.
 */
void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	if (off == 0 && size == PAGE_SIZE)
		pagezero((void *)va);
	else
		bzero((char *)va + off, size);
}

/*
 *	pmap_zero_page_idle zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.  This
 *	is intended to be called from the vm_pagezero process only and
 *	outside of Giant.
 */
void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	pagezero((void *)va);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t src = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mdst));

	pagecopy((void *)src, (void *)dst);
}

int unmapped_buf_allowed = 1;

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
	void *a_cp, *b_cp;
	vm_page_t m_a, m_b;
	vm_paddr_t p_a, p_b;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;

	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		m_a = ma[a_offset >> PAGE_SHIFT];
		p_a = m_a->phys_addr;
		b_pg_offset = b_offset & PAGE_MASK;
		m_b = mb[b_offset >> PAGE_SHIFT];
		p_b = m_b->phys_addr;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		if (__predict_false(!PHYS_IN_DMAP(p_a))) {
			panic("!DMAP a %lx", p_a);
		} else {
			a_cp = (char *)PHYS_TO_DMAP(p_a) + a_pg_offset;
		}
		if (__predict_false(!PHYS_IN_DMAP(p_b))) {
			panic("!DMAP b %lx", p_b);
		} else {
			b_cp = (char *)PHYS_TO_DMAP(p_b) + b_pg_offset;
		}
		bcopy(a_cp, b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
}

vm_offset_t
pmap_quick_enter_page(vm_page_t m)
{

	return (PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)));
}

void
pmap_quick_remove_page(vm_offset_t addr)
{
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	struct md_page *pvh;
	struct rwlock *lock;
	pv_entry_t pv;
	int loops = 0;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	rv = FALSE;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		if (PV_PMAP(pv) == pmap) {
			rv = TRUE;
			break;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	if (!rv && loops < 16 && (m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			if (PV_PMAP(pv) == pmap) {
				rv = TRUE;
				break;
			}
			loops++;
			if (loops >= 16)
				break;
		}
	}
	rw_runlock(lock);
	return (rv);
}

/*
 *	pmap_page_wired_mappings:
 *
 *	Return the number of managed mappings to the given physical page
 *	that are wired.
 */
int
pmap_page_wired_mappings(vm_page_t m)
{
	struct rwlock *lock;
	struct md_page *pvh;
	pmap_t pmap;
	pt_entry_t *pte;
	pv_entry_t pv;
	int count, lvl, md_gen, pvh_gen;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (0);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	count = 0;
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va, &lvl);
		if (pte != NULL && (pmap_load(pte) & ATTR_SW_WIRED) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pte(pmap, pv->pv_va, &lvl);
			if (pte != NULL &&
			    (pmap_load(pte) & ATTR_SW_WIRED) != 0)
				count++;
			PMAP_UNLOCK(pmap);
		}
	}
	rw_runlock(lock);
	return (count);
}

/*
 * Destroy all managed, non-wired mappings in the given user-space
 * pmap.  This pmap cannot be active on any processor besides the
 * caller.
 *
 * This function cannot be applied to the kernel pmap.  Moreover, it
 * is not intended for general use.  It is only to be used during
 * process termination.  Consequently, it can be implemented in ways
 * that make it faster than pmap_remove().  First, it can more quickly
 * destroy mappings by iterating over the pmap's collection of PV
 * entries, rather than searching the page table.  Second, it doesn't
 * have to test and clear the page table entries atomically, because
 * no processor is currently accessing the user address space.  In
 * particular, a page table entry's dirty bit won't change state once
 * this function starts.
 */
void
pmap_remove_pages(pmap_t pmap)
{
	pd_entry_t *pde;
	pt_entry_t *pte, tpte;
	struct spglist free;
	vm_page_t m, ml3, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct rwlock *lock;
	int64_t bit;
	uint64_t inuse, bitmask;
	int allfree, field, freed, idx, lvl;
	vm_paddr_t pa;

	lock = NULL;

	SLIST_INIT(&free);
	PMAP_LOCK(pmap);
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = ffsl(inuse) - 1;
				bitmask = 1UL << bit;
				idx = field * 64 + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pde = pmap_pde(pmap, pv->pv_va, &lvl);
				KASSERT(pde != NULL,
				    ("Attempting to remove an unmapped page"));

				switch(lvl) {
				case 1:
					pte = pmap_l1_to_l2(pde, pv->pv_va);
					tpte = pmap_load(pte); 
					KASSERT((tpte & ATTR_DESCR_MASK) ==
					    L2_BLOCK,
					    ("Attempting to remove an invalid "
					    "block: %lx", tpte));
					tpte = pmap_load(pte);
					break;
				case 2:
					pte = pmap_l2_to_l3(pde, pv->pv_va);
					tpte = pmap_load(pte);
					KASSERT((tpte & ATTR_DESCR_MASK) ==
					    L3_PAGE,
					    ("Attempting to remove an invalid "
					     "page: %lx", tpte));
					break;
				default:
					panic(
					    "Invalid page directory level: %d",
					    lvl);
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (tpte & ATTR_SW_WIRED) {
					allfree = 0;
					continue;
				}

				pa = tpte & ~ATTR_MASK;

				m = PHYS_TO_VM_PAGE(pa);
				KASSERT(m->phys_addr == pa,
				    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
				    m, (uintmax_t)m->phys_addr,
				    (uintmax_t)tpte));

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("pmap_remove_pages: bad pte %#jx",
				    (uintmax_t)tpte));

				if (pmap_is_current(pmap)) {
					if (lvl == 2 &&
					    pmap_l3_valid_cacheable(tpte)) {
						cpu_dcache_wb_range(pv->pv_va,
						    L3_SIZE);
					} else if (lvl == 1 &&
					    pmap_pte_valid_cacheable(tpte)) {
						cpu_dcache_wb_range(pv->pv_va,
						    L2_SIZE);
					}
				}
				pmap_load_clear(pte);
				PTE_SYNC(pte);
				pmap_invalidate_page(pmap, pv->pv_va);

				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if ((tpte & ATTR_AP_RW_BIT) ==
				    ATTR_AP(ATTR_AP_RW)) {
					switch (lvl) {
					case 1:
						for (mt = m; mt < &m[L2_SIZE / PAGE_SIZE]; mt++)
							vm_page_dirty(m);
						break;
					case 2:
						vm_page_dirty(m);
						break;
					}
				}

				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(&lock, m);

				/* Mark free */
				pc->pc_map[field] |= bitmask;
				switch (lvl) {
				case 1:
					pmap_resident_count_dec(pmap,
					    L2_SIZE / PAGE_SIZE);
					pvh = pa_to_pvh(tpte & ~ATTR_MASK);
					TAILQ_REMOVE(&pvh->pv_list, pv,pv_next);
					pvh->pv_gen++;
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[L2_SIZE / PAGE_SIZE]; mt++)
							if ((mt->aflags & PGA_WRITEABLE) != 0 &&
							    TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					ml3 = pmap_lookup_pt_page(pmap,
					    pv->pv_va);
					if (ml3 != NULL) {
						pmap_remove_pt_page(pmap, ml3);
						pmap_resident_count_dec(pmap,1);
						KASSERT(ml3->wire_count == NL3PG,
						    ("pmap_remove_pages: l3 page wire count error"));
						ml3->wire_count = 0;
						pmap_add_delayed_free_list(ml3,
						    &free, FALSE);
						atomic_subtract_int(
						    &vm_cnt.v_wire_count, 1);
					}
					break;
				case 2:
					pmap_resident_count_dec(pmap, 1);
					TAILQ_REMOVE(&m->md.pv_list, pv,
					    pv_next);
					m->md.pv_gen++;
					if ((m->aflags & PGA_WRITEABLE) != 0 &&
					    TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(
						    VM_PAGE_TO_PHYS(m));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m,
							    PGA_WRITEABLE);
					}
					break;
				}
				pmap_unuse_l3(pmap, pv->pv_va, pmap_load(pde),
				    &free);
				freed++;
			}
		}
		PV_STAT(atomic_add_long(&pv_entry_frees, freed));
		PV_STAT(atomic_add_int(&pv_entry_spare, freed));
		PV_STAT(atomic_subtract_long(&pv_entry_count, freed));
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			free_pv_chunk(pc);
		}
	}
	pmap_invalidate_all(pmap);
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(&free);
}

/*
 * This is used to check if a page has been accessed or modified. As we
 * don't have a bit to see if it has been modified we have to assume it
 * has been if the page is read/write.
 */
static boolean_t
pmap_page_test_mappings(vm_page_t m, boolean_t accessed, boolean_t modified)
{
	struct rwlock *lock;
	pv_entry_t pv;
	struct md_page *pvh;
	pt_entry_t *pte, mask, value;
	pmap_t pmap;
	int lvl, md_gen, pvh_gen;
	boolean_t rv;

	rv = FALSE;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va, &lvl);
		KASSERT(lvl == 3,
		    ("pmap_page_test_mappings: Invalid level %d", lvl));
		mask = 0;
		value = 0;
		if (modified) {
			mask |= ATTR_AP_RW_BIT;
			value |= ATTR_AP(ATTR_AP_RW);
		}
		if (accessed) {
			mask |= ATTR_AF | ATTR_DESCR_MASK;
			value |= ATTR_AF | L3_PAGE;
		}
		rv = (pmap_load(pte) & mask) == value;
		PMAP_UNLOCK(pmap);
		if (rv)
			goto out;
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pte(pmap, pv->pv_va, &lvl);
			KASSERT(lvl == 2,
			    ("pmap_page_test_mappings: Invalid level %d", lvl));
			mask = 0;
			value = 0;
			if (modified) {
				mask |= ATTR_AP_RW_BIT;
				value |= ATTR_AP(ATTR_AP_RW);
			}
			if (accessed) {
				mask |= ATTR_AF | ATTR_DESCR_MASK;
				value |= ATTR_AF | L2_BLOCK;
			}
			rv = (pmap_load(pte) & mask) == value;
			PMAP_UNLOCK(pmap);
			if (rv)
				goto out;
		}
	}
out:
	rw_runlock(lock);
	return (rv);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));

	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no PTEs can have PG_M set.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return (FALSE);
	return (pmap_page_test_mappings(m, FALSE, TRUE));
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is eligible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pt_entry_t *pte;
	boolean_t rv;
	int lvl;

	rv = FALSE;
	PMAP_LOCK(pmap);
	pte = pmap_pte(pmap, addr, &lvl);
	if (pte != NULL && pmap_load(pte) != 0) {
		rv = TRUE;
	}
	PMAP_UNLOCK(pmap);
	return (rv);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
boolean_t
pmap_is_referenced(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	return (pmap_page_test_mappings(m, TRUE, FALSE));
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	struct rwlock *lock;
	pv_entry_t next_pv, pv;
	pt_entry_t oldpte, *pte;
	vm_offset_t va;
	int lvl, md_gen, pvh_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * set by another thread while the object is locked.  Thus,
	 * if PGA_WRITEABLE is clear, no page table entries need updating.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
retry_pv_loop:
	rw_wlock(lock);
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				rw_wunlock(lock);
				goto retry_pv_loop;
			}
		}
		va = pv->pv_va;
		pte = pmap_pte(pmap, pv->pv_va, &lvl);
		if ((pmap_load(pte) & ATTR_AP_RW_BIT) == ATTR_AP(ATTR_AP_RW))
			pmap_demote_l2_locked(pmap, pte, va & ~L2_OFFSET,
			    &lock);
		KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
		    ("inconsistent pv lock %p %p for page %p",
		    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
		PMAP_UNLOCK(pmap);
	}
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen ||
			    md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				rw_wunlock(lock);
				goto retry_pv_loop;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va, &lvl);
retry:
		oldpte = pmap_load(pte);
		if ((oldpte & ATTR_AP_RW_BIT) == ATTR_AP(ATTR_AP_RW)) {
			if (!atomic_cmpset_long(pte, oldpte,
			    oldpte | ATTR_AP(ATTR_AP_RO)))
				goto retry;
			if ((oldpte & ATTR_AF) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	vm_page_aflag_clear(m, PGA_WRITEABLE);
}

static __inline boolean_t
safe_to_clear_referenced(pmap_t pmap, pt_entry_t pte)
{

	return (FALSE);
}

#define	PMAP_TS_REFERENCED_MAX	5

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 *	XXX: The exact number of bits to check and clear is a matter that
 *	should be tested and standardized at some point in the future for
 *	optimal aging of shared pages.
 */
int
pmap_ts_referenced(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t *pde, tpde;
	pt_entry_t *pte, tpte;
	pt_entry_t *l3;
	vm_offset_t va;
	vm_paddr_t pa;
	int cleared, md_gen, not_cleared, lvl, pvh_gen;
	struct spglist free;
	bool demoted;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	SLIST_INIT(&free);
	cleared = 0;
	pa = VM_PAGE_TO_PHYS(m);
	lock = PHYS_TO_PV_LIST_LOCK(pa);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy : pa_to_pvh(pa);
	rw_wlock(lock);
retry:
	not_cleared = 0;
	if ((pvf = TAILQ_FIRST(&pvh->pv_list)) == NULL)
		goto small_mappings;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		va = pv->pv_va;
		pde = pmap_pde(pmap, pv->pv_va, &lvl);
		KASSERT(pde != NULL, ("pmap_ts_referenced: no l1 table found"));
		KASSERT(lvl == 1,
		    ("pmap_ts_referenced: invalid pde level %d", lvl));
		tpde = pmap_load(pde);
		KASSERT((tpde & ATTR_DESCR_MASK) == L1_TABLE,
		    ("pmap_ts_referenced: found an invalid l1 table"));
		pte = pmap_l1_to_l2(pde, pv->pv_va);
		tpte = pmap_load(pte);
		if ((tpte & ATTR_AF) != 0) {
			/*
			 * Since this reference bit is shared by 512 4KB
			 * pages, it should not be cleared every time it is
			 * tested.  Apply a simple "hash" function on the
			 * physical page number, the virtual superpage number,
			 * and the pmap address to select one 4KB page out of
			 * the 512 on which testing the reference bit will
			 * result in clearing that reference bit.  This
			 * function is designed to avoid the selection of the
			 * same 4KB page for every 2MB page mapping.
			 *
			 * On demotion, a mapping that hasn't been referenced
			 * is simply destroyed.  To avoid the possibility of a
			 * subsequent page fault on a demoted wired mapping,
			 * always leave its reference bit set.  Moreover,
			 * since the superpage is wired, the current state of
			 * its reference bit won't affect page replacement.
			 */
			if ((((pa >> PAGE_SHIFT) ^ (pv->pv_va >> L2_SHIFT) ^
			    (uintptr_t)pmap) & (Ln_ENTRIES - 1)) == 0 &&
			    (tpte & ATTR_SW_WIRED) == 0) {
				if (safe_to_clear_referenced(pmap, tpte)) {
					/*
					 * TODO: We don't handle the access
					 * flag at all. We need to be able
					 * to set it in  the exception handler.
					 */
					panic("ARM64TODO: "
					    "safe_to_clear_referenced\n");
				} else if (pmap_demote_l2_locked(pmap, pte,
				    pv->pv_va, &lock) != NULL) {
					demoted = true;
					va += VM_PAGE_TO_PHYS(m) -
					    (tpte & ~ATTR_MASK);
					l3 = pmap_l2_to_l3(pte, va);
					pmap_remove_l3(pmap, l3, va,
					    pmap_load(pte), NULL, &lock);
				} else
					demoted = true;

				if (demoted) {
					/*
					 * The superpage mapping was removed
					 * entirely and therefore 'pv' is no
					 * longer valid.
					 */
					if (pvf == pv)
						pvf = NULL;
					pv = NULL;
				}
				cleared++;
				KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
				    ("inconsistent pv lock %p %p for page %p",
				    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
			} else
				not_cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
			pvh->pv_gen++;
		}
		if (cleared + not_cleared >= PMAP_TS_REFERENCED_MAX)
			goto out;
	} while ((pv = TAILQ_FIRST(&pvh->pv_list)) != pvf);
small_mappings:
	if ((pvf = TAILQ_FIRST(&m->md.pv_list)) == NULL)
		goto out;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		pde = pmap_pde(pmap, pv->pv_va, &lvl);
		KASSERT(pde != NULL, ("pmap_ts_referenced: no l2 table found"));
		KASSERT(lvl == 2,
		    ("pmap_ts_referenced: invalid pde level %d", lvl));
		tpde = pmap_load(pde);
		KASSERT((tpde & ATTR_DESCR_MASK) == L2_TABLE,
		    ("pmap_ts_referenced: found an invalid l2 table"));
		pte = pmap_l2_to_l3(pde, pv->pv_va);
		tpte = pmap_load(pte);
		if ((tpte & ATTR_AF) != 0) {
			if (safe_to_clear_referenced(pmap, tpte)) {
				/*
				 * TODO: We don't handle the access flag
				 * at all. We need to be able to set it in
				 * the exception handler.
				 */
				panic("ARM64TODO: safe_to_clear_referenced\n");
			} else if ((tpte & ATTR_SW_WIRED) == 0) {
				/*
				 * Wired pages cannot be paged out so
				 * doing accessed bit emulation for
				 * them is wasted effort. We do the
				 * hard work for unwired pages only.
				 */
				pmap_remove_l3(pmap, pte, pv->pv_va, tpde,
				    &free, &lock);
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
				if (pvf == pv)
					pvf = NULL;
				pv = NULL;
				KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
				    ("inconsistent pv lock %p %p for page %p",
				    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
			} else
				not_cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
			m->md.pv_gen++;
		}
	} while ((pv = TAILQ_FIRST(&m->md.pv_list)) != pvf && cleared +
	    not_cleared < PMAP_TS_REFERENCED_MAX);
out:
	rw_wunlock(lock);
	pmap_free_zero_pages(&free);
	return (cleared + not_cleared);
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	KASSERT(!vm_page_xbusied(m),
	    ("pmap_clear_modify: page %p is exclusive busied", m));

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have PG_M set.
	 * If the object containing the page is locked and the page is not
	 * exclusive busied, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;

	/* ARM64TODO: We lack support for tracking if a page is modified */
}

void *
pmap_mapbios(vm_paddr_t pa, vm_size_t size)
{

        return ((void *)PHYS_TO_DMAP(pa));
}

void
pmap_unmapbios(vm_paddr_t pa, vm_size_t size)
{
}

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	m->md.pv_memattr = ma;

	/*
	 * If "m" is a normal page, update its direct mapping.  This update
	 * can be relied upon to perform any cache operations that are
	 * required for data coherence.
	 */
	if ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_change_attr(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)), PAGE_SIZE,
	    m->md.pv_memattr) != 0)
		panic("memory attribute change on the direct map failed");
}

/*
 * Changes the specified virtual address range's memory type to that given by
 * the parameter "mode".  The specified virtual address range must be
 * completely contained within either the direct map or the kernel map.  If
 * the virtual address range is contained within the kernel map, then the
 * memory type for each of the corresponding ranges of the direct map is also
 * changed.  (The corresponding ranges of the direct map are those ranges that
 * map the same physical pages as the specified virtual address range.)  These
 * changes to the direct map are necessary because Intel describes the
 * behavior of their processors as "undefined" if two or more mappings to the
 * same physical page have different memory types.
 *
 * Returns zero if the change completed successfully, and either EINVAL or
 * ENOMEM if the change failed.  Specifically, EINVAL is returned if some part
 * of the virtual address range was not mapped, and ENOMEM is returned if
 * there was insufficient memory available to complete the change.  In the
 * latter case, the memory type may have been changed on some part of the
 * virtual address range or the direct map.
 */
static int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
	int error;

	PMAP_LOCK(kernel_pmap);
	error = pmap_change_attr_locked(va, size, mode);
	PMAP_UNLOCK(kernel_pmap);
	return (error);
}

static int
pmap_change_attr_locked(vm_offset_t va, vm_size_t size, int mode)
{
	vm_offset_t base, offset, tmpva;
	pt_entry_t l3, *pte, *newpte;
	int lvl;

	PMAP_LOCK_ASSERT(kernel_pmap, MA_OWNED);
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	if (!VIRT_IN_DMAP(base))
		return (EINVAL);

	for (tmpva = base; tmpva < base + size; ) {
		pte = pmap_pte(kernel_pmap, va, &lvl);
		if (pte == NULL)
			return (EINVAL);

		if ((pmap_load(pte) & ATTR_IDX_MASK) == ATTR_IDX(mode)) {
			/*
			 * We already have the correct attribute,
			 * ignore this entry.
			 */
			switch (lvl) {
			default:
				panic("Invalid DMAP table level: %d\n", lvl);
			case 1:
				tmpva = (tmpva & ~L1_OFFSET) + L1_SIZE;
				break;
			case 2:
				tmpva = (tmpva & ~L2_OFFSET) + L2_SIZE;
				break;
			case 3:
				tmpva += PAGE_SIZE;
				break;
			}
		} else {
			/*
			 * Split the entry to an level 3 table, then
			 * set the new attribute.
			 */
			switch (lvl) {
			default:
				panic("Invalid DMAP table level: %d\n", lvl);
			case 1:
				newpte = pmap_demote_l1(kernel_pmap, pte,
				    tmpva & ~L1_OFFSET);
				if (newpte == NULL)
					return (EINVAL);
				pte = pmap_l1_to_l2(pte, tmpva);
			case 2:
				newpte = pmap_demote_l2(kernel_pmap, pte,
				    tmpva & ~L2_OFFSET);
				if (newpte == NULL)
					return (EINVAL);
				pte = pmap_l2_to_l3(pte, tmpva);
			case 3:
				/* Update the entry */
				l3 = pmap_load(pte);
				l3 &= ~ATTR_IDX_MASK;
				l3 |= ATTR_IDX(mode);

				pmap_update_entry(kernel_pmap, pte, l3, tmpva,
				    PAGE_SIZE);

				/*
				 * If moving to a non-cacheable entry flush
				 * the cache.
				 */
				if (mode == VM_MEMATTR_UNCACHEABLE)
					cpu_dcache_wbinv_range(tmpva, L3_SIZE);

				break;
			}
			tmpva += PAGE_SIZE;
		}
	}

	return (0);
}

/*
 * Create an L2 table to map all addresses within an L1 mapping.
 */
static pt_entry_t *
pmap_demote_l1(pmap_t pmap, pt_entry_t *l1, vm_offset_t va)
{
	pt_entry_t *l2, newl2, oldl1;
	vm_offset_t tmpl1;
	vm_paddr_t l2phys, phys;
	vm_page_t ml2;
	int i;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldl1 = pmap_load(l1);
	KASSERT((oldl1 & ATTR_DESCR_MASK) == L1_BLOCK,
	    ("pmap_demote_l1: Demoting a non-block entry"));
	KASSERT((va & L1_OFFSET) == 0,
	    ("pmap_demote_l1: Invalid virtual address %#lx", va));
	KASSERT((oldl1 & ATTR_SW_MANAGED) == 0,
	    ("pmap_demote_l1: Level 1 table shouldn't be managed"));

	tmpl1 = 0;
	if (va <= (vm_offset_t)l1 && va + L1_SIZE > (vm_offset_t)l1) {
		tmpl1 = kva_alloc(PAGE_SIZE);
		if (tmpl1 == 0)
			return (NULL);
	}

	if ((ml2 = vm_page_alloc(NULL, 0, VM_ALLOC_INTERRUPT |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
		CTR2(KTR_PMAP, "pmap_demote_l1: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (NULL);
	}

	l2phys = VM_PAGE_TO_PHYS(ml2);
	l2 = (pt_entry_t *)PHYS_TO_DMAP(l2phys);

	/* Address the range points at */
	phys = oldl1 & ~ATTR_MASK;
	/* The attributed from the old l1 table to be copied */
	newl2 = oldl1 & ATTR_MASK;

	/* Create the new entries */
	for (i = 0; i < Ln_ENTRIES; i++) {
		l2[i] = newl2 | phys;
		phys += L2_SIZE;
	}
	cpu_dcache_wb_range((vm_offset_t)l2, PAGE_SIZE);
	KASSERT(l2[0] == ((oldl1 & ~ATTR_DESCR_MASK) | L2_BLOCK),
	    ("Invalid l2 page (%lx != %lx)", l2[0],
	    (oldl1 & ~ATTR_DESCR_MASK) | L2_BLOCK));

	if (tmpl1 != 0) {
		pmap_kenter(tmpl1, PAGE_SIZE,
		    DMAP_TO_PHYS((vm_offset_t)l1) & ~L3_OFFSET, CACHED_MEMORY);
		l1 = (pt_entry_t *)(tmpl1 + ((vm_offset_t)l1 & PAGE_MASK));
	}

	pmap_update_entry(pmap, l1, l2phys | L1_TABLE, va, PAGE_SIZE);

	if (tmpl1 != 0) {
		pmap_kremove(tmpl1);
		kva_free(tmpl1, PAGE_SIZE);
	}

	return (l2);
}

/*
 * Create an L3 table to map all addresses within an L2 mapping.
 */
static pt_entry_t *
pmap_demote_l2_locked(pmap_t pmap, pt_entry_t *l2, vm_offset_t va,
    struct rwlock **lockp)
{
	pt_entry_t *l3, newl3, oldl2;
	vm_offset_t tmpl2;
	vm_paddr_t l3phys, phys;
	vm_page_t ml3;
	int i;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	l3 = NULL;
	oldl2 = pmap_load(l2);
	KASSERT((oldl2 & ATTR_DESCR_MASK) == L2_BLOCK,
	    ("pmap_demote_l2: Demoting a non-block entry"));
	KASSERT((va & L2_OFFSET) == 0,
	    ("pmap_demote_l2: Invalid virtual address %#lx", va));

	tmpl2 = 0;
	if (va <= (vm_offset_t)l2 && va + L2_SIZE > (vm_offset_t)l2) {
		tmpl2 = kva_alloc(PAGE_SIZE);
		if (tmpl2 == 0)
			return (NULL);
	}

	if ((ml3 = pmap_lookup_pt_page(pmap, va)) != NULL) {
		pmap_remove_pt_page(pmap, ml3);
	} else {
		ml3 = vm_page_alloc(NULL, pmap_l2_pindex(va),
		    (VIRT_IN_DMAP(va) ? VM_ALLOC_INTERRUPT : VM_ALLOC_NORMAL) |
		    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED);
		if (ml3 == NULL) {
			CTR2(KTR_PMAP, "pmap_demote_l2: failure for va %#lx"
			    " in pmap %p", va, pmap);
			goto fail;
		}
		if (va < VM_MAXUSER_ADDRESS)
			pmap_resident_count_inc(pmap, 1);
	}

	l3phys = VM_PAGE_TO_PHYS(ml3);
	l3 = (pt_entry_t *)PHYS_TO_DMAP(l3phys);

	/* Address the range points at */
	phys = oldl2 & ~ATTR_MASK;
	/* The attributed from the old l2 table to be copied */
	newl3 = (oldl2 & (ATTR_MASK & ~ATTR_DESCR_MASK)) | L3_PAGE;

	/*
	 * If the page table page is new, initialize it.
	 */
	if (ml3->wire_count == 1) {
		for (i = 0; i < Ln_ENTRIES; i++) {
			l3[i] = newl3 | phys;
			phys += L3_SIZE;
		}
		cpu_dcache_wb_range((vm_offset_t)l3, PAGE_SIZE);
	}
	KASSERT(l3[0] == ((oldl2 & ~ATTR_DESCR_MASK) | L3_PAGE),
	    ("Invalid l3 page (%lx != %lx)", l3[0],
	    (oldl2 & ~ATTR_DESCR_MASK) | L3_PAGE));

	/*
	 * Map the temporary page so we don't lose access to the l2 table.
	 */
	if (tmpl2 != 0) {
		pmap_kenter(tmpl2, PAGE_SIZE,
		    DMAP_TO_PHYS((vm_offset_t)l2) & ~L3_OFFSET, CACHED_MEMORY);
		l2 = (pt_entry_t *)(tmpl2 + ((vm_offset_t)l2 & PAGE_MASK));
	}

	/*
	 * The spare PV entries must be reserved prior to demoting the
	 * mapping, that is, prior to changing the PDE.  Otherwise, the state
	 * of the L2 and the PV lists will be inconsistent, which can result
	 * in reclaim_pv_chunk() attempting to remove a PV entry from the
	 * wrong PV list and pmap_pv_demote_l2() failing to find the expected
	 * PV entry for the 2MB page mapping that is being demoted.
	 */
	if ((oldl2 & ATTR_SW_MANAGED) != 0)
		reserve_pv_entries(pmap, Ln_ENTRIES - 1, lockp);

	pmap_update_entry(pmap, l2, l3phys | L2_TABLE, va, PAGE_SIZE);

	/*
	 * Demote the PV entry.
	 */
	if ((oldl2 & ATTR_SW_MANAGED) != 0)
		pmap_pv_demote_l2(pmap, va, oldl2 & ~ATTR_MASK, lockp);

	atomic_add_long(&pmap_l2_demotions, 1);
	CTR3(KTR_PMAP, "pmap_demote_l2: success for va %#lx"
	    " in pmap %p %lx", va, pmap, l3[0]);

fail:
	if (tmpl2 != 0) {
		pmap_kremove(tmpl2);
		kva_free(tmpl2, PAGE_SIZE);
	}

	return (l3);

}

static pt_entry_t *
pmap_demote_l2(pmap_t pmap, pt_entry_t *l2, vm_offset_t va)
{
	struct rwlock *lock;
	pt_entry_t *l3;

	lock = NULL;
	l3 = pmap_demote_l2_locked(pmap, l2, va, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	return (l3);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pd_entry_t *l1p, l1;
	pd_entry_t *l2p, l2;
	pt_entry_t *l3p, l3;
	vm_paddr_t pa;
	bool managed;
	int val;

	PMAP_LOCK(pmap);
retry:
	pa = 0;
	val = 0;
	managed = false;

	l1p = pmap_l1(pmap, addr);
	if (l1p == NULL) /* No l1 */
		goto done;

	l1 = pmap_load(l1p);
	if ((l1 & ATTR_DESCR_MASK) == L1_INVAL)
		goto done;

	if ((l1 & ATTR_DESCR_MASK) == L1_BLOCK) {
		pa = (l1 & ~ATTR_MASK) | (addr & L1_OFFSET);
		managed = (l1 & ATTR_SW_MANAGED) == ATTR_SW_MANAGED;
		val = MINCORE_SUPER | MINCORE_INCORE;
		if (pmap_page_dirty(l1))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((l1 & ATTR_AF) == ATTR_AF)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
		goto done;
	}

	l2p = pmap_l1_to_l2(l1p, addr);
	if (l2p == NULL) /* No l2 */
		goto done;

	l2 = pmap_load(l2p);
	if ((l2 & ATTR_DESCR_MASK) == L2_INVAL)
		goto done;

	if ((l2 & ATTR_DESCR_MASK) == L2_BLOCK) {
		pa = (l2 & ~ATTR_MASK) | (addr & L2_OFFSET);
		managed = (l2 & ATTR_SW_MANAGED) == ATTR_SW_MANAGED;
		val = MINCORE_SUPER | MINCORE_INCORE;
		if (pmap_page_dirty(l2))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((l2 & ATTR_AF) == ATTR_AF)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
		goto done;
	}

	l3p = pmap_l2_to_l3(l2p, addr);
	if (l3p == NULL) /* No l3 */
		goto done;

	l3 = pmap_load(l2p);
	if ((l3 & ATTR_DESCR_MASK) == L3_INVAL)
		goto done;

	if ((l3 & ATTR_DESCR_MASK) == L3_PAGE) {
		pa = (l3 & ~ATTR_MASK) | (addr & L3_OFFSET);
		managed = (l3 & ATTR_SW_MANAGED) == ATTR_SW_MANAGED;
		val = MINCORE_INCORE;
		if (pmap_page_dirty(l3))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((l3 & ATTR_AF) == ATTR_AF)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	}

done:
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) && managed) {
		/* Ensure that "PHYS_TO_VM_PAGE(pa)->object" doesn't change. */
		if (vm_page_pa_tryrelock(pmap, pa, locked_pa))
			goto retry;
	} else
		PA_UNLOCK_COND(*locked_pa);
	PMAP_UNLOCK(pmap);

	return (val);
}

void
pmap_activate(struct thread *td)
{
	pmap_t	pmap;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	td->td_pcb->pcb_l0addr = vtophys(pmap->pm_l0);
	__asm __volatile("msr ttbr0_el1, %0" : : "r"(td->td_pcb->pcb_l0addr));
	pmap_invalidate_all(pmap);
	critical_exit();
}

void
pmap_sync_icache(pmap_t pmap, vm_offset_t va, vm_size_t sz)
{

	if (va >= VM_MIN_KERNEL_ADDRESS) {
		cpu_icache_sync_range(va, sz);
	} else {
		u_int len, offset;
		vm_paddr_t pa;

		/* Find the length of data in this page to flush */
		offset = va & PAGE_MASK;
		len = imin(PAGE_SIZE - offset, sz);

		while (sz != 0) {
			/* Extract the physical address & find it in the DMAP */
			pa = pmap_extract(pmap, va);
			if (pa != 0)
				cpu_icache_sync_range(PHYS_TO_DMAP(pa), len);

			/* Move to the next page */
			sz -= len;
			va += len;
			/* Set the length for the next iteration */
			len = imin(PAGE_SIZE, sz);
		}
	}
}

int
pmap_fault(pmap_t pmap, uint64_t esr, uint64_t far)
{
#ifdef SMP
	uint64_t par;
#endif

	switch (ESR_ELx_EXCEPTION(esr)) {
	case EXCP_DATA_ABORT_L:
	case EXCP_DATA_ABORT:
		break;
	default:
		return (KERN_FAILURE);
	}

#ifdef SMP
	PMAP_LOCK(pmap);
	switch (esr & ISS_DATA_DFSC_MASK) {
	case ISS_DATA_DFSC_TF_L0:
	case ISS_DATA_DFSC_TF_L1:
	case ISS_DATA_DFSC_TF_L2:
	case ISS_DATA_DFSC_TF_L3:
		/* Ask the MMU to check the address */
		if (pmap == kernel_pmap)
			par = arm64_address_translate_s1e1r(far);
		else
			par = arm64_address_translate_s1e0r(far);

		/*
		 * If the translation was successful the address was invalid
		 * due to a break-before-make sequence. We can unlock and
		 * return success to the trap handler.
		 */
		if (PAR_SUCCESS(par)) {
			PMAP_UNLOCK(pmap);
			return (KERN_SUCCESS);
		}
		break;
	default:
		break;
	}
	PMAP_UNLOCK(pmap);
#endif

	return (KERN_FAILURE);
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
	vm_offset_t superpage_offset;

	if (size < L2_SIZE)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & L2_OFFSET;
	if (size - ((L2_SIZE - superpage_offset) & L2_OFFSET) < L2_SIZE ||
	    (*addr & L2_OFFSET) == superpage_offset)
		return;
	if ((*addr & L2_OFFSET) < superpage_offset)
		*addr = (*addr & ~L2_OFFSET) + superpage_offset;
	else
		*addr = ((*addr + L2_OFFSET) & ~L2_OFFSET) + superpage_offset;
}

/**
 * Get the kernel virtual address of a set of physical pages. If there are
 * physical addresses not covered by the DMAP perform a transient mapping
 * that will be removed when calling pmap_unmap_io_transient.
 *
 * \param page        The pages the caller wishes to obtain the virtual
 *                    address on the kernel memory map.
 * \param vaddr       On return contains the kernel virtual memory address
 *                    of the pages passed in the page parameter.
 * \param count       Number of pages passed in.
 * \param can_fault   TRUE if the thread using the mapped pages can take
 *                    page faults, FALSE otherwise.
 *
 * \returns TRUE if the caller must call pmap_unmap_io_transient when
 *          finished or FALSE otherwise.
 *
 */
boolean_t
pmap_map_io_transient(vm_page_t page[], vm_offset_t vaddr[], int count,
    boolean_t can_fault)
{
	vm_paddr_t paddr;
	boolean_t needs_mapping;
	int error, i;

	/*
	 * Allocate any KVA space that we need, this is done in a separate
	 * loop to prevent calling vmem_alloc while pinned.
	 */
	needs_mapping = FALSE;
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (__predict_false(!PHYS_IN_DMAP(paddr))) {
			error = vmem_alloc(kernel_arena, PAGE_SIZE,
			    M_BESTFIT | M_WAITOK, &vaddr[i]);
			KASSERT(error == 0, ("vmem_alloc failed: %d", error));
			needs_mapping = TRUE;
		} else {
			vaddr[i] = PHYS_TO_DMAP(paddr);
		}
	}

	/* Exit early if everything is covered by the DMAP */
	if (!needs_mapping)
		return (FALSE);

	if (!can_fault)
		sched_pin();
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (!PHYS_IN_DMAP(paddr)) {
			panic(
			   "pmap_map_io_transient: TODO: Map out of DMAP data");
		}
	}

	return (needs_mapping);
}

void
pmap_unmap_io_transient(vm_page_t page[], vm_offset_t vaddr[], int count,
    boolean_t can_fault)
{
	vm_paddr_t paddr;
	int i;

	if (!can_fault)
		sched_unpin();
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (!PHYS_IN_DMAP(paddr)) {
			panic("ARM64TODO: pmap_unmap_io_transient: Unmap data");
		}
	}
}
