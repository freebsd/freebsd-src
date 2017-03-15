/*
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
 * Copyright (c) 2015 Stacey D. Son <sson@...>
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: src/sys/i386/i386/pmap.c,v 1.250.2.8 2000/11/21 00:09:14 ps
 *	JNPR: pmap.c,v 1.11.2.1 2007/08/16 11:51:06 girish
 */

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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_pmap.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#ifdef SMP
#include <sys/smp.h>
#else
#include <sys/cpuset.h>
#endif
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/uma.h>

#include <machine/cache.h>
#include <machine/cpuinfo.h>
#include <machine/md_var.h>
#include <machine/tlb.h>

#undef PMAP_DEBUG

#if !defined(DIAGNOSTIC)
#define	PMAP_INLINE __inline
#else
#define	PMAP_INLINE
#endif

// #define PV_STATS
#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

#define	pa_index(pa)	((pa) >> PDRSHIFT)
#define	pa_to_pvh(pa)	(&pv_table[pa_index(pa)])

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

#define CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m)	\
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

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_seg_index(v)	(((v) >> SEGSHIFT) & (NPDEPG - 1))
#define	pmap_pde_index(v)	(((v) >> PDRSHIFT) & (NPDEPG - 1))
#define	pmap_pte_index(v)	(((v) >> PAGE_SHIFT) & (NPTEPG - 1))
#define	pmap_pde_pindex(v)	((v) >> PDRSHIFT)

#define	NUPDE			(NPDEPG * NPDEPG)
#define	NUSERPGTBLS		(NUPDE + NPDEPG)

#define	is_kernel_pmap(x)	((x) == kernel_pmap)

struct pmap kernel_pmap_store;
pd_entry_t *kernel_segmap;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

static int nkpt;
unsigned pmap_max_asid;		/* max ASID supported by the system */

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");
static int pg_sp_enabled = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, pg_ps_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pg_sp_enabled, 0, "Are large page mappings enabled?");

#define	PMAP_ASID_RESERVED	0

vm_offset_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;

static void pmap_asid_alloc(pmap_t pmap);

static struct rwlock_padalign pvh_global_lock;

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static struct mtx pv_chunks_mutex;
static struct rwlock pv_list_locks[NPV_LIST_LOCKS];
static struct md_page *pv_table;

static void free_pv_chunk(struct pv_chunk *pc);
static void free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t pmap, struct rwlock **lockp);
static vm_page_t reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp);
static void pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
    vm_offset_t va);
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte,  struct rwlock **lockp);
static void reserve_pv_entries(pmap_t pmap, int needed,
    struct rwlock **lockp);
static boolean_t pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va);
static boolean_t pmap_demote_pde_locked(pmap_t pmap, pd_entry_t *pde,
    vm_offset_t va, struct rwlock **lockp);
static vm_page_t pmap_allocpde(pmap_t pmap, vm_offset_t va,
    struct rwlock **lockp);
static boolean_t pmap_enter_pde(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, struct rwlock **lockp);
static void pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp);
static boolean_t pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp);
static void pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp);
static void pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte);
static __inline int pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte);
static __inline vm_page_t pmap_lookup_pt_page(pmap_t pmap, vm_offset_t va);
static void pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp);
static int pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp);
static int pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va,
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va,
    struct spglist *free);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m, struct rwlock **lockp);
static void pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte);
static void pmap_invalidate_all(pmap_t pmap);
static void pmap_invalidate_page(pmap_t pmap, vm_offset_t va);
static void _pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va,
    struct rwlock **lockp);
static vm_page_t _pmap_allocpte(pmap_t pmap, unsigned ptepindex,
    struct rwlock **lockp);
static int pmap_unuse_pt(pmap_t, vm_offset_t, pd_entry_t, struct spglist *);
static pt_entry_t init_pte_prot(vm_page_t m, vm_prot_t access, vm_prot_t prot);

static void pmap_invalidate_page_action(void *arg);
static void pmap_invalidate_range_action(void *arg);
static void pmap_update_page_action(void *arg);

static __inline int
pmap_pte_cache_bits(vm_paddr_t pa, vm_page_t m)
{
	vm_memattr_t ma;

	ma = pmap_page_get_memattr(m);
	if (ma == VM_MEMATTR_WRITE_BACK && !is_cacheable_mem(pa))
		ma = VM_MEMATTR_UNCACHEABLE;
	return PTE_C(ma);
}
#define PMAP_PTE_SET_CACHE_BITS(pte, ps, m) {   \
    pte &= ~PTE_C_MASK;                         \
    pte |= pmap_pte_cache_bits(pa, m);          \
}

/*
 * Page table entry lookup routines.
 */

/* Return a segment entry for given pmap & virtual address. */
static __inline pd_entry_t *
pmap_segmap(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_segtab[pmap_seg_index(va)]);
}

/* Return a page directory entry for given segment table & virtual address. */
static __inline pd_entry_t *
pmap_pdpe_to_pde(pd_entry_t *pdpe, vm_offset_t va)
{
	pd_entry_t *pde;

	pde = (pd_entry_t *)*pdpe;
	return (&pde[pmap_pde_index(va)]);
}

/* Return a page directory entry for given pmap & virtual address. */
static __inline pd_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pdpe;

	pdpe = pmap_segmap(pmap, va);
	if (*pdpe == NULL)
		return (NULL);

	return (pmap_pdpe_to_pde(pdpe, va));
}

/* Return a page table entry for given page directory & virtual address. */
static __inline pt_entry_t *
pmap_pde_to_pte(pd_entry_t *pde, vm_offset_t va)
{
	pt_entry_t *pte;

	pte = (pt_entry_t *)*pde;
	return (&pte[pmap_pte_index(va)]);
}

/* Return a page table entry for given pmap & virtual address. */
pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (pde == NULL || *pde == NULL)
		return (NULL);
	if (pde_is_superpage(pde)) {
		return ((pt_entry_t *)pde);
	} else
		return (pmap_pde_to_pte(pde, va));
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

/*
 * Allocate some wired memory before the virtual memory system is
 * bootstrapped.
 */
vm_offset_t
pmap_steal_memory(vm_size_t size)
{
	vm_paddr_t bank_size, pa;
	vm_offset_t va;

	size = round_page(size);
	bank_size = phys_avail[1] - phys_avail[0];
	while (size > bank_size) {
		int i;

		for (i = 0; phys_avail[i + 2]; i += 2) {
			phys_avail[i] = phys_avail[i + 2];
			phys_avail[i + 1] = phys_avail[i + 3];
		}
		phys_avail[i] = 0;
		phys_avail[i + 1] = 0;
		if (!phys_avail[0])
			panic("pmap_steal_memory: out of memory");
		bank_size = phys_avail[1] - phys_avail[0];
	}

	pa = phys_avail[0];
	phys_avail[0] += size;
	va = MIPS_PHYS_TO_DIRECT(pa);
	bzero((caddr_t)va, size);
	return (va);
}

/*
 * Bootstrap the system enough to run with virtual memory.  This
 * assumes that the phys_avail array has been initialized.
 */
static void
pmap_create_kernel_pagetable(void)
{
	int i, j;
	vm_offset_t ptaddr;
	pt_entry_t *pte;
	pd_entry_t *pde;
	vm_offset_t pdaddr;
	int npt, npde;

	/*
	 * Allocate segment table for the kernel
	 */
	kernel_segmap = (pd_entry_t *)pmap_steal_memory(PAGE_SIZE);

	/*
	 * Allocate second level page tables for the kernel
	 */
	npde = howmany(NKPT, NPDEPG);
	pdaddr = pmap_steal_memory(PAGE_SIZE * npde);
	nkpt = NKPT;
	ptaddr = pmap_steal_memory(PAGE_SIZE * nkpt);

	/*
	 * The R[4-7]?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry. Thus invalid
	 * entrys must have the Global bit set so when Entry LO and Entry HI
	 * G bits are anded together they will produce a global bit to store
	 * in the tlb.
	 */
	for (i = 0, pte = (pt_entry_t *)ptaddr; i < (nkpt * NPTEPG); i++, pte++)
		*pte = PTE_G;

	for (i = 0,  npt = nkpt; npt > 0; i++) {
		kernel_segmap[i] = (pd_entry_t)(pdaddr + i * PAGE_SIZE);
		pde = (pd_entry_t *)kernel_segmap[i];

		for (j = 0; j < NPDEPG && npt > 0; j++, npt--)
			pde[j] = (pd_entry_t)(ptaddr + (i * NPDEPG + j) *
			    PAGE_SIZE);
	}

	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_segtab = kernel_segmap;
	CPU_FILL(&kernel_pmap->pm_active);
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);
	kernel_pmap->pm_asid[0].asid = PMAP_ASID_RESERVED;
	kernel_pmap->pm_asid[0].gen = 0;
	kernel_vm_end += nkpt * NPTEPG * PAGE_SIZE;
}

void
pmap_bootstrap(void)
{
	int i;

	/* Sort. */
again:
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		/*
		 * Keep the memory aligned on page boundary.
		 */
		phys_avail[i] = round_page(phys_avail[i]);
		phys_avail[i + 1] = trunc_page(phys_avail[i + 1]);

		if (i < 2)
			continue;
		if (phys_avail[i - 2] > phys_avail[i]) {
			vm_paddr_t ptemp[2];

			ptemp[0] = phys_avail[i + 0];
			ptemp[1] = phys_avail[i + 1];

			phys_avail[i + 0] = phys_avail[i - 2];
			phys_avail[i + 1] = phys_avail[i - 1];

			phys_avail[i - 2] = ptemp[0];
			phys_avail[i - 1] = ptemp[1];
			goto again;
		}
	}

	/*
	 * Copy the phys_avail[] array before we start stealing memory from it.
	 */
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		physmem_desc[i] = phys_avail[i];
		physmem_desc[i + 1] = phys_avail[i + 1];
	}

	Maxmem = atop(phys_avail[i - 1]);

	if (bootverbose) {
		printf("Physical memory chunk(s):\n");
		for (i = 0; phys_avail[i + 1] != 0; i += 2) {
			vm_paddr_t size;

			size = phys_avail[i + 1] - phys_avail[i];
			printf("%#08jx - %#08jx, %ju bytes (%ju pages)\n",
			    (uintmax_t) phys_avail[i],
			    (uintmax_t) phys_avail[i + 1] - 1,
			    (uintmax_t) size, (uintmax_t) size / PAGE_SIZE);
		}
		printf("Maxmem is 0x%0jx\n", ptoa((uintmax_t)Maxmem));
	}
	/*
	 * Steal the message buffer from the beginning of memory.
	 */
	msgbufp = (struct msgbuf *)pmap_steal_memory(msgbufsize);
	msgbufinit(msgbufp, msgbufsize);

	/*
	 * Steal thread0 kstack. This must be aligned to
	 * (KSTACK_PAGE_SIZE * 2) so it can mapped to a single TLB entry.
	 *
	 */
#ifdef KSTACK_LARGE_PAGE
	kstack0 = pmap_steal_memory(((KSTACK_PAGES + KSTACK_GUARD_PAGES) * 2) \
					<< PAGE_SHIFT);
#else
	kstack0 = pmap_steal_memory((KSTACK_PAGES  + KSTACK_GUARD_PAGES) <<
	    PAGE_SHIFT);
#endif
	kstack0 = roundup2(kstack0, (KSTACK_PAGE_SIZE * 2));

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

#ifdef SMP
	/*
	 * Steal some virtual address space to map the pcpu area.
	 */
	virtual_avail = roundup2(virtual_avail, PAGE_SIZE * 2);
	pcpup = (struct pcpu *)virtual_avail;
	virtual_avail += PAGE_SIZE * 2;

	/*
	 * Initialize the wired TLB entry mapping the pcpu region for
	 * the BSP at 'pcpup'. Up until this point we were operating
	 * with the 'pcpup' for the BSP pointing to a virtual address
	 * in KSEG0 so there was no need for a TLB mapping.
	 */
	mips_pcpu_tlb_init(PCPU_ADDR(0));

	if (bootverbose)
		printf("pcpu is available at virtual address %p.\n", pcpup);
#endif

	pmap_create_kernel_pagetable();
	pmap_max_asid = VMNUM_PIDS;
	mips_wr_entryhi(0);
	mips_wr_pagemask(0);

	/*
	 * Initialize the global pv list lock.
	 */
	rw_init(&pvh_global_lock, "pmap pv global");
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_flags = VM_MEMATTR_DEFAULT << PV_MEMATTR_SHIFT;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	int i;
	vm_size_t s;
	int pv_npg;

	/*
	 * Initialize the pv chunk list mutex.
	 */
	mtx_init(&pv_chunks_mutex, "pmap pv chunk list", NULL, MTX_DEF);

	/*
	 * Initialize the pool of pv list locks.
	 */
	for (i = 0; i < NPV_LIST_LOCKS; i++)
		rw_init(&pv_list_locks[i], "pv list");

	/*
	 * Calculate the size of the pv head table for superpages.
	 */
	for (i = 0; phys_avail[i + 1]; i += 2);
	pv_npg = round_2mpage(phys_avail[(i - 2) + 1]) / NBPDR;

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)(pv_npg * sizeof(struct md_page));
	s = round_page(s);
	pv_table = (struct md_page *)kmem_malloc(kernel_arena, s,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);
}

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pde, CTLFLAG_RD, 0,
    "2MB page mapping counters");

static u_long pmap_pde_demotions;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_pde_demotions, 0, "2MB page demotions");

static u_long pmap_pde_mappings;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_pde_mappings, 0, "2MB page mappings");

static u_long pmap_pde_p_failures;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_pde_p_failures, 0, "2MB page promotion failures");

static u_long pmap_pde_promotions;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_pde_promotions, 0, "2MB page promotions");

/***************************************************
 * Low level helper routines.....
 ***************************************************/

#ifdef	SMP
static __inline void
pmap_call_on_active_cpus(pmap_t pmap, void (*fn)(void *), void *arg)
{
	int	cpuid, cpu, self;
	cpuset_t active_cpus;

	sched_pin();
	if (is_kernel_pmap(pmap)) {
		smp_rendezvous(NULL, fn, NULL, arg);
		goto out;
	}
	/* Force ASID update on inactive CPUs */
	CPU_FOREACH(cpu) {
		if (!CPU_ISSET(cpu, &pmap->pm_active))
			pmap->pm_asid[cpu].gen = 0;
	}
	cpuid = PCPU_GET(cpuid);
	/*
	 * XXX: barrier/locking for active?
	 *
	 * Take a snapshot of active here, any further changes are ignored.
	 * tlb update/invalidate should be harmless on inactive CPUs
	 */
	active_cpus = pmap->pm_active;
	self = CPU_ISSET(cpuid, &active_cpus);
	CPU_CLR(cpuid, &active_cpus);
	/* Optimize for the case where this cpu is the only active one */
	if (CPU_EMPTY(&active_cpus)) {
		if (self)
			fn(arg);
	} else {
		if (self)
			CPU_SET(cpuid, &active_cpus);
		smp_rendezvous_cpus(active_cpus, NULL, fn, NULL, arg);
	}
out:
	sched_unpin();
}
#else /* !SMP */
static __inline void
pmap_call_on_active_cpus(pmap_t pmap, void (*fn)(void *), void *arg)
{
	int	cpuid;

	if (is_kernel_pmap(pmap)) {
		fn(arg);
		return;
	}
	cpuid = PCPU_GET(cpuid);
	if (!CPU_ISSET(cpuid, &pmap->pm_active))
		pmap->pm_asid[cpuid].gen = 0;
	else
		fn(arg);
}
#endif /* SMP */

static void
pmap_invalidate_all(pmap_t pmap)
{

	pmap_call_on_active_cpus(pmap,
	    (void (*)(void *))tlb_invalidate_all_user, pmap);
}

struct pmap_invalidate_page_arg {
	pmap_t pmap;
	vm_offset_t va;
};

static void
pmap_invalidate_page_action(void *arg)
{
	struct pmap_invalidate_page_arg *p = arg;

	tlb_invalidate_address(p->pmap, p->va);
}

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	struct pmap_invalidate_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	pmap_call_on_active_cpus(pmap, pmap_invalidate_page_action, &arg);
}

struct pmap_invalidate_range_arg {
	pmap_t pmap;
	vm_offset_t sva;
	vm_offset_t eva;
};

static void
pmap_invalidate_range_action(void *arg)
{
	struct pmap_invalidate_range_arg *p = arg;

	tlb_invalidate_range(p->pmap, p->sva, p->eva);
}

static void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct pmap_invalidate_range_arg arg;

	arg.pmap = pmap;
	arg.sva = sva;
	arg.eva = eva;
	pmap_call_on_active_cpus(pmap, pmap_invalidate_range_action, &arg);
}

struct pmap_update_page_arg {
	pmap_t pmap;
	vm_offset_t va;
	pt_entry_t pte;
};

static void
pmap_update_page_action(void *arg)
{
	struct pmap_update_page_arg *p = arg;

	tlb_update(p->pmap, p->va, p->pte);
}

static void
pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte)
{
	struct pmap_update_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	arg.pte = pte;
	pmap_call_on_active_cpus(pmap, pmap_update_page_action, &arg);
}

static void
pmap_update_pde_invalidate(pmap_t pmap, vm_offset_t va, pt_entry_t newpde)
{

	if (!pte_is_1m_superpage(&newpde)) {
		/* Demotion: flush a specific 2mb page mapping. */
		tlb_invalidate_range(pmap, (va & ~PDRMASK),
		    (va & ~PDRMASK) + NBPDR);
	} else if (!pte_test(&newpde, PTE_G)) {
		/*
		 * Promotion: flush every 4KB page mapping from the TLB
		 * because there are too many to flush individually.
		 */
		tlb_invalidate_all_user(pmap);
	} else {
		/*
		 * Promotion: flush every 4KB page mapping from the TLB,
		 * including any global (PTE_G) mappings.
		 */
		tlb_invalidate_all();
	}
}

struct pmap_update_pde_arg {
	pmap_t pmap;
	vm_offset_t va;
	pd_entry_t *pde;
	pt_entry_t newpde;
};

static void
pmap_update_pde_action(void *act)
{
	struct pmap_update_pde_arg *arg = act;

	pmap_update_pde_invalidate(arg->pmap, arg->va, arg->newpde);
}

static void
pmap_update_pde_store(pmap_t pmap, pd_entry_t *pde, pt_entry_t newpde)
{

	pde_store(pde, newpde);
}


/*
 * Change the page size for the specified virtual address in a way that
 * prevents any possibility of the TLB ever having two entries that map the
 * same virtual address using different page sizes.
 */
static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pt_entry_t newpde)
{
	struct pmap_update_pde_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	arg.pde = pde;
	arg.newpde = newpde;

	pmap_update_pde_store(pmap, pde, newpde);
	pmap_call_on_active_cpus(pmap, pmap_update_pde_action, &arg);
}

/* --- */

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_offset_t pa;

	pa = 0;
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, va);
	if (pde_is_1m_superpage(pde)) {
		pa = TLBLO_PDE_TO_PA(*pde) | (va & PDRMASK);
	} else {
		pte = pmap_pde_to_pte(pde, va);
		if (pte)
			pa = TLBLO_PTE_TO_PA(*pte) | (va & PAGE_MASK);
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
	pd_entry_t *pdep;
	pt_entry_t pte, *ptep;
	vm_paddr_t pa, pte_pa;
	vm_page_t m;
	vm_paddr_t pde_pa;

	pa = 0;
	m = NULL;
	PMAP_LOCK(pmap);
retry:
	pdep = pmap_pde(pmap, va);
	if (pdep != NULL && *pdep != NULL) {
		if (pde_is_1m_superpage(pdep)) {
			if (!pde_test(pdep, PTE_RO) ||
			    (prot & VM_PROT_WRITE) == 0) {
				pde_pa = TLBLO_PDE_TO_PA(*pdep) |
				    (va & PDRMASK);
				if (vm_page_pa_tryrelock(pmap, pde_pa, &pa))
					goto retry;
				m = PHYS_TO_VM_PAGE(pde_pa);
				vm_page_hold(m);
			}
		} else {
			ptep = pmap_pde_to_pte(pdep, va);
			if (ptep != NULL) {
				pte = *ptep;
				if (pte_is_valid(&pte) &&
				    (!pte_test(&pte, PTE_RO) ||
				    (prot & VM_PROT_WRITE) == 0)) {
					pte_pa = TLBLO_PTE_TO_PA(pte);
					if (vm_page_pa_tryrelock(pmap, pte_pa,
					    &pa))
						goto retry;
					m = PHYS_TO_VM_PAGE(pte_pa);
					vm_page_hold(m);
				}
			}
		}
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
}

/*-
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		virtual address.
 */
vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	int mapped;

	/*
	 * First, the direct-mapped regions.
	 */
	if (va >= MIPS_XKPHYS_START && va < MIPS_XKPHYS_END)
		return (MIPS_XKPHYS_TO_PHYS(va));

	if (va >= MIPS_KSEG0_START && va < MIPS_KSEG0_END)
		return (MIPS_KSEG0_TO_PHYS(va));

	if (va >= MIPS_KSEG1_START && va < MIPS_KSEG1_END)
		return (MIPS_KSEG1_TO_PHYS(va));

	/*
	 * User virtual addresses.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		pd_entry_t *pdep;
		pt_entry_t *ptep;

		if (curproc && curproc->p_vmspace) {
			pdep = pmap_pde(&curproc->p_vmspace->vm_pmap, va);
			if (pdep == NULL || *pdep == NULL)
				return (0);
			if (pde_is_1m_superpage(pdep)) {
				ptep = (pt_entry_t *)pdep;
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PDRMASK));
			}
			ptep = pmap_pde_to_pte(pdep, va);
			if (ptep) {
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PAGE_MASK));
			}
			return (0);
		}
	}

	/*
	 * Should be kernel virtual here, otherwise fail
	 */
	mapped = (va >= MIPS_KSEG2_START || va < MIPS_KSEG2_END);
	mapped = mapped || (va >= MIPS_XKSEG_START || va < MIPS_XKSEG_END);
	/*
	 * Kernel virtual.
	 */

	if (mapped) {
		pd_entry_t *pdep;
		pt_entry_t *ptep;

		/* Is the kernel pmap initialized? */
		if (!CPU_EMPTY(&kernel_pmap->pm_active)) {
			/* It's inside the virtual address range */
			pdep = pmap_pde(kernel_pmap, va);
			if (pdep == NULL || *pdep == NULL)
				return (0);
			if (pde_is_1m_superpage(pdep)) {
				ptep = (pt_entry_t *)pdep;
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PDRMASK));
			}
			ptep = pmap_pde_to_pte(pdep, va);
			if (ptep) {
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PAGE_MASK));
			}
		}
		return (0);
	}

	panic("%s for unknown address space %p.", __func__, (void *)va);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*-
 * add a wired page to the kva
 */
void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, vm_memattr_t ma)
{
	pt_entry_t *pte;
	pt_entry_t opte, npte;

#ifdef PMAP_DEBUG
	printf("pmap_kenter:  va: %p -> pa: %p\n", (void *)va, (void *)pa);
#endif

	pte = pmap_pte(kernel_pmap, va);
	opte = *pte;
	npte = TLBLO_PA_TO_PFN(pa) | PTE_C(ma) | PTE_D | PTE_REF | PTE_VALID | PTE_G;
	pte_store(pte, npte);
	if (pte_is_valid(&opte) && opte != npte)
		pmap_update_page(kernel_pmap, va, npte);
}

void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{

	KASSERT(is_cacheable_mem(pa),
		("pmap_kenter: memory at 0x%lx is not cacheable", (u_long)pa));

	pmap_kenter_attr(va, pa, VM_MEMATTR_DEFAULT);
}

/*-
 * remove a page from the kernel pagetables
 */
 /* PMAP_INLINE */ void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	/*
	 * Write back all caches from the page being destroyed
	 */
	mips_dcache_wbinv_range_index(va, PAGE_SIZE);

	pte = pmap_pte(kernel_pmap, va);
	pte_store(pte, PTE_G);
	pmap_invalidate_page(kernel_pmap, va);
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
 *
 *	Use XKPHYS for 64 bit.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{

	return (MIPS_PHYS_TO_DIRECT(start));
}

/*-
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	int i;
	vm_offset_t origva = va;

	for (i = 0; i < count; i++) {
		pmap_flush_pvcache(m[i]);
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
		va += PAGE_SIZE;
	}

	mips_dcache_wbinv_range_index(origva, PAGE_SIZE*count);
}

/*-
 * This routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	pt_entry_t *pte;
	vm_offset_t origva;

	if (count < 1)
		return;
	mips_dcache_wbinv_range_index(va, PAGE_SIZE * count);
	origva = va;
	do {
		pte = pmap_pte(kernel_pmap, va);
		pte_store(pte, PTE_G);
		va += PAGE_SIZE;
	} while (--count > 0);
	pmap_invalidate_range(kernel_pmap, origva, va);
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

/*-
 * Schedule the specified unused page table page to be freed.  Specifically
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
	return (vm_radix_lookup(&pmap->pm_root, pmap_pde_pindex(va)));
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
 * Decrements a page table page's wire count, which is used to record the
 * number of valid page table entries within the page.  If the wire count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static PMAP_INLINE boolean_t
pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	--m->wire_count;
	if (m->wire_count == 0) {
		_pmap_unwire_ptp(pmap, va, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{
	pd_entry_t *pde, *pdp;
	vm_page_t pdpg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * unmap the page table page
	 */
	if (m->pindex < NUPDE) {
		pde = pmap_pde(pmap, va);
		*pde = 0;
		pmap_resident_count_dec(pmap, 1);

		/*
		 * Recursively decrement next level pagetable refcount
		 */
		pdp = (pd_entry_t *)*pmap_segmap(pmap, va);
		pdpg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(pdp));
		pmap_unwire_ptp(pmap, va, pdpg, free);
	} else {
		pde = pmap_segmap(pmap, va);
		*pde = 0;
		pmap_resident_count_dec(pmap, 1);
	}

	/*
	 * If the page is finally unwired, simply free it.
	 * This is a release store so that the ordinary store unmapping
	 * the page table page is globally performed before TLB shoot-
	 * down is begun.
	 */
	atomic_subtract_int(&vm_cnt.v_wire_count, 1);

	/*
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done.
	 */
	pmap_add_delayed_free_list(m, free, TRUE);
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, pd_entry_t ptepde,
    struct spglist *free)
{
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	KASSERT(ptepde != 0, ("pmap_unuse_pt: ptepde != 0"));
	mpte = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(ptepde));
	return (pmap_unwire_ptp(pmap, va, mpte, free));
}

void
pmap_pinit0(pmap_t pmap)
{
	int i;

	PMAP_LOCK_INIT(pmap);
	pmap->pm_segtab = kernel_segmap;
	CPU_ZERO(&pmap->pm_active);
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asid[i].asid = PMAP_ASID_RESERVED;
		pmap->pm_asid[i].gen = 0;
	}
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*-
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit(pmap_t pmap)
{
	vm_offset_t ptdva;
	vm_page_t ptdpg;
	int i;

	/*
	 * allocate the page directory page
	 */
	while ((ptdpg = vm_page_alloc(NULL, NUSERPGTBLS, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL)
		VM_WAIT;
	if ((ptdpg->flags & PG_ZERO) == 0)
		pmap_zero_page(ptdpg);

	ptdva = MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(ptdpg));
	pmap->pm_segtab = (pd_entry_t *)ptdva;
	CPU_ZERO(&pmap->pm_active);
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asid[i].asid = PMAP_ASID_RESERVED;
		pmap->pm_asid[i].gen = 0;
	}
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

	return (1);
}

/*
 * This routine is called if the desired page table page does not exist.
 */
static vm_page_t
_pmap_allocpte(pmap_t pmap, unsigned ptepindex, struct rwlock **lockp)
{
	vm_offset_t pageva;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Find or fabricate a new pagetable page
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if (lockp != NULL) {
			RELEASE_PV_LIST_LOCK(lockp);
			PMAP_UNLOCK(pmap);
			rw_runlock(&pvh_global_lock);
			VM_WAIT;
			rw_rlock(&pvh_global_lock);
			PMAP_LOCK(pmap);
		}

		/*
		 * Indicate the need to retry.	While waiting, the page
		 * table page may have been allocated.
		 */
		return (NULL);
	}
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	/*
	 * Map the pagetable page into the process address space, if it
	 * isn't already there.
	 */
	pageva = MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));

	if (ptepindex >= NUPDE) {
		pmap->pm_segtab[ptepindex - NUPDE] = (pd_entry_t)pageva;
	} else {
		pd_entry_t *pdep, *pde;
		int segindex = ptepindex >> (SEGSHIFT - PDRSHIFT);
		int pdeindex = ptepindex & (NPDEPG - 1);
		vm_page_t pg;

		pdep = &pmap->pm_segtab[segindex];
		if (*pdep == NULL) {
			/* Have to allocate a new pd, recurse */
			if (_pmap_allocpte(pmap, NUPDE + segindex,
			    lockp) == NULL) {
				/* alloc failed, release current */
				--m->wire_count;
				atomic_subtract_int(&vm_cnt.v_wire_count, 1);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			/* Add reference to the pd page */
			pg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(*pdep));
			pg->wire_count++;
		}
		/* Next level entry */
		pde = (pd_entry_t *)*pdep;
		pde[pdeindex] = (pd_entry_t)pageva;
	}

	pmap_resident_count_inc(pmap, 1);

	return (m);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	unsigned ptepindex;
	pd_entry_t *pd;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_pde_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	pd = pmap_pde(pmap, va);

	/*
	 * This supports switching from a 2MB page to a
	 * normal 4K page.
	 */
	if (pd != NULL && (pde_is_1m_superpage(pd) &&
	    pte_is_valid((pt_entry_t *)pd))) {
		 if (!pmap_demote_pde_locked(pmap, pd, va, lockp)) {
			 /*
			  * Invalidation of the 2MB page mapping may have caused
			  * the deallocation of the underlying PD page.
			  */
			 pd = NULL;
		 }
	}

	/*
	 * If the page table page is mapped, we just increment the hold
	 * count, and activate it.
	 */
	if (pd != NULL && *pd != NULL) {
		m = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS((pt_entry_t)*pd));
		m->wire_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		m = _pmap_allocpte(pmap, ptepindex, lockp);
		if (m == NULL && lockp != NULL)
			goto retry;
	}
	return (m);
}

/***************************************************
 * Pmap allocation/deallocation routines.
 ***************************************************/

/*-
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	vm_offset_t ptdva;
	vm_page_t ptdpg;

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	/*
	 * Invalidate any left TLB entries, to allow the reuse
	 * of the asid.
	 */
	pmap_invalidate_all(pmap);

	ptdva = (vm_offset_t)pmap->pm_segtab;
	ptdpg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(ptdva));

	ptdpg->wire_count--;
	atomic_subtract_int(&vm_cnt.v_wire_count, 1);
	vm_page_free_zero(ptdpg);
}

/*-
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_page_t nkpg;
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;
	int i;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
	addr = roundup2(addr, NBSEG);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		pdpe = pmap_segmap(kernel_pmap, kernel_vm_end);
		if (*pdpe == 0) {
			/* new intermediate page table entry */
			nkpg = vm_page_alloc(NULL, nkpt, VM_ALLOC_INTERRUPT |
			    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (nkpg == NULL)
				panic("%s: no memory to grow kernel", __func__);
			*pdpe = (pd_entry_t)MIPS_PHYS_TO_DIRECT(
			    VM_PAGE_TO_PHYS(nkpg));
			continue; /* try again */
		}
		pde = pmap_pdpe_to_pde(pdpe, kernel_vm_end);
		if (*pde != 0) {
			kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
			continue;
		}

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = vm_page_alloc(NULL, nkpt, VM_ALLOC_INTERRUPT |
		    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");
		nkpt++;
		*pde = (pd_entry_t)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(nkpg));

		/*
		 * The R[4-7]?00 stores only one copy of the Global bit in
		 * the translation lookaside buffer for each 2 page entry.
		 * Thus invalid entrys must have the Global bit set so when
		 * Entry LO and Entry HI G bits are anded together they will
		 * produce a global bit to store in the tlb.
		 */
		pte = (pt_entry_t *)*pde;
		for (i = 0; i < NPTEPG; i++)
			pte[i] = PTE_G;

		kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
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

static const u_long pc_freemask[_NPCM] = { PC_FREE0, PC_FREE1, PC_FREE2 };

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

static long pv_entry_count, pv_entry_frees, pv_entry_allocs;
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

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 *
 * Returns NULL if PV entries were reclaimed from the specified pmap.
 *
 * We do not, however, unmap 2mpages because subsequent access will
 * allocate per-page pv entries until repromotion occurs, thereby
 * exacerbating the shortage of free pv entries.
 */
static vm_page_t
reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp)
{
	struct pch new_tail;
	struct pv_chunk *pc;
	pd_entry_t *pde;
	pmap_t pmap;
	pt_entry_t *pte, oldpte;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m, m_pc;
	struct spglist free;
	uint64_t inuse;
	int bit, field, freed, idx;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reclaim_pv_chunk: lockp is NULL"));
	pmap = NULL;
	m_pc = NULL;
	SLIST_INIT(&free);
	TAILQ_INIT(&new_tail);
	mtx_lock(&pv_chunks_mutex);
	while ((pc = TAILQ_FIRST(&pv_chunks)) != NULL && SLIST_EMPTY(&free)) {
		TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
		mtx_unlock(&pv_chunks_mutex);
		if (pmap != pc->pc_pmap) {
			if (pmap != NULL) {
				pmap_invalidate_all(pmap);
				if (pmap != locked_pmap)
					PMAP_UNLOCK(pmap);
			}
			pmap = pc->pc_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap) {
				RELEASE_PV_LIST_LOCK(lockp);
				PMAP_LOCK(pmap);
			} else if (pmap != locked_pmap && !PMAP_TRYLOCK(pmap)) {
				pmap = NULL;
				TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
				mtx_lock(&pv_chunks_mutex);
				continue;
			}
		}

		/*
		 * Destroy every non-wired, 4 KB page mapping in the chunk.
		 */
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			for (inuse = ~pc->pc_map[field] & pc_freemask[field];
			    inuse != 0; inuse &= ~(1UL << bit)) {
				bit = ffsl(inuse) - 1;
				idx = field * sizeof(inuse) * NBBY + bit;
				pv = &pc->pc_pventry[idx];
				va = pv->pv_va;
				pde = pmap_pde(pmap, va);
				KASSERT(pde != NULL && *pde != 0,
				    ("%s: pde", __func__));
				if (pde_is_1m_superpage(pde))
					continue;
				pte = pmap_pde_to_pte(pde, va);
				oldpte = *pte;
				if (pte_test(&oldpte, PTE_W))
					continue;
				if (is_kernel_pmap(pmap))
					*pte = PTE_G;
				else
					*pte = 0;
				if (pte_test(&oldpte, PTE_G))
					pmap_invalidate_page(pmap, va);
				m = PHYS_TO_VM_PAGE(TLBLO_PTE_TO_PA(oldpte));
				if (pte_test(&oldpte, PTE_D))
					vm_page_dirty(m);
				if (pte_is_ref(&oldpte))
					vm_page_aflag_set(m, PGA_REFERENCED);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
				m->md.pv_gen++;
				if (TAILQ_EMPTY(&m->md.pv_list) &&
				    (m->flags & PG_FICTITIOUS) == 0) {
					struct md_page *pvh =
					    pa_to_pvh(VM_PAGE_TO_PHYS(m));
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						vm_page_aflag_clear(m,
						    PGA_WRITEABLE);
					}
				}
				pc->pc_map[field] |= 1UL << bit;
				pmap_unuse_pt(pmap, va, *pde, &free);
				freed++;
			}
		}
		if (freed == 0) {
			TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
			mtx_lock(&pv_chunks_mutex);
			continue;
		}
		/* Every freed mapping is for a 4 KB page. */
		pmap_resident_count_dec(pmap, freed);
		PV_STAT(atomic_add_long(&pv_entry_frees, freed));
		PV_STAT(atomic_add_int(&pv_entry_spare, freed));
		PV_STAT(atomic_subtract_long(&pv_entry_count, freed));
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		if (pc->pc_map[0] == PC_FREE0 && pc->pc_map[1] == PC_FREE1 &&
		    pc->pc_map[2] == PC_FREE2) {
			PV_STAT(atomic_subtract_int(&pv_entry_spare, _NPCPV));
			PV_STAT(atomic_subtract_int(&pc_chunk_count, 1));
			PV_STAT(atomic_add_int(&pc_chunk_frees, 1));
			/* Entire chunk is free; return it. */
			m_pc = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(
			    (vm_offset_t)pc));
			dump_drop_page(m_pc->phys_addr);
			mtx_lock(&pv_chunks_mutex);
			break;
		}
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
		mtx_lock(&pv_chunks_mutex);
		/* One freed pv entry in locked_pmap is sufficient. */
		if (pmap == locked_pmap)
			break;
	}
	TAILQ_CONCAT(&pv_chunks, &new_tail, pc_lru);
	mtx_unlock(&pv_chunks_mutex);
	if (pmap != NULL) {
		pmap_invalidate_all(pmap);
		if (pmap != locked_pmap)
			PMAP_UNLOCK(pmap);
	}
	if (m_pc == NULL && !SLIST_EMPTY(&free)) {
		m_pc = SLIST_FIRST(&free);
		SLIST_REMOVE_HEAD(&free, plinks.s.ss);
		/* Recycle a freed page table page. */
		m_pc->wire_count = 1;
		atomic_add_int(&vm_cnt.v_wire_count, 1);
	}
	pmap_free_zero_pages(&free);
	return (m_pc);
}

/*
 * free the pv_entry back to the free list
 */
static void
free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int bit, field, idx;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(atomic_add_long(&pv_entry_frees, 1));
	PV_STAT(atomic_add_int(&pv_entry_spare, 1));
	PV_STAT(atomic_subtract_long(&pv_entry_count, 1));
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / (sizeof(u_long) * NBBY);
	bit = idx % (sizeof(u_long) * NBBY);
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
	m = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS((vm_offset_t)pc));
	dump_drop_page(m->phys_addr);
	vm_page_unwire(m, PQ_INACTIVE);
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
	int bit, field, idx;
	pv_entry_t pv;
	struct pv_chunk *pc;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_LOCKED);
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
			idx = field * sizeof(pc->pc_map[field]) * NBBY + bit;
			pv = &pc->pc_pventry[idx];
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
	pc = (struct pv_chunk *)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));
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
 * Returns the number of one bits within the given PV chunk map element.
 */
static inline int
popcount_pc_map_elem(uint64_t elem)
{
	int count;

	/*
	 * This simple method of counting the one bits performs well because
	 * the given element typically contains more zero bits than one bits.
	 */
	count = 0;
	for (; elem != 0; elem &= elem - 1)
		count++;
	return (count);
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

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("%s: lockp is NULL", __func__));

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
		free = popcount_pc_map_elem(pc->pc_map[0]);
		free += popcount_pc_map_elem(pc->pc_map[1]);
		free += popcount_pc_map_elem(pc->pc_map[2]);
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
		pc = (struct pv_chunk *)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));
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
static pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_LOCKED);
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
pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	struct pv_chunk *pc;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;
	int bit, field;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((pa & PDRMASK) == 0,
	    ("%s: pa is not 2mpage aligned", __func__));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the 2mpage's pv entry for this mapping to the first
	 * page's pv list.  Once this transfer begins, the pv list lock
	 * must not be released until the last pv entry is reinstantiated.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("%s: pv not found", __func__));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
	m->md.pv_gen++;
	/* Instantiate the remaining NPTEPG - 1 pv entries. */
	PV_STAT(atomic_add_long(&pv_entry_allocs, NPTEPG - 1));
	va_last = va + NBPDR - PAGE_SIZE;
	for (;;) {
		pc = TAILQ_FIRST(&pmap->pm_pvchunk);
		KASSERT(pc->pc_map[0] != 0 || pc->pc_map[1] != 0 ||
		    pc->pc_map[2] != 0, ("%s: missing spare", __func__));
		for (field = 0; field < _NPCM; field++) {
			while (pc->pc_map[field]) {
				bit = ffsl(pc->pc_map[field]) - 1;
				pc->pc_map[field] &= ~(1ul << bit);
				pv = &pc->pc_pventry[field * 64 + bit];
				va += PAGE_SIZE;
				pv->pv_va = va;
				m++;
				KASSERT((m->oflags & VPO_UNMANAGED) == 0,
				   ("%s: page %p is not managed", __func__, m));
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
	PV_STAT(atomic_add_long(&pv_entry_count, NPTEPG - 1));
	PV_STAT(atomic_subtract_int(&pv_entry_spare, NPTEPG - 1));
}

/*
 * After promotion from 512 4KB page mappings to a single 2MB page mapping,
 * replace the many pv entries for the 4KB page mappings by a single pv entry
 * for the 2MB page mapping.
 */
static void
pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	KASSERT((pa & PDRMASK) == 0,
	    ("%s: pa is not 2mpage aligned", __func__));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the first page's pv entry for this mapping to the 2mpage's
	 * pv list.  Aside from avoiding the cost of a call to get_pv_entry(),
	 * a transfer avoids the possibility that get_pv_entry() calls
	 * reclaim_pv_chunk() and that reclaim_pv_chunk() removes one of the
	 * mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("%s: pv not found", __func__));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
	pvh->pv_gen++;
	/* Free the remaining NPTEPG - 1 pv entries. */
	va_last = va + NBPDR - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
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
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found, pa %lx va %lx",
	     (u_long)VM_PAGE_TO_PHYS(__containerof(pvh, struct vm_page, md)),
	     (u_long)va));
	free_pv_entry(pmap, pv);
}

/*
 * Conditionally create the pv entry for a 4KB page mapping if the required
 * memory can be allocated without restorting to reclamation.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct rwlock **lockp)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
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
 * Conditionally create the PV entry for a 2MB page mapping if the required
 * memory can be allocated without resorting to reclamation.
 */
static boolean_t
pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, NULL)) != NULL) {
		pv->pv_va = va;
		CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);
		pvh = pa_to_pvh(pa);
		TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
		pvh->pv_gen++;
		return (TRUE);
	} else
		return (FALSE);
}


/*
 * Fills a page table page with mappings to consecutive physical pages.
 */
static void
pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte)
{
	pt_entry_t *pte;

	for (pte = firstpte; pte < firstpte + NPTEPG; pte++) {
		*pte = newpte;
		newpte += (PAGE_SIZE >> TLBLO_PFN_SHIFT);
	}
}

/*
 * Tries to demote a 2MB page mapping.  If demotion fails, the 2MB page
 * mapping is invalidated.
 */
static boolean_t
pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	struct rwlock *lock;
	boolean_t rv;

	lock = NULL;
	rv = pmap_demote_pde_locked(pmap, pde, va, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	return (rv);
}

static boolean_t
pmap_demote_pde_locked(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp)
{
	 pd_entry_t newpde, oldpde;
	 pt_entry_t oldpte, *firstpte, newpte;
	 vm_paddr_t mptepa;
	 vm_page_t mpte;
	 struct spglist free;

	 PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	 oldpde = *pde;
	 oldpte = (pt_entry_t)oldpde;
	 KASSERT(pte_is_1m_superpage(&oldpte) && pte_is_valid(&oldpte),
	     ("%s: oldpde is not superpage and/or valid.", __func__));
	 if (pte_is_ref(&oldpte) && (mpte = pmap_lookup_pt_page(pmap, va)) !=
	     NULL)
		 pmap_remove_pt_page(pmap, mpte);
	 else {
		 KASSERT(!pte_test(&oldpte, PTE_W),
		     ("%s: page table page for a wired mapping is missing",
		     __func__));
		 /*
		  * Invalidate the 2MB page mapping and return "failure" if the
		  * mapping was never accessed or the allocation of the new
		  * page table page fails.  If the 2MB page mapping belongs to
		  * the direct map region of the kernel's address space, then
		  * the page allocation request specifies the highest possible
		  * priority (VM_ALLOC_INTERRUPT).  Otherwise, the priority is
		  * normal.  Page table pages are preallocated for every other
		  * part of the kernel address space, so the direct map region
		  * is the only part of the kernel address space that must be
		  * handled here.
		  */
		 if (!pte_is_ref(&oldpte) || (mpte = vm_page_alloc(NULL,
		     pmap_pde_pindex(va), (va >= VM_MIN_KERNEL_ADDRESS && va <
		     VM_MAX_ADDRESS ? VM_ALLOC_INTERRUPT : VM_ALLOC_NORMAL) |
		     VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
			 SLIST_INIT(&free);
			 pmap_remove_pde(pmap, pde, trunc_2mpage(va), &free,
			     lockp);
			 pmap_invalidate_range(pmap,
			     (vm_offset_t)(va & ~PDRMASK),
			     (vm_offset_t)(va & ~PDRMASK) + NBPDR);
			 pmap_free_zero_pages(&free);
			 CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
			     __func__, va, pmap);
			 return (FALSE);
		 }
		 if (va < VM_MAXUSER_ADDRESS)
			 pmap_resident_count_inc(pmap, 1);
	 }
	 mptepa = VM_PAGE_TO_PHYS(mpte);
	 newpde = (pd_entry_t)MIPS_PHYS_TO_DIRECT(mptepa);
	 firstpte = newpde;
	 KASSERT(pte_is_ref(&oldpte),
	     ("%s: oldpte is not referenced", __func__));
	 KASSERT(!pte_test(&oldpte, PTE_RO) && pte_test(&oldpte, PTE_D),
	     ("%s: oldpte is missing PTE_D", __func__));
	 newpte = oldpte & ~PTE_PS_IDX_MASK;

	 /*
	  * If the page table page is new, initialize it.
	  */
	 if (mpte->wire_count == 1) {
		 mpte->wire_count = NPTEPG;
		 pmap_fill_ptp(firstpte, newpte);
	 }
	 KASSERT(TLBLO_PTE_TO_PA(*firstpte) == TLBLO_PTE_TO_PA(newpte),
	     ("%s: firstpte and newpte map different physical addresses",
	     __func__));

	 /*
	  * If the mapping has changed attributes, update the page table
	  * entries.
	  */
	 if ((*firstpte & PG_PROMOTE_MASK) != (newpte & PG_PROMOTE_MASK))
		 pmap_fill_ptp(firstpte, newpte);


	 /*
	  * The spare PV entries must be reserved prior to demoting the
	  * mapping, that is, prior to changing the PDE.  Otherwise, the state
	  * of the PDE and the PV lists will be inconsistent, which can result
	  * in reclaim_pv_chunk() attempting to remove a PV entry from the
	  * wrong PV list and pmap_pv_demote_pde() failing to find the expected
	  * PV entry for the 2MB page mapping that is being demoted.
	  */
	 if (pde_test(&oldpde, PTE_MANAGED))
		 reserve_pv_entries(pmap, NPTEPG - 1, lockp);

	 /*
	  * Demote the mapping.  This pmap is locked.  The old PDE has
	  * PTE_REF set.  If the old PDE has PTE_RO clear, it also has
	  * PTE_D set.  Thus, there is no danger of a race with another
	  * processor changing the setting of PTE_REF and/or PTE_D between
	  * the read above and the store below.
	  */
	 pmap_update_pde(pmap, va, pde, (pt_entry_t)newpde);

	 /*
	  * Invalidate a stale recursive mapping of the page table page.
	  */
	 if (va >= VM_MAXUSER_ADDRESS)
		 pmap_invalidate_page(pmap, (vm_offset_t)pmap_pte(pmap, va));

	 /*
	  * Demote the PV entry.
	  */
	 if (pde_test(&oldpde, PTE_MANAGED)) {
		 pmap_pv_demote_pde(pmap, va, TLBLO_PDE_TO_PA(oldpde), lockp);
	 }
	 atomic_add_long(&pmap_pde_demotions, 1);
	 CTR3(KTR_PMAP, "%s: success for va %#lx in pmap %p", __func__, va,
	     pmap);

	 return (TRUE);
}

/*
 * pmap_remove_kernel_pde: Remove a kernel superpage mapping.
 */
static void
pmap_remove_kernel_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	/* XXX Not doing kernel superpages yet. */
	panic("pmap_remove_kernel_pde: kernel superpage");
}

/*
 * pmap_remove_pde: do the things to unmap a superpage in a process
 */
static int
pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pd_entry_t oldpde;
	vm_offset_t eva, va;
	vm_page_t m, mpte;

	/*
	 * Write back all cache lines from the superpage being unmapped.
	 */
	mips_dcache_wbinv_range_index((sva & ~PDRMASK), NBPDR);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_remove_pde: sva is not 2mpage aligned"));

	if (is_kernel_pmap(pmap))
		oldpde = pde_load_store(pdq, PTE_G);
	else
		oldpde = pde_load_store(pdq, 0);
	if (pde_test(&oldpde, PTE_W))
		pmap->pm_stats.wired_count -= NBPDR / PAGE_SIZE;

	if (pde_test(&oldpde, PTE_G))
		pmap_invalidate_page(kernel_pmap, sva);

	pmap_resident_count_dec(pmap, NBPDR / PAGE_SIZE);
	if (pde_test(&oldpde, PTE_MANAGED)) {
	    CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, TLBLO_PDE_TO_PA(oldpde));
	    pvh = pa_to_pvh(TLBLO_PDE_TO_PA(oldpde));
	    pmap_pvh_free(pvh, pmap, sva);
	    eva = sva + NBPDR;
	    for (va = sva, m = PHYS_TO_VM_PAGE(TLBLO_PDE_TO_PA(oldpde));
		va < eva; va += PAGE_SIZE, m++) {
		    if (pde_test(&oldpde, PTE_D) && !pde_test(&oldpde, PTE_RO))
			    vm_page_dirty(m);
		    if (pde_test(&oldpde, PTE_REF))
			vm_page_aflag_set(m, PGA_REFERENCED);
		    if (TAILQ_EMPTY(&m->md.pv_list) &&
			TAILQ_EMPTY(&pvh->pv_list))
			    vm_page_aflag_clear(m, PGA_WRITEABLE);
	    }
	}
	if (pmap == kernel_pmap) {
		pmap_remove_kernel_pde(pmap, pdq, sva);
	} else {
		mpte = pmap_lookup_pt_page(pmap, sva);
		if (mpte != NULL) {
			pmap_remove_pt_page(pmap, mpte);
			pmap_resident_count_dec(pmap, 1);
			KASSERT(mpte->wire_count == NPTEPG,
			    ("pmap_remove_pde: pte page wire count error"));
			mpte->wire_count = 0;
			pmap_add_delayed_free_list(mpte, free, FALSE);
			atomic_subtract_int(&vm_cnt.v_wire_count, 1);
		}
	}
	return (pmap_unuse_pt(pmap, sva, *pmap_segmap(pmap, sva), free));
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va,
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pt_entry_t oldpte;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Write back all cache lines from the page being unmapped.
	 */
	mips_dcache_wbinv_range_index(va, PAGE_SIZE);

	oldpte = *ptq;
	if (is_kernel_pmap(pmap))
		*ptq = PTE_G;
	else
		*ptq = 0;

	if (pte_test(&oldpte, PTE_W))
		pmap->pm_stats.wired_count -= 1;

	pmap_resident_count_dec(pmap, 1);
	if (pte_test(&oldpte, PTE_MANAGED)) {
		m = PHYS_TO_VM_PAGE(TLBLO_PTE_TO_PA(oldpte));
		if (pte_test(&oldpte, PTE_D)) {
			KASSERT(!pte_test(&oldpte, PTE_RO),
			   ("%s: modified page not writable: va: %p, pte: %#jx",
			    __func__, (void *)va, (uintmax_t)oldpte));
			vm_page_dirty(m);
		}
		if (pte_is_ref(&oldpte))
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
	return (pmap_unuse_pt(pmap, va, ptepde, free));
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(struct pmap *pmap, vm_offset_t va, struct spglist *free)
{
	struct rwlock *lock;
	pd_entry_t *pde;
	pt_entry_t *pte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pde = pmap_pde(pmap, va);
	if (pde == NULL || *pde == 0)
		return;
	pte = pmap_pde_to_pte(pde, va);

	/*
	 * If there is no pte for this address, just skip it!
	 */
	if (!pte_is_valid(pte))
		return;

	lock = NULL;
	(void)pmap_remove_pte(pmap, pte, va, *pde, free, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_page(pmap, va);
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
	pd_entry_t ptpaddr, *pde, *pdpe;
	pt_entry_t *pte;
	struct spglist free;
	int anyvalid;

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = 0;
	SLIST_INIT(&free);

	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);

	/*
	 * special handling of removing one page.  a very common operation
	 * and easy to short circuit some code.
	 */
	if ((sva + PAGE_SIZE) == eva) {
		pde = pmap_pde(pmap, sva);
		if (!pde_is_1m_superpage(pde)) {
			pmap_remove_page(pmap, sva, &free);
			goto out;
		}
	}

	lock = NULL;
	for (; sva < eva; sva = va_next) {
		if (pmap->pm_stats.resident_count == 0)
			break;

		pdpe = pmap_segmap(pmap, sva);
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		/*
		 * Calculate index for next page table.
		 */
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		ptpaddr = *pde;

		/*
		 * Weed out invalid mappings.
		 */
		if (ptpaddr == NULL)
			continue;
		/*
		 * Check for superpage.
		 */
		if (pde_is_1m_superpage(&ptpaddr)) {
			/*
			 * Are we removing the entire superpage?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				/*
				 * The TLB entry for a PTE_G mapping is
				 * invalidated by pmap_remove_pde().
				 */
				if (!pde_test(&ptpaddr, PTE_G))
					anyvalid = TRUE;
				pmap_remove_pde(pmap, pde, sva, &free, &lock);
				continue;
			} else if (!pmap_demote_pde_locked(pmap, pde, sva,
			    &lock)) {
				/* The superpage mapping was destroyed. */
				continue;
			} else {
				ptpaddr = *pde;
			}
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if (!pte_is_valid(pte)) {
				if (va != va_next) {
					pmap_invalidate_range(pmap, va, sva);
					va = va_next;
				}
				continue;
			}
			if (!pte_test(pte, PTE_G))
				anyvalid = TRUE;
			if (va == va_next)
				va = sva;
			if (pmap_remove_pte(pmap, pte, sva, ptpaddr, &free,
			    &lock)) {
				sva += PAGE_SIZE;
				break;
			}
		}
		if (va != va_next)
			pmap_invalidate_range(pmap, va, sva);
	}
	if (lock != NULL)
		rw_wunlock(lock);
out:
	if (anyvalid)
		pmap_invalidate_all(pmap);
	rw_runlock(&pvh_global_lock);
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
	pt_entry_t *pte, tpte;
	pd_entry_t *pde;
	vm_offset_t va;
	struct spglist free;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	rw_wlock(&pvh_global_lock);
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		(void)pmap_demote_pde(pmap, pde, va);
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);

		/*
		 * If it's last mapping writeback all caches from
		 * the page being destroyed
		 */
		if (TAILQ_NEXT(pv, pv_next) == NULL)
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);

		pmap_resident_count_dec(pmap, 1);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT(pde != NULL && *pde != 0, ("pmap_remove_all: pde"));
		KASSERT(!pde_is_superpage(pde), ("pmap_remove_all: found"
		    " a superpage in page %p 's pv list", m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);

		tpte = *pte;
		pte_store(pte, is_kernel_pmap(pmap) ? PTE_G : 0);

		if (pte_test(&tpte, PTE_W))
			pmap->pm_stats.wired_count--;

		/*
		 * Update the vm_page_t dirty and reference bits.
		 */
		if (pte_is_ref(&tpte))
			vm_page_aflag_set(m, PGA_REFERENCED);
		if (pte_test(&tpte, PTE_D)) {
			KASSERT(!pte_test(&tpte, PTE_RO),
			    ("%s: modified page not writable: va: %p, pte: %#jx"
			    , __func__, (void *)pv->pv_va, (uintmax_t)tpte));
			vm_page_dirty(m);
		}
		pmap_unuse_pt(pmap, pv->pv_va, *pde, &free);
		pmap_invalidate_page(pmap, pv->pv_va);

		TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}

	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(&pvh_global_lock);
	pmap_free_zero_pages(&free);
}

/*
 * pmap_protect_pde: do the things to protect a superpage in a process
 */
static boolean_t
pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva, vm_prot_t prot)
{
	pd_entry_t newpde, oldpde;
	vm_offset_t eva, va;
	vm_page_t m;
	boolean_t anychanged;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("%s: sva is not 2mpage aligned", __func__));
	anychanged = FALSE;
retry:
	oldpde = newpde = *pde;
	if (pde_test(&oldpde, PTE_MANAGED)) {
		eva = sva + NBPDR;
		for (va = sva, m = PHYS_TO_VM_PAGE(TLBLO_PDE_TO_PA(oldpde));
		    va < eva; va += PAGE_SIZE, m++)
			if (pde_test(&oldpde, PTE_D) &&
			    !pde_test(&oldpde, PTE_RO))
				vm_page_dirty(m);
	}
	if ((prot & VM_PROT_WRITE) == 0) {
		pde_set(&newpde, PTE_RO);
		pde_clear(&newpde, PTE_D);
	}
	if (newpde != oldpde) {
		if (!atomic_cmpset_long((pt_entry_t *)pde, (pt_entry_t)oldpde,
		    (pt_entry_t)newpde))
			goto retry;
		if (pde_test(&oldpde, PTE_G))
			pmap_invalidate_page(pmap, sva);
		else
			anychanged = TRUE;
	}
	return (anychanged);
}

/*-
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va, va_next;
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;
	boolean_t anychanged, pv_lists_locked;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	pv_lists_locked = FALSE;
resume:
	anychanged = FALSE;

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pdpe = pmap_segmap(pmap, sva);
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);

		/*
		 * Weed out invalid mappings.
		 */
		if (*pde == NULL)
			continue;

		/*
		 * Check for superpage.
		 */
		if (pde_is_1m_superpage(pde)) {
			/*
			 * Are we protecting the entire superpage? If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				/*
				 * The TLB entry for a PG_G mapping is
				 * invalidated by pmap_protect_pde().
				 */
				if (pmap_protect_pde(pmap, pde, sva, prot))
					anychanged = TRUE;
				continue;
			} else {
				if (!pv_lists_locked) {
					pv_lists_locked = TRUE;
					if (!rw_try_rlock(&pvh_global_lock)) {
						if (anychanged)
							pmap_invalidate_all(
							    pmap);
						PMAP_UNLOCK(pmap);
						rw_rlock(&pvh_global_lock);
						goto resume;
					}
				}
				if (!pmap_demote_pde(pmap, pde, sva)) {
					/*
					 * The superpage mapping was destroyed.
					 */
					continue;
				}
			}
		}
		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being write protected.
		 */
		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			pt_entry_t pbits;
			vm_page_t m;
			vm_paddr_t pa;

			pbits = *pte;
			if (!pte_is_valid(&pbits) || pte_test(&pbits, PTE_RO)) {
				if (va != va_next) {
					pmap_invalidate_range(pmap, va, sva);
					va = va_next;
				}
				continue;
			}
			pte_set(&pbits, PTE_RO);
			if (pte_test(&pbits, PTE_D)) {
				pte_clear(&pbits, PTE_D);
				if (pte_test(&pbits, PTE_MANAGED)) {
					pa = TLBLO_PTE_TO_PA(pbits);
					m = PHYS_TO_VM_PAGE(pa);
					vm_page_dirty(m);
				}
				if (va == va_next)
					va = sva;
			} else {
				/*
				 * Unless PTE_D is set, any TLB entries
				 * mapping "sva" don't allow write access, so
				 * they needn't be invalidated.
				 */
				if (va != va_next) {
					pmap_invalidate_range(pmap, va, sva);
					va = va_next;
				}
			}
			*pte = pbits;
		}
		if (va != va_next)
			pmap_invalidate_range(pmap, va, sva);
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	if (pv_lists_locked) {
		rw_runlock(&pvh_global_lock);
	}
	PMAP_UNLOCK(pmap);
}

/*-
 * Tries to promote the 512, contiguous 4KB page mappings that are within a
 * single page table page to a single 2MB page mapping.  For promotion to
 * occur, two conditions must be met: (1) the 4KB page mappings must map
 * aligned, contiguous physical memory and (2) the 4KB page mappings must have
 * identical characteristics.
 *
 * On MIPS64 promotions are actually done in pairs of two 1MB superpages
 * because of the hardware architecture (two physical pages are in a single
 * TLB entry) even though VM layer is under the impression that the superpage
 * size is actually 2MB.
 */
static void
pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp)
{
	pt_entry_t newpde;
	pt_entry_t *firstpte, oldpte, pa, *pte;
	vm_offset_t oldpteva;
	vm_page_t mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * Examine the first PTE in the specified PTP.  Abort if this PTE is
	 * either invalid, unused, or does not map the first 4KB physical page
	 * within a 2MB page.
	 */
	firstpte = pmap_pte(pmap, trunc_2mpage(va));
setpde:
	newpde = *firstpte;
	if (is_kernel_pmap(pmap)) {
		/* XXX not doing kernel pmap yet */
		atomic_add_long(&pmap_pde_p_failures, 1);
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
		    __func__, va, pmap);
		return;
	}
	if (!pte_is_valid(&newpde) || !pte_is_ref(&newpde)) {
		atomic_add_long(&pmap_pde_p_failures, 1);
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
		    __func__, va, pmap);
		return;
	}
	if (!pte_test(&newpde, PTE_D) && !pte_test(&newpde, PTE_RO)) {
		/*
		 * When PTE_D is already clear, PTE_RO can be set without
		 * a TLB invalidation.
		 */
		if (!atomic_cmpset_long((u_long *)firstpte, newpde, newpde |
		    PTE_RO))
			goto setpde;
		newpde |= PTE_RO;
	}

	/*
	 * Examine each of the other PTEs in the specified PTP.  Abort if this
	 * PTE maps an unexpected 4KB physical page or does not have identical
	 * characteristics to the first PTE.
	 */
	pa = TLBLO_PTE_TO_PA(newpde) + NBPDR - PAGE_SIZE;
	for (pte = firstpte + NPTEPG - 1; pte > firstpte; pte--) {
setpte:
		oldpte = *pte;
		if (TLBLO_PTE_TO_PA(oldpte) != pa) {
			atomic_add_long(&pmap_pde_p_failures, 1);
			CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
			    __func__, va, pmap);
			return;
		}
		if (!pte_test(&oldpte, PTE_D) && !pte_test(&oldpte, PTE_RO)) {
			if (!atomic_cmpset_long(pte, oldpte, oldpte | PTE_RO))
				goto setpte;
			oldpte |= PTE_RO;
			oldpteva = (va & ~PDRMASK) |
			    (TLBLO_PTE_TO_PA(oldpte) & PDRMASK);
			CTR3(KTR_PMAP, "%s: protect for va %#lx in pmap %p",
			    __func__, oldpteva, pmap);
		}
		if ((oldpte & PG_PROMOTE_MASK) != (newpde & PG_PROMOTE_MASK)) {
			atomic_add_long(&pmap_pde_p_failures, 1);
			CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
			    __func__, va, pmap);
			return;
		}
		pa -= PAGE_SIZE;
	}

	/*
	 * Save the page table page in its current state until the PDE
	 * mapping the superpage is demoted by pmap_demote_pde() or
	 * destroyed by pmap_remove_pde().
	 */
	mpte = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(*pde));
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("%s: page table page is out of range", __func__));
	KASSERT(mpte->pindex == pmap_pde_pindex(va),
	    ("%s: page table page's pindex is wrong", __func__));
	if (pmap_insert_pt_page(pmap, mpte)) {
		atomic_add_long(&pmap_pde_p_failures, 1);
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
		    __func__, va, pmap);
		return;
	}

	/*
	 * Promote the pv entries.
	 */
	if (pte_test(&newpde, PTE_MANAGED))
		pmap_pv_promote_pde(pmap, va, TLBLO_PTE_TO_PA(newpde), lockp);

	/*
	 * Map the superpage.
	 */
	pmap_update_pde(pmap, va, pde, newpde | PTE_PS_1M);

	atomic_add_long(&pmap_pde_promotions, 1);
	CTR3(KTR_PMAP, "%s: success for va %#lx in pmap %p", __func__, va,
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
        vm_paddr_t pa, opa;
        pd_entry_t *pde;
        pt_entry_t *pte;
        pt_entry_t origpte, newpte;
        pv_entry_t pv;
        vm_page_t mpte, om;
        boolean_t nosleep;

        va = trunc_page(va);
        KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
        KASSERT((m->oflags & VPO_UNMANAGED) != 0 || va < kmi.clean_sva ||
            va >= kmi.clean_eva,
            ("pmap_enter: managed mapping within the clean submap"));
        if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
                VM_OBJECT_ASSERT_LOCKED(m->object);

        mpte = NULL;

        lock = NULL;
        rw_rlock(&pvh_global_lock);
        PMAP_LOCK(pmap);

        /*
         * In the case that a page table page is not resident, we are
         * creating it here.
         */
	if (va < VM_MAXUSER_ADDRESS) {
                /*
                 * Here if the pte page isn't mapped, or if it has been
                 * deallocated.
                 */
                nosleep = (flags & PMAP_ENTER_NOSLEEP) != 0;
                mpte = pmap_allocpte(pmap, va, nosleep ? NULL : &lock);
                if (mpte == NULL) {
                        KASSERT(nosleep != 0,
                            ("pmap_allocpte failed with sleep allowed"));
                        if (lock != NULL)
                                rw_wunlock(lock);
                        rw_runlock(&pvh_global_lock);
                        PMAP_UNLOCK(pmap);
                        return (KERN_RESOURCE_SHORTAGE);
                }
        }
	pde = pmap_pde(pmap, va);
	if (pde_is_1m_superpage(pde)) {
		panic("%s: attempted pmap_enter on superpage", __func__);
	}
	pte = pmap_pde_to_pte(pde, va);

        /*
         *  Page Directory table entry not valid, we need a new PT page
         */
        if (pte == NULL) {
                panic("pmap_enter: invalid page directory, pdir=%p, va=%p",
                    (void *)pmap->pm_segtab, (void *)va);
        }

        pa = VM_PAGE_TO_PHYS(m);
        om = NULL;
        origpte = *pte;
	opa = TLBLO_PTE_TO_PA(origpte);

        newpte = TLBLO_PA_TO_PFN(pa) | init_pte_prot(m, flags, prot);
        /*
         * pmap_enter() is called during a fault or simulated fault so
         * set the reference bit now to avoid a fault.
         */
        pte_ref_set(&newpte);
        if ((flags & PMAP_ENTER_WIRED) != 0)
                newpte |= PTE_W;
        if (is_kernel_pmap(pmap))
                newpte |= PTE_G;
	PMAP_PTE_SET_CACHE_BITS(newpte, pa, m);
#ifdef CPU_CHERI
        if ((flags & PMAP_ENTER_NOLOADTAGS) != 0)
                newpte |= PTE_LC;
        if ((flags & PMAP_ENTER_NOSTORETAGS) != 0)
                newpte |= PTE_SC;
#endif

	/*
	 * Set modified bit gratuitously for writeable mappings if
	 * the page is unmanaged. We do not want to take a fault
	 * to do the dirty bit emulation for these mappings.
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0) {
		if (!pte_test(&newpte, PTE_RO))
			newpte |= PTE_D;
	}

        /*
         * Mapping has not changed, must be protection or wiring change.
         */
        if (pte_is_valid(&origpte) && opa == pa) {
                /*
                 * Wiring change, just update stats. We don't worry about
                 * wiring PT pages as they remain resident as long as there
                 * are valid mappings in them. Hence, if a user page is
                 * wired, the PT page will be also.
                 */
                if (pte_test(&newpte, PTE_W) && !pte_test(&origpte, PTE_W))
                        pmap->pm_stats.wired_count++;
                else if (!pte_test(&newpte, PTE_W) && pte_test(&origpte,
                    PTE_W))
                        pmap->pm_stats.wired_count--;

                KASSERT(!pte_test(&origpte, PTE_D | PTE_RO),
                    ("%s: modified page not writable: va: %p, pte: %#jx",
                    __func__, (void *)va, (uintmax_t)origpte));

                /*
                 * Remove the extra PT page reference
                 */
                if (mpte != NULL) {
                        mpte->wire_count--;
                        KASSERT(mpte->wire_count > 0,
                            ("pmap_enter: missing reference to page table page,"
                             " va: 0x%lx", va));
                }
		if (pte_test(&origpte, PTE_MANAGED)) {
			om = m;
			newpte |= PTE_MANAGED;
			if (!pte_test(&newpte, PTE_RO))
				vm_page_aflag_set(m, PGA_WRITEABLE);
		}
		goto validate;
	}

	pv = NULL;

        /*
         * Mapping has changed, invalidate old range and fall through to
         * handle validating new mapping.
         */
        if (opa) {
                if (pte_test(&origpte, PTE_W))
                        pmap->pm_stats.wired_count--;

                if (pte_test(&origpte, PTE_MANAGED)) {
                        om = PHYS_TO_VM_PAGE(opa);
                        CHANGE_PV_LIST_LOCK_TO_VM_PAGE(&lock, om);
                        pv = pmap_pvh_remove(&om->md, pmap, va);
                }
                if (mpte != NULL) {
                        mpte->wire_count--;
                        KASSERT(mpte->wire_count > 0,
                            ("pmap_enter: missing reference to page table page,"
                            " va: %p", (void *)va));
                }
        } else
                pmap_resident_count_inc(pmap, 1);

        /*
         * Enter on the PV list if part of our managed memory.
         */
        if ((m->oflags & VPO_UNMANAGED) == 0) {
                newpte |= PTE_MANAGED;
		/* Insert Entry */
		if (pv == NULL)
			pv = get_pv_entry(pmap, &lock);
                pv->pv_va = va;
                CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, pa);
                TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
                m->md.pv_gen++;
                if (!pte_test(&newpte, PTE_RO))
                        vm_page_aflag_set(m, PGA_WRITEABLE);
        } else if (pv != NULL)
		free_pv_entry(pmap, pv);


        /*
         * Increment counters
         */
        if (pte_test(&newpte, PTE_W))
                pmap->pm_stats.wired_count++;

validate:
#ifdef PMAP_DEBUG
        printf("pmap_enter:  va: %p -> pa: %p\n", (void *)va, (void *)pa);
#endif
        /*
         * if the mapping or permission bits are different, we need to
         * update the pte.
         */
        if ((origpte & ~ (PTE_D|PTE_REF)) != newpte) {
                newpte |= PTE_VR;
                if ((flags & VM_PROT_WRITE) != 0)
                        newpte |= PTE_D;
                if (pte_is_valid(&origpte)) {
                        boolean_t invlva = FALSE;

			origpte = pte_load_store(pte, newpte);
                        if (pte_is_ref(&origpte)) {
                                if (pte_test(&origpte, PTE_MANAGED))
                                        vm_page_aflag_set(om, PGA_REFERENCED);
                                if (opa != pa)
                                        invlva = TRUE;
                        }
                        if (pte_test(&origpte, PTE_D) &&
			    !pte_test(&origpte, PTE_RO)) {
                                if (pte_test(&origpte, PTE_MANAGED))
                                        vm_page_dirty(om);
                                if ((prot & VM_PROT_WRITE) == 0)
                                        invlva = TRUE;
                        }
                        if (pte_test(&origpte, PTE_MANAGED) &&
                            TAILQ_EMPTY(&om->md.pv_list) &&
			    ((om->flags & PG_FICTITIOUS) != 0 ||
			    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
                                vm_page_aflag_clear(om, PGA_WRITEABLE);
                        if (invlva)
                                pmap_invalidate_page(pmap, va);
                } else
			pte_store(pte, newpte);
        }

        /*
         *  If both the page table page and the reservation are fully
         *  populated, then attempt promotion.
         */
        if ((mpte == NULL || mpte->wire_count == NPTEPG) &&
            (m->flags & PG_FICTITIOUS) == 0 &&
            pg_sp_enabled && vm_reserv_level_iffullpop(m) == 0)
                pmap_promote_pde(pmap, pde, va, &lock);

        /*
         * Sync I & D caches for executable pages.  Do this only if the
         * target pmap belongs to the current process.  Otherwise, an
         * unresolvable TLB miss may occur.
         */
        if (!is_kernel_pmap(pmap) && (pmap == &curproc->p_vmspace->vm_pmap) &&
            (prot & VM_PROT_EXECUTE)) {
                mips_icache_sync_range(va, PAGE_SIZE);
                mips_dcache_wbinv_range(va, PAGE_SIZE);
        }
        if (lock != NULL)
                rw_wunlock(lock);
        rw_runlock(&pvh_global_lock);
        PMAP_UNLOCK(pmap);
        return (KERN_SUCCESS);
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
	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(pmap, va, m, prot, NULL, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

static vm_page_t
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp)
{
	pt_entry_t *pte, newpte;
	vm_paddr_t pa;
	struct spglist free;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("%s: managed mapping within the clean submap", __func__));
	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * In the case that a page table page is not resident, we are
	 * creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		pd_entry_t *pde;
		unsigned ptepindex;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = pmap_pde_pindex(va);
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->wire_count++;
		} else {
			/*
			 * Get the page directory entry
			 */
			pde = pmap_pde(pmap, va);

			/*
			 * If the page table page is mapped, we just
			 * increment the hold count, and activate it.
			 */
			if (pde && *pde != 0) {
				if (pde_is_1m_superpage(pde))
					return (NULL);
				mpte = PHYS_TO_VM_PAGE(
				    MIPS_DIRECT_TO_PHYS(*pde));
				mpte->wire_count++;
			} else {
				/*
				 * Pass NULL instead of the PV list lock
				 * pointer, because we don't intend to sleep.
				 */
				mpte = _pmap_allocpte(pmap, ptepindex, NULL);
				if (mpte == NULL)
					return (mpte);
			}
		}
	} else {
		mpte = NULL;
	}

	pte = pmap_pte(pmap, va);
	if (pte_is_valid(pte)) {
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
			if (pmap_unwire_ptp(pmap, va, mpte, &free)) {
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

	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * Now validate mapping with RO protection
	 */
	newpte = PTE_RO | TLBLO_PA_TO_PFN(pa) | PTE_VALID;
	if ((m->oflags & VPO_UNMANAGED) == 0)
		newpte |= PTE_MANAGED;

	PMAP_PTE_SET_CACHE_BITS(newpte, pa, m);

	sched_pin();
	if (is_kernel_pmap(pmap)) {
		newpte |= PTE_G;
		pte_ref_set(&newpte);
		pte_store(pte, newpte);
	} else {
		pte_store(pte, newpte);
		/*
		 * Sync I & D caches.  Do this only if the target pmap
		 * belongs to the current process.  Otherwise, an
		 * unresolvable TLB miss may occur.
		 */
		if (pmap == &curproc->p_vmspace->vm_pmap) {
			va &= ~PAGE_MASK;
			mips_icache_sync_range(va, PAGE_SIZE);
			mips_dcache_wbinv_range(va, PAGE_SIZE);
		}
	}
	sched_unpin();

	return (mpte);
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 *
 * Use XKPHYS for 64 bit.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{

	if (i != 0)
		printf("%s: ERROR!!! More than one page of virtual address "
		    "mapping not supported\n", __func__);

	return ((void *)MIPS_PHYS_TO_DIRECT(pa));
}

void
pmap_kenter_temporary_free(vm_paddr_t pa)
{

	/* nothing to do for mips64 */
	return;
}

static vm_page_t
pmap_allocpde(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t pdpindex, ptepindex;
	pd_entry_t *pdpe;
	vm_page_t mpte = NULL;

	if (va < VM_MAXUSER_ADDRESS) {
retry:
		pdpe = pmap_segmap(pmap, va);
		if (pdpe != NULL && (*pdpe != NULL)) {
			/* Add a reference to the pd page. */
			mpte = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(*pdpe));
			mpte->wire_count++;
		} else {
			/* Allocate a pd page. */

			/* Calculate pagetable page index. */
			ptepindex = pmap_pde_pindex(va);
			pdpindex = ptepindex >> NPDEPGSHIFT;  /* XXX */
			mpte = _pmap_allocpte(pmap, NUPDE + pdpindex, lockp);
			if (mpte == NULL && lockp != NULL)
				goto retry;
		}
	}
	return (mpte);
}

/*
 * Tries to create a 2MB page mapping.  Returns TRUE if successful and FALSE
 * otherwise.  Fails if (1) a page table page cannot be allocated without
 * blocking, (2) a mapping already exists at the specified virtual address, or
 * (3) a pv entry cannot be allocated without reclaiming another pv entry.
 */
static boolean_t
pmap_enter_pde(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    struct rwlock **lockp)
{
	pd_entry_t *pde;
	pt_entry_t newpde;
	vm_page_t mpde;
	struct spglist free;
	vm_paddr_t pa;


	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	if (is_kernel_pmap(pmap)) {
		/* Not doing the kernel pmap for now */
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p: kernel map",
		    __func__, va, pmap);
		return (FALSE);
	}
	if ((mpde = pmap_allocpde(pmap, va, NULL)) == NULL) {
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
		    __func__, va, pmap);
		return (FALSE);
	}
	/* pde = pmap_pde(pmap, va); */
	pde = (pd_entry_t *)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(mpde));
	pde = &pde[pmap_pde_index(va)];
	if (pde == NULL) {
		KASSERT(mpde->wire_count > 1,
		    ("%s: mpde's wire count is too low", __func__));
		mpde->wire_count--;
		CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p", __func__,
		    va, pmap);
		return (FALSE);
	}
	pa = VM_PAGE_TO_PHYS(m);
        newpde = PTE_RO | TLBLO_PA_TO_PFN(pa) | PTE_VALID | PTE_PS_1M;
	PMAP_PTE_SET_CACHE_BITS(newpde, pa, m);
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		newpde |= PTE_MANAGED;

		/*
		 * Abort this mapping if its PV entry could not be created.
		 */
		if (!pmap_pv_insert_pde(pmap, va, VM_PAGE_TO_PHYS(m),
		    lockp)) {
			SLIST_INIT(&free);
			if (pmap_unwire_ptp(pmap, va, mpde, &free)) {
				pmap_invalidate_page(pmap, va);
				pmap_free_zero_pages(&free);
			}
			CTR3(KTR_PMAP, "%s: failure for va %#lx in pmap %p",
			    __func__, va, pmap);
			return (FALSE);
		}
	}

	/*
	 * Increment counters.
	 */
	 pmap_resident_count_inc(pmap, NBPDR / PAGE_SIZE);

	 /*
	  * Map the superpage.
	  */
	 sched_pin();
	 pde_store(pde, newpde);

	/*
	 * Sync I & D caches for executable pages.  Do this only if the
	 * target pmap belongs to the current process.  Otherwise, an
	 * unresolvable TLB miss may occur.
	 */
	 if (!is_kernel_pmap(pmap) && (pmap == &curproc->p_vmspace->vm_pmap) &&
	     (prot & VM_PROT_EXECUTE)) {
			 va &= ~PDRMASK;
			 mips_icache_sync_range(va, NBPDR);
			 mips_dcache_wbinv_range(va, NBPDR);

	 }
	 sched_unpin();

	 atomic_add_long(&pmap_pde_mappings, 1);
	 CTR3(KTR_PMAP, "%s: success for va %#lx in pmap %p", __func__, va,
	     pmap);
	 return (TRUE);
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
	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		if ((va & PDRMASK) == 0 && va + NBPDR <= end &&
		    m->psind == 1 && pg_sp_enabled &&
		    pmap_enter_pde(pmap, va, m, prot, &lock))
			m = &m[NBPDR / PAGE_SIZE - 1];
		else
			mpte = pmap_enter_quick_locked(pmap, va, m, prot,
			    mpte, &lock);
		m = TAILQ_NEXT(m, listq);
	}
	if (lock != NULL)
		rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after a mmap().
 *
 * This code maps large physical mmap regions into the
 * processor address space. Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{
	pd_entry_t *pde;
	vm_paddr_t pa, ptepa;
	vm_page_t p, pdpg;
	vm_memattr_t memattr;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));

	if (is_kernel_pmap(pmap)) {
		/* Not doing the kernel pmap for now. */
		return;
	}

	if ((addr & (NBPDR - 1)) == 0 && (size & (NBPDR - 1)) == 0) {
		if (!pg_sp_enabled)
			return;
		if (!vm_object_populate(object, pindex, pindex + atop(size)))
			return;
		p = vm_page_lookup(object, pindex);
		KASSERT(p->valid == VM_PAGE_BITS_ALL,
		    ("%s: invalid page %p", __func__, p));
		memattr = pmap_page_get_memattr(p);

		/*
		 * Abort the mapping if the first page is not physically
		 * aligned to a 2MB page boundary.
		 */
		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1))
			return;

		/*
		 * Skip the first page. Abort the mapping if the rest of
		 * the pages are not physically contiguous or have differing
		 * memory attributes.
		 */
		p = TAILQ_NEXT(p, listq);
		for (pa = ptepa + PAGE_SIZE; pa < ptepa + size;
		    pa += PAGE_SIZE) {
			KASSERT(p->valid == VM_PAGE_BITS_ALL,
			    ("%s: invalid page %p", __func__, p));
			if (pa != VM_PAGE_TO_PHYS(p) ||
			    memattr != pmap_page_get_memattr(p))
				return;
			p = TAILQ_NEXT(p, listq);
		}

		/*
		 * Map using 2MB pages.  "ptepa" is 2M aligned and "size"
		 * is a multiple of 2M.
		 */
		PMAP_LOCK(pmap);
		for (pa = ptepa; pa < ptepa + size; pa += NBPDR) {
			pdpg = pmap_allocpde(pmap, addr, NULL);
			if (pdpg == NULL) {
				/*
				 * The creation of mappings below is only an
				 * optimization.  If a page directory page
				 * cannot be allocated without blocking,
				 * continue on to the next mapping rather than
				 * blocking.
				 */
				addr += NBPDR;
				continue;
			}
			pde = (pd_entry_t *)MIPS_PHYS_TO_DIRECT(
			    VM_PAGE_TO_PHYS(pdpg));
			pde = &pde[pmap_pde_index(addr)];
			if (!pte_is_valid((pt_entry_t *)pde)) {
				pt_entry_t newpte = TLBLO_PA_TO_PFN(pa) |
				    PTE_PS_1M | PTE_D | PTE_REF | PTE_VALID;

				PMAP_PTE_SET_CACHE_BITS(newpte, pa, p);
				pde_store(pde, newpte);
				pmap_resident_count_inc(pmap, NBPDR/PAGE_SIZE);
				atomic_add_long(&pmap_pde_mappings, 1);
			} else {
				/* Continue on if the PDE is already valid. */
				pdpg->wire_count--;
				KASSERT(pdpg->wire_count > 0,
				    ("%s: missing reference to page directory "
				    "page, va: 0x%lx", __func__, addr));
			}
			addr += NBPDR;
		}
		PMAP_UNLOCK(pmap);
	}
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
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;
	vm_offset_t va_next;
	boolean_t pv_lists_locked;

	pv_lists_locked = FALSE;
resume:
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pdpe = pmap_segmap(pmap, sva);
		if (*pdpe == NULL) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;
		pde = pmap_pdpe_to_pde(pdpe, sva);
		if (*pde == NULL)
			continue;
		if (pde_is_1m_superpage(pde)) {
			if (!pde_test(pde, PTE_W))
				panic("pmap_unwire: pde %#jx is missing PTE_W",
				    (uintmax_t)*pde);
			/*
			 * Are we unwiring the entire superpage? If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				atomic_clear_long((pt_entry_t *)pde, PTE_W);
				pmap->pm_stats.wired_count -= NBPDR / PAGE_SIZE;
				continue;
			} else {
				if (!pv_lists_locked) {
					pv_lists_locked = TRUE;
					if (!rw_try_rlock(&pvh_global_lock)) {
						 PMAP_UNLOCK(pmap);
						 rw_rlock(&pvh_global_lock);
						 /* Repeat sva. */
						 goto resume;
					}
				}
				if (!pmap_demote_pde(pmap, pde, sva))
					panic("pmap_unwire: demotion failed");
			}
		}
		if (va_next > eva)
			va_next = eva;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if (!pte_is_valid(pte))
				continue;
			if (!pte_test(pte, PTE_W))
				panic("pmap_unwire: pte %#jx is missing PG_W",
				    (uintmax_t)*pte);
			/*
			 * PTE_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PTE_W, another processor
			 * could be setting PTE_D and/or PTE_REF concurrently.
			 */
			pte_atomic_clear(pte, PTE_W);
			pmap->pm_stats.wired_count--;
		}
	}
	if (pv_lists_locked) {
		rw_runlock(&pvh_global_lock);
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
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
    vm_size_t len, vm_offset_t src_addr)
{
#if 0
/*
 * XXX This doesn't help with fork() performance and
 * adds more overhead.  Maybe the reference bit emulation
 * is causing fault-like overhead anyway?
 */

	struct rwlock *lock;
	struct spglist free;
	vm_offset_t addr, end_addr, va_next;

	if (dst_addr != src_addr)
		return;

	if (PCPU_GET(curpmap) != src_pmap)
		return;

	end_addr = src_addr + len;

	lock = NULL;
	rw_rlock(&pvh_global_lock);
	/* Lock the pmaps in the same order to avoid deadlock. */
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}

	for (addr = src_addr; addr < end_addr; addr = va_next) {
		pt_entry_t *src_pte, *dst_pte;
		vm_page_t dstmpde, dstmpte, srcmpte;
		pd_entry_t *src_pdpe, *src_pde, *dst_pde;
		pt_entry_t srcpte;
		vm_paddr_t srcpaddr;
		vm_page_t m;


		src_pdpe = pmap_segmap(src_pmap, addr);
		if (src_pdpe == NULL || *src_pdpe == 0) {
			va_next = (addr + NBSEG) & ~SEGMASK;
			/*
			 * If the next va is out of the copy range then set
			 * it to end_addr in order to copy all mappings until
			 * given limit.
			 */
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next < addr)
			va_next = end_addr;

		src_pde = pmap_pdpe_to_pde(src_pdpe, addr);
		if (src_pde == NULL || *src_pde == 0)
			continue;
		srcpte = (pt_entry_t)*src_pde;

		if (pte_is_1m_superpage(&srcpte)) {
			/* Copy superpage pde. */
			if ((addr & PDRMASK) != 0 || addr + NBPDR > end_addr)
				continue;
			dstmpde = pmap_allocpde(dst_pmap, addr, NULL);
			if (dstmpde == NULL)
				break;

			/* dst_pde = pmap_pde(dst_pmap, addr); */
			dst_pde = (pd_entry_t *)MIPS_PHYS_TO_DIRECT(
			    VM_PAGE_TO_PHYS(dstmpde));
			dst_pde = &dst_pde[pmap_pde_index(addr)];

			if (*dst_pde == 0 &&
			    (!pte_test(&srcpte, PTE_MANAGED) ||
			    pmap_pv_insert_pde(dst_pmap, addr,
			    TLBLO_PTE_TO_PA(srcpte), &lock))) {
				*dst_pde = (pd_entry_t)(srcpte & ~PTE_W);
				pmap_resident_count_inc(dst_pmap, NBPDR /
				    PAGE_SIZE);
			} else
				dstmpde->wire_count--;
			continue;
		}

		srcpaddr = MIPS_DIRECT_TO_PHYS(*src_pde);
		srcmpte = PHYS_TO_VM_PAGE(srcpaddr);
		KASSERT(srcmpte->wire_count > 0,
		    ("pmap_copy: source page table page is unused"));

		/*
		 * Limit our scan to either the end of the vaddr represented
		 * by the source page table page, or to the end of the range
		 * being copied.
		 */
		if (va_next > end_addr)
			va_next = end_addr;

		/*
		 * Walk the source page table entries and copy the managed
		 * entries.
		 */

		/* src_pte = pmap_pde_to_pte(src_pde, addr); */
		src_pte = (pt_entry_t *)MIPS_PHYS_TO_DIRECT(srcpaddr);
		src_pte = &src_pte[pmap_pte_index(addr)];

		if (src_pte == NULL || *src_pte == 0)
			continue;

		dstmpte = NULL;
		while (addr < va_next) {
			unsigned pdepindex;
			pt_entry_t ptetemp;


			ptetemp = *src_pte;

			/*
			 * we only virtual copy managed pages
			 */
			if (pte_test(&ptetemp, PTE_MANAGED)) {
				/* Calculate pagetable page index */
				pdepindex = pmap_pde_pindex(addr);

				/* Get the page directory entry. */
				dst_pde = pmap_pde(dst_pmap, addr);

				if (dst_pde != NULL && *dst_pde != 0) {
					dstmpte = PHYS_TO_VM_PAGE(
					    MIPS_DIRECT_TO_PHYS(*dst_pde));
				} else
					dstmpte = NULL;

				if (dstmpte != NULL &&
				    dstmpte->pindex == pdepindex) {
					/*
					 * The page table is mapped so just
					 * increment the hold count.
					 */
					dstmpte->wire_count++;
				} else {
					/*
					 * The page table isn't mapped, or it
					 * has been deallocated.
					 */
					dstmpte = pmap_allocpte(dst_pmap,
					    addr, NULL);

					/*
					 * If we are having memory alloc issues
					 * then abandon trying to copy the page
					 * tables.
					 */
					if (dstmpte == NULL)
						goto out;
				}
				/*
				 * Now that we have a page directory, get the
				 * pte.
				 */

				/* dst_pte = pmap_pte(dst_pmap, addr); */
				dst_pte = (pt_entry_t *)
				    MIPS_PHYS_TO_DIRECT(
					VM_PAGE_TO_PHYS(dstmpte));
				dst_pte = &dst_pte[pmap_pte_index(addr)];

				/* Try and insert the pv_entry. */
				m = PHYS_TO_VM_PAGE(TLBLO_PTE_TO_PA(ptetemp));
				if (*dst_pte == 0 &&
				    pmap_try_insert_pv_entry(dst_pmap, addr, m,
				    &lock)) {
					/*
					 * Populate the entry.
					 *
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					pte_clear(&ptetemp, PTE_W | PTE_D |
					    PTE_REF);
					*dst_pte = ptetemp;
					/* Update stats. */
					pmap_resident_count_inc(dst_pmap, 1);
				} else {
					SLIST_INIT(&free);
					if (pmap_unwire_ptp(dst_pmap, addr,
					    dstmpte, &free)) {
						pmap_invalidate_page(dst_pmap,
						    addr);
						pmap_free_zero_pages(&free);
					}
					goto out;
				}
				/* Check the wire_count to see if we're done. */
				if (dstmpte->wire_count >= srcmpte->wire_count)
					break;
			}
			addr += PAGE_SIZE;
			src_pte++;
		}
	}
out:
	if (lock != NULL)
		rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
#endif /* #if 0 */
}

/*
 *	pmap_zero_page zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.
 *
 * 	Use XKPHYS for 64 bit.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	va = MIPS_PHYS_TO_DIRECT(phys);
	sched_pin();
	bzero((caddr_t)va, PAGE_SIZE);
	mips_dcache_wbinv_range(va, PAGE_SIZE);
	sched_unpin();
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
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	va = MIPS_PHYS_TO_DIRECT(phys);
	sched_pin();
	bzero((char *)(caddr_t)va + off, size);
	mips_dcache_wbinv_range(va + off, size);
	sched_unpin();
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 *
 * 	Use XKPHYS for 64 bit.
 */
#define	PMAP_COPY_TAGS	0x00000001
static void
pmap_copy_page_internal(vm_page_t src, vm_page_t dst, int flags)
{
	vm_offset_t va_src, va_dst;
	vm_paddr_t phys_src = VM_PAGE_TO_PHYS(src);
	vm_paddr_t phys_dst = VM_PAGE_TO_PHYS(dst);

	/* easy case, all can be accessed via KSEG0 */
	/*
	 * Flush all caches for VA that are mapped to this page
	 * to make sure that data in SDRAM is up to date
	 */
	sched_pin();
	pmap_flush_pvcache(src);
	mips_dcache_wbinv_range_index(MIPS_PHYS_TO_DIRECT(phys_dst), PAGE_SIZE);
	va_src = MIPS_PHYS_TO_DIRECT(phys_src);
	va_dst = MIPS_PHYS_TO_DIRECT(phys_dst);
#ifdef CPU_CHERI
	if (flags & PMAP_COPY_TAGS)
		cheri_bcopy((caddr_t)va_src, (caddr_t)va_dst, PAGE_SIZE);
	else
#else
		bcopy((caddr_t)va_src, (caddr_t)va_dst, PAGE_SIZE);
#endif
	mips_dcache_wbinv_range(va_dst, PAGE_SIZE);
	sched_unpin();
}

/*
 * With CHERI, it is sometimes desirable to explicitly propagate tags between
 * pages (e.g., during copy-on-write), but not other times (e.g., copying data
 * from VM to buffer cache).  There is more playing out here yet to do (e.g.,
 * if any filesystems learn to preserve tags) but these KPIs allow us to
 * capture that difference in the mean time.
 */
void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{

	pmap_copy_page_internal(src, dst, 0);
}

#ifdef CPU_CHERI
void
pmap_copy_page_tags(vm_page_t src, vm_page_t dst)
{

	pmap_copy_page_internal(src, dst, PMAP_COPY_TAGS);
}
#endif

int unmapped_buf_allowed;

static void
pmap_copy_pages_internal(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize, int flags)
{
	char *a_cp, *b_cp;
	vm_page_t a_m, b_m;
	vm_offset_t a_pg_offset, b_pg_offset;
	vm_paddr_t a_phys, b_phys;
	int cnt;

	sched_pin();
	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		a_m = ma[a_offset >> PAGE_SHIFT];
		a_phys = VM_PAGE_TO_PHYS(a_m);
		b_pg_offset = b_offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		b_m = mb[b_offset >> PAGE_SHIFT];
		b_phys = VM_PAGE_TO_PHYS(b_m);
		pmap_flush_pvcache(a_m);
		mips_dcache_wbinv_range_index(MIPS_PHYS_TO_DIRECT(b_phys),
		    PAGE_SIZE);
		a_cp = (char *)MIPS_PHYS_TO_DIRECT(a_phys) + a_pg_offset;
		b_cp = (char *)MIPS_PHYS_TO_DIRECT(b_phys) + b_pg_offset;
#ifdef CPU_CHERI
		if (flags & PMAP_COPY_TAGS)
			cheri_bcopy(a_cp, b_cp, cnt);
		else
#else
			bcopy(a_cp, b_cp, cnt);
#endif
		mips_dcache_wbinv_range((vm_offset_t)b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
	sched_unpin();
}

/*
 * As with pmap_copy_page(), CHERI requires tagged and non-tagged versions
 * depending on the circumstances.
 */
void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{

	pmap_copy_pages_internal(ma, a_offset, mb, b_offset, xfersize, 0);
}

#ifdef CPU_CHERI
void
pmap_copy_pages_tags(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{

	pmap_copy_pages_internal(ma, a_offset, mb, b_offset, xfersize,
	    PMAP_COPY_TAGS);
}
#endif

vm_offset_t
pmap_quick_enter_page(vm_page_t m)
{
	return MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));
}

void
pmap_quick_remove_page(vm_offset_t addr)
{
	mips_dcache_wbinv_range(addr, PAGE_SIZE);
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
	    ("%s: page %p is not managed", __func__, m));
	rv = FALSE;
	rw_rlock(&pvh_global_lock);
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
	rw_runlock(&pvh_global_lock);
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
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *pte;
	int count, md_gen;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (0);
	rw_rlock(&pvh_global_lock);
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
		pte = pmap_pte(pmap, pv->pv_va);
		if (pte_test(pte, PTE_W))
			count++;
		PMAP_UNLOCK(pmap);
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		struct md_page *pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				int pvh_gen = pvh->pv_gen;
				md_gen = m->md.pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pd_entry_t *pde = pmap_pde(pmap, pv->pv_va);
			if (pte_test((pt_entry_t *)pde, PTE_W))
				count++;
			PMAP_UNLOCK(pmap);
		}
	}
	rw_runlock(lock);
	rw_runlock(&pvh_global_lock);
	return (count);
}

/*
 *  Returns TRUE if the given page is mapped individually or as part of
 *  a 2mpage. Otherwise, returns FALSE.
 */
boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	struct rwlock *lock;
	boolean_t rv;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);
	rw_rlock(&pvh_global_lock);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	rv = !TAILQ_EMPTY(&m->md.pv_list) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
		!TAILQ_EMPTY(&pa_to_pvh(VM_PAGE_TO_PHYS(m))->pv_list));
	rw_runlock(lock);
	rw_runlock(&pvh_global_lock);
	return (rv);
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
	pd_entry_t ptepde, *pde;
	pt_entry_t *pte, tpte;
	struct spglist free;
	vm_page_t m, mpte, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct rwlock *lock;
	int bit;
	uint64_t inuse, bitmask;
	int allfree, field, freed, idx;
	boolean_t superpage;
	vm_paddr_t pa;

	/*
	 * Assert that the given pmap is only active on the current
	 * CPU.  Unfortunately, we cannot block another CPU from
	 * activating the pmap while this function is executing.
	 */
	KASSERT(pmap == vmspace_pmap(curthread->td_proc->p_vmspace),
	    ("%s: non-current pmap %p", __func__, pmap));

	lock = NULL;
	SLIST_INIT(&free);
	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = ffsl(inuse) - 1;
				bitmask = 1UL << bit;
				idx = field * sizeof(inuse) * NBBY + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pde = pmap_segmap(pmap, pv->pv_va);
				ptepde = *pde;
				pde = pmap_pdpe_to_pde(pde, pv->pv_va);
				if (pde_is_1m_superpage(pde)) {
					superpage = TRUE;
					pte = (pt_entry_t *)pde;
				} else {
					superpage = FALSE;
					ptepde = *pde;
					pte = pmap_pde_to_pte(pde, pv->pv_va);
				}
				tpte = *pte;
				if (!pte_is_valid(pte))  {
					panic("%s: bad %s pte va %lx pte %lx",
					    __func__, superpage ? "superpage" :
					    "", pv->pv_va, tpte);
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (pte_test(&tpte, PTE_W)) {
					allfree = 0;
					continue;
				}

				pa = TLBLO_PTE_TO_PA(tpte);
				if (superpage)
					pa &= ~PDRMASK;
				m = PHYS_TO_VM_PAGE(pa);
				KASSERT(m->phys_addr == pa,
				    ("%s: vm_page_t %p phys_addr mismatch "
				    "%016jx %016jx", __func__, m,
				    (uintmax_t)m->phys_addr, (uintmax_t)tpte));

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("%s: bad tpte %#jx", __func__,
				    (uintmax_t)tpte));

				/* Clear PTE */
				if (superpage)
					pte_store(pte, 0);
				else
					pte_store(pte, is_kernel_pmap(pmap) ?
					    PTE_G : 0);

				/*
				 * Update the vm_page_t clean and reference bits
				 */
				if (pte_test(&tpte, PTE_D) &&
				    !pte_test(&tpte, PTE_RO)) {
					if (superpage) {
						vm_page_t mt;

						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							vm_page_dirty(mt);
					} else
						vm_page_dirty(m);
				}

				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(&lock, m);

				/* Mark free */
				pc->pc_map[field] |= bitmask;
				if (superpage) {
					pmap_resident_count_dec(pmap, NBPDR / PAGE_SIZE);
					pvh = pa_to_pvh(TLBLO_PTE_TO_PA(tpte));
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
					pvh->pv_gen++;
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							if (TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					mpte = pmap_lookup_pt_page(pmap, pv->pv_va);
					if (mpte != NULL) {
						pmap_remove_pt_page(pmap, mpte);
						pmap_resident_count_dec(pmap, 1);
						KASSERT(mpte->wire_count == NPTEPG,
						    ("%s: pte page wire count error",
						    __func__));
						mpte->wire_count = 0;
						pmap_add_delayed_free_list(mpte, &free, FALSE);
						atomic_subtract_int(&vm_cnt.v_wire_count, 1);
					}
				} else {
					pmap_resident_count_dec(pmap, 1);
					TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
					m->md.pv_gen++;
					if ((m->aflags & PGA_WRITEABLE) != 0 &&
					    TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m, PGA_WRITEABLE);
					}
				}
				pmap_unuse_pt(pmap, pv->pv_va, ptepde, &free);
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
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	rw_runlock(&pvh_global_lock);
	pmap_free_zero_pages(&free);
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	struct md_page *pvh;
	vm_offset_t va;
	pv_entry_t next_pv;
	int pvh_gen;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t *pde;
	pt_entry_t oldpte, *pte;
	pv_entry_t pv;
	int md_gen;

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
	rw_rlock(&pvh_global_lock);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
retry_pv_loop:
	rw_wlock(lock);
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
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
		pde = pmap_pde(pmap, va);
		if (pde_is_1m_superpage(pde) && !pde_test(pde, PTE_RO))
			(void)pmap_demote_pde_locked(pmap, pde, va, &lock);
		KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
		    ("inconsistent pv lock %p %p for page %p",
		    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
		PMAP_UNLOCK(pmap);
	}
small_mappings:
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
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT(!pde_is_superpage(pde),
		    ("%s: found a superpage in page %p's pv list",
		    __func__, m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		KASSERT(pte != NULL && pte_is_valid(pte),
		    ("%s:page on pv_list has no pte", __func__));
retry:
		oldpte = *pte;
		if (!pte_test(&oldpte, PTE_RO)) {
			if (!atomic_cmpset_long(pte, oldpte,
			    ((oldpte & ~PTE_D) | PTE_RO)))
				goto retry;
			if (pte_test(&oldpte, PTE_D))
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
}

#define	PMAP_TS_REFERENCED_MAX	5

/*-
 *	pmap_ts_referenced:
 *
 *  Return a count of pages that have been referenced, and reset the
 *  reference bit.  It is not necessary for every reference bit to be
 *  reset, but it is necessary that 0 only be returned when there are
 *  truly have been pages referenced.
 *
 *  XXX: The exact number of flags to check and reset is a matter that
 *  should be tested and standardized at some point in the future for
 *  optimal aging of shared pages.
 */
int
pmap_ts_referenced(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_offset_t va;
	vm_paddr_t pa;
	int cleared, md_gen, not_cleared, pvh_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	cleared = 0;
	pa = VM_PAGE_TO_PHYS(m);
	lock = PHYS_TO_PV_LIST_LOCK(pa);
	pvh = pa_to_pvh(pa);
	rw_rlock(&pvh_global_lock);
	rw_wlock(lock);
retry:
	not_cleared = 0;
	if ((m->flags & PG_FICTITIOUS) != 0 ||
	    (pvf = TAILQ_FIRST(&pvh->pv_list)) == NULL)
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
		pde = pmap_pde(pmap, pv->pv_va);
		if (pte_is_ref((pt_entry_t *)pde)) {
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
			if ((((pa >> PAGE_SHIFT) ^ (pv->pv_va >> PDRSHIFT) ^
			    (uintptr_t)pmap) & (NPTEPG - 1)) == 0 &&
			    !pde_test(pde, PTE_W)) {
				atomic_clear_long((pt_entry_t *)pde, PTE_REF);
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
			} else
				not_cleared++;

		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
		}
		if ((cleared + not_cleared) >= PMAP_TS_REFERENCED_MAX)
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
			if (
			    pvh_gen != pvh->pv_gen ||
			    md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}

		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT(!pde_is_superpage(pde),
		    ("pmap_ts_referenced: found superpage in page %p's pv list",
		    m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if (pte_is_ref(pte)) {
			atomic_clear_long((pt_entry_t *)pde, PTE_REF);
			pmap_invalidate_page(pmap, pv->pv_va);
			cleared++;
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
	rw_runlock(&pvh_global_lock);
	return (cleared + not_cleared);
}

static boolean_t
pmap_page_test_mappings(vm_page_t m, boolean_t accessed, boolean_t modified)
{
	struct rwlock *lock;
	pv_entry_t pv;
	struct md_page *pvh;
	pt_entry_t *pte;
	pmap_t pmap;
	int md_gen, pvh_gen;
	boolean_t rv;

	rv = FALSE;
	rw_rlock(&pvh_global_lock);
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
		pte = pmap_pte(pmap, pv->pv_va);
		if (modified) {
			rv = pte_test(pte, PTE_D) && !pte_test(pte, PTE_RO);
			if (accessed)
				rv = rv && pte_is_valid(pte) && pte_is_ref(pte);
		} else if (accessed) {
			rv = pte_is_valid(pte) && pte_is_ref(pte);
		}
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
			pte = (pt_entry_t *)pmap_pde(pmap, pv->pv_va);
			if (modified) {
				rv = pte_test(pte, PTE_D) &&
				    !pte_test(pte, PTE_RO);
				if (accessed)
					rv = rv && pte_is_valid(pte) &&
					    pte_is_ref(pte);
			} else if (accessed) {
				rv = pte_is_valid(pte) &&
				    pte_is_ref(pte);
			}
			PMAP_UNLOCK(pmap);
			if (rv)
				goto out;
		}
	}
out:
	rw_runlock(lock);
	rw_runlock(&pvh_global_lock);
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
	 * is clear, no PTEs can have PTE_D set.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return (FALSE);
	return (pmap_page_test_mappings(m, FALSE, TRUE));
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	boolean_t rv;

	rv = FALSE;
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, addr);
	if (pde != NULL && *pde != 0) {
		if (pde_is_1m_superpage(pde))
			pte = (pt_entry_t *)pde;
		else
			pte = pmap_pde_to_pte(pde, addr);
		rv = (*pte == 0) || (*pte == PTE_G);
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
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	struct rwlock *lock;
	pd_entry_t *pde, *pdpe, oldpde;
	pt_entry_t *pte;
	vm_offset_t va_next;
	vm_page_t m;
	boolean_t anychanged, pv_lists_locked;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;

	pv_lists_locked = FALSE;
resume:
	anychanged = FALSE;
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pdpe = pmap_segmap(pmap, sva);
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		oldpde = *pde;
		if (pde == NULL || *pde == 0)
			continue;
		else if (pde_is_1m_superpage(pde)) {
			if (!pde_test(&oldpde, PTE_MANAGED))
				continue;
			if (!pv_lists_locked) {
				pv_lists_locked = TRUE;
				if (!rw_try_rlock(&pvh_global_lock)) {
					if (anychanged)
						pmap_invalidate_all(pmap);
					PMAP_UNLOCK(pmap);
					rw_rlock(&pvh_global_lock);
					goto resume;
				}
			}
			lock = NULL;
			if (!pmap_demote_pde_locked(pmap, pde, sva, &lock)) {
				if (lock != NULL)
					rw_wunlock(lock);

				/*
				 * The superpage mapping was destroyed.
				 */
				continue;
			}

			/*
			 * Unless the page mappings are wired, remove the
			 * mapping to a single page so that a subsequent
			 * access may repromote.  Since the underlying page
			 * table page is fully populated, this removal never
			 * frees a page table page.
			 */
			if (!pde_test(&oldpde, PTE_W)) {
				pte = pmap_pde_to_pte(pde, sva);
				KASSERT(pte_test(pte, PTE_VALID),
				    ("pmap_advise: invalid PTE"));
				pmap_remove_pte(pmap, pte, sva, *pde, NULL,
				    &lock);
				anychanged = TRUE;
			}
			if (lock != NULL)
				rw_wunlock(lock);
		}
		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being write protected.
		 */
		if (va_next > eva)
			va_next = eva;

		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if (!pte_is_valid(pte) || !pte_test(pte, PTE_MANAGED))
				continue;
			else if (pte_test(pte, PTE_D) &&
			    !pte_test(pte, PTE_RO)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Future calls to pmap_is_modified()
					 * can be avoided by making the page
					 * dirty now.
					 */
					m = PHYS_TO_VM_PAGE(
					    TLBLO_PTE_TO_PA(*pte));
					vm_page_dirty(m);
				}
				pte_atomic_clear(pte, PTE_D | PTE_REF);
			} else if (pte_is_ref(pte))
				pte_atomic_clear(pte, PTE_REF);
			else
				continue;
			if (pte_test(pte, PTE_G))
				pmap_invalidate_page(pmap, sva);
			else
				anychanged = TRUE;
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	if (pv_lists_locked) {
		rw_runlock(&pvh_global_lock);
	}
	PMAP_UNLOCK(pmap);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	pv_entry_t next_pv, pv;
	pd_entry_t oldpde, *pde;
	pt_entry_t oldpte, *pte;
	struct rwlock *lock;
	vm_offset_t va;
	int md_gen, pvh_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	KASSERT(!vm_page_xbusied(m),
	    ("pmap_clear_modify: page %p is exclusive busied", m));

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have PTE_D set.
	 * If the object containing the page is locked and the page is not
	 * write busied, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	rw_rlock(&pvh_global_lock);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_wlock(lock);
restart:
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		oldpde = *pde;
		if (!pde_test(pde, PTE_RO)) {
			if (pmap_demote_pde_locked(pmap, pde, va, &lock)) {
				if (!pde_test(&oldpde, PTE_W)) {
					/*
					 * Write protect the mapping to a
					 * single page so that a subsequent
					 * write access may repromote.
					 */
					va += VM_PAGE_TO_PHYS(m) -
					   TLBLO_PDE_TO_PA(oldpde);
					pte = pmap_pde_to_pte(pde, va);
					oldpte = *pte;
					if (!pte_test(&oldpte, PTE_VALID)) {
						while (!atomic_cmpset_long(pte,
						    oldpte,
						    (oldpte & ~PTE_D) | PTE_RO))
							oldpte = *pte;
						vm_page_dirty(m);
						pmap_invalidate_page(pmap, va);
					}
				}
			}
		}
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (md_gen != m->md.pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT(!pde_is_superpage(pde), ("pmap_clear_modify: found"
		    " a superpage in page %p's pv list", m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if (pte_test(pte, PTE_D) && !pte_test(pte, PTE_RO)) {
			pte_atomic_clear(pte, PTE_D);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
}

/*
 * Miscellaneous support routines follow
 */

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 *
 * Use XKPHYS uncached for 64 bit.
 */
void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{

	return ((void *)MIPS_PHYS_TO_DIRECT_UNCACHED(pa));
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{

	/* Nothing to do for mips64. */
}

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	/*
	 * Set the memattr field so the appropriate bits are set in the
	 * PTE as mappings are created.
	 */

	/* Clean memattr portion of pv_flags */
	m->md.pv_flags &= ~PV_MEMATTR_MASK;
	m->md.pv_flags |= (ma << PV_MEMATTR_SHIFT) & PV_MEMATTR_MASK;

	/*
	 * It is assumed that this function is only called before any mappings
	 * are established.  If this is not the case then this function will
	 * need to walk the pv_list and make each of the existing mappings
	 * uncacheable, sync the cache (with mips_icache_sync_all() and
	 * mips_dcache_wbinv_all()) and most likely invalidate TLB entries for
	 * any of the current mappings it modifies.
	 */
	if (TAILQ_FIRST(&m->md.pv_list) != NULL)
		panic("Can't change memattr on page with existing mappings");
}

static inline void
pmap_pte_attr(pt_entry_t *pte, vm_memattr_t ma)
{
	u_int npte;

	npte = *(u_int *)pte;
	npte &= ~PTE_C_MASK;
	npte |= PTE_C(ma);
	*pte = npte;
}

/*
 * Changes the specified virtual address range's memory attribute to that
 * given by the parameter "ma".  The specified virtual address range must be
 * completely contained within the kernel map.
 *
 * Returns zero if the change completed successfully, and either EINVAL or
 * ENOMEM if the change failed.  Specifically, EINVAL is returned if some part
 * of the virtual address range was not mapped, and ENOMEM is returned if
 * there was insufficient memory available to complete the change.  In the
 * latter case, the memory type may have been changed on some part of the
 * virtual address range or the direct map.
 */
int
pmap_change_attr(vm_offset_t sva, vm_size_t size, vm_memattr_t ma)
{
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;
	vm_offset_t ova, eva, va, va_next;

	ova = sva;
	eva = sva + size;
	if (eva < sva)
		return (EINVAL);

	PMAP_LOCK(kernel_pmap);

	for (; sva < eva; sva = va_next) {
		pdpe = pmap_segmap(kernel_pmap, sva);
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		if (*pde == NULL)
			continue;

		if (pde_is_1m_superpage(pde)) {
			if (pte_cache_bits(pte) == ma) {
				/*
				 * Superpage already has the required memory type
				 * so we don't need to demote it. Increment to the
				 * next superpage frame.
				 */
				va_next = (sva + NBPDR) & ~PDRMASK;
				if (va_next < sva)
					va_next = eva;
				continue;
			}

			if ((sva & PDRMASK) == 0 && (sva + PDRMASK < eva)) {
				/*
				 * Aligns with the superpage frame and there
				 * is at least a superpage left in the range.
				 * No need to break down into 4K pages.
				 */
				va_next = sva + NBPDR;
				continue;
			}


			if (!pmap_demote_pde(kernel_pmap, pde, sva)) {
				PMAP_UNLOCK(kernel_pmap);
				return (ENOMEM);
			}
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if (!pte_test(pte, PTE_VALID) || pte_cache_bits(pte) == ma) {
				if (va != va_next) {
					pmap_invalidate_range(kernel_pmap, va, sva);
					va = va_next;
				}
				continue;
			}
			if (va == va_next)
				va = sva;

			pmap_pte_attr(pte, ma);
		}
		if (va != va_next)
			pmap_invalidate_range(kernel_pmap, va, sva);
	}
	PMAP_UNLOCK(kernel_pmap);

	/* Flush caches to be in the safe side */
	mips_dcache_wbinv_range(ova, size);
	return 0;
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pd_entry_t *pdep;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	int val;

	PMAP_LOCK(pmap);
retry:
	pdep = pmap_pde(pmap, addr);
	if (pdep != NULL) {
		if (pde_is_1m_superpage(pdep)) {
			pte = (pt_entry_t)*pdep;
			pa = TLBLO_PTE_TO_PA(pte);
			val = MINCORE_SUPER;
		} else {
			ptep = pmap_pde_to_pte(pdep, addr);
			pte = (ptep != NULL) ? *ptep : 0;
			pa = TLBLO_PTE_TO_PA(pte);
			val = 0;
		}
	} else {
		pte = 0;
		pa = 0;
		val = 0;
	}
	if (pte_is_valid(&pte)) {
		val |= MINCORE_INCORE;
		if (pte_test(&pte, PTE_D))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if (pte_is_ref(&pte))
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	}
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) &&
	    pte_test(&pte, PTE_MANAGED)) {
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
	pmap_t pmap, oldpmap;
	struct proc *p = td->td_proc;
	u_int cpuid;

	critical_enter();

	pmap = vmspace_pmap(p->p_vmspace);
	oldpmap = PCPU_GET(curpmap);
	cpuid = PCPU_GET(cpuid);

	if (oldpmap)
		CPU_CLR_ATOMIC(cpuid, &oldpmap->pm_active);
	CPU_SET_ATOMIC(cpuid, &pmap->pm_active);
	pmap_asid_alloc(pmap);
	if (td == curthread) {
		PCPU_SET(segbase, pmap->pm_segtab);
		mips_wr_entryhi(pmap->pm_asid[cpuid].asid);
	}

	PCPU_SET(curpmap, pmap);
	critical_exit();
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
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

	if (size < PDRSIZE)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & PDRMASK;
	if (size - ((PDRSIZE - superpage_offset) & PDRMASK) < PDRSIZE ||
	    (*addr & PDRMASK) == superpage_offset)
		return;
	if ((*addr & PDRMASK) < superpage_offset)
		*addr = (*addr & ~PDRMASK) + superpage_offset;
	else
		*addr = ((*addr + PDRMASK) & ~PDRMASK) + superpage_offset;
}

#ifdef DDB
DB_SHOW_COMMAND(ptable, ddb_pid_dump)
{
	pmap_t pmap;
	struct thread *td = NULL;
	struct proc *p;
	int i, j, k;
	vm_paddr_t pa;
	vm_offset_t va;

	if (have_addr) {
		td = db_lookup_thread(addr, TRUE);
		if (td == NULL) {
			db_printf("Invalid pid or tid");
			return;
		}
		p = td->td_proc;
		if (p->p_vmspace == NULL) {
			db_printf("No vmspace for process");
			return;
		}
			pmap = vmspace_pmap(p->p_vmspace);
	} else
		pmap = kernel_pmap;

	db_printf("pmap:%p segtab:%p asid:%x generation:%x\n",
	    pmap, pmap->pm_segtab, pmap->pm_asid[0].asid,
	    pmap->pm_asid[0].gen);
	for (i = 0; i < NPDEPG; i++) {
		pd_entry_t *pdpe;
		pt_entry_t *pde;
		pt_entry_t pte;

		pdpe = (pd_entry_t *)pmap->pm_segtab[i];
		if (pdpe == NULL)
			continue;
		db_printf("[%4d] %p\n", i, pdpe);
		for (j = 0; j < NPDEPG; j++) {
			pde = (pt_entry_t *)pdpe[j];
			if (pde == NULL)
				continue;
			db_printf("\t[%4d] %p\n", j, pde);
			for (k = 0; k < NPTEPG; k++) {
				pte = pde[k];
				if (pte == 0 || !pte_is_valid(&pte))
					continue;
				pa = TLBLO_PTE_TO_PA(pte);
				va = ((u_long)i << SEGSHIFT) | (j << PDRSHIFT) | (k << PAGE_SHIFT);
				db_printf("\t\t[%04d] va: %p pte: %8jx pa:%jx\n",
				       k, (void *)va, (uintmax_t)pte, (uintmax_t)pa);
			}
		}
	}
}
#endif

#if defined(DEBUG)

static void pads(pmap_t pm);
void pmap_pvdump(vm_offset_t pa);

/* print address space of pmap*/
static void
pads(pmap_t pm)
{
	unsigned va, i, j;
	pt_entry_t *ptep;

	if (pm == kernel_pmap)
		return;
	for (i = 0; i < NPTEPG; i++)
		if (pm->pm_segtab[i])
			for (j = 0; j < NPTEPG; j++) {
				va = (i << SEGSHIFT) + (j << PAGE_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
					continue;
				if (pm != kernel_pmap &&
				    va >= VM_MAXUSER_ADDRESS)
					continue;
				ptep = pmap_pte(pm, va);
				if (pte_is_valid(ptep))
					printf("%x:%x ", va, *(int *)ptep);
			}

}

void
pmap_pvdump(vm_offset_t pa)
{
	register pv_entry_t pv;
	vm_page_t m;

	printf("pa %x", pa);
	m = PHYS_TO_VM_PAGE(pa);
	for (pv = TAILQ_FIRST(&m->md.pv_list); pv;
	    pv = TAILQ_NEXT(pv, pv_list)) {
		printf(" -> pmap %p, va %x", (void *)pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}

/* N/C */
#endif


/*
 * Allocate TLB address space tag (called ASID or TLBPID) and return it.
 * It takes almost as much or more time to search the TLB for a
 * specific ASID and flush those entries as it does to flush the entire TLB.
 * Therefore, when we allocate a new ASID, we just take the next number. When
 * we run out of numbers, we flush the TLB, increment the generation count
 * and start over. ASID zero is reserved for kernel use.
 */
static void
pmap_asid_alloc(pmap)
	pmap_t pmap;
{
	if (pmap->pm_asid[PCPU_GET(cpuid)].asid != PMAP_ASID_RESERVED &&
	    pmap->pm_asid[PCPU_GET(cpuid)].gen == PCPU_GET(asid_generation));
	else {
		if (PCPU_GET(next_asid) == pmap_max_asid) {
			tlb_invalidate_all_user(NULL);
			PCPU_SET(asid_generation,
			    (PCPU_GET(asid_generation) + 1) & ASIDGEN_MASK);
			if (PCPU_GET(asid_generation) == 0) {
				PCPU_SET(asid_generation, 1);
			}
			PCPU_SET(next_asid, 1);	/* 0 means invalid */
		}
		pmap->pm_asid[PCPU_GET(cpuid)].asid = PCPU_GET(next_asid);
		pmap->pm_asid[PCPU_GET(cpuid)].gen = PCPU_GET(asid_generation);
		PCPU_SET(next_asid, PCPU_GET(next_asid) + 1);
	}
}

static pt_entry_t
init_pte_prot(vm_page_t m, vm_prot_t access, vm_prot_t prot)
{
	pt_entry_t rw;

	if (!(prot & VM_PROT_WRITE))
		rw = PTE_VALID | PTE_RO;
	else if ((m->oflags & VPO_UNMANAGED) == 0) {
		if ((access & VM_PROT_WRITE) != 0)
			rw = PTE_VALID | PTE_D;
		else
			rw = PTE_VALID;
	} else {
		/*
		 * Needn't emulate a reference/modified bit for unmanaged
		 * pages.
		 */
		rw = PTE_VALID | PTE_D;
		pte_ref_set(&rw);
	}

	return (rw);
}

/*
 * pmap_emulate_modified : do dirty bit emulation
 *
 * On SMP, update just the local TLB, other CPUs will update their
 * TLBs from PTE lazily, if they get the exception.
 * Returns 0 in case of sucess, 1 if the page is read only and we
 * need to fault.
 */
int
pmap_emulate_modified(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t *pte;

	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, va);
	if (pde_is_1m_superpage(pde))
		pte = (pt_entry_t *)pde;
	else
		pte = pmap_pde_to_pte(pde, va);
	if (pte == NULL)
		panic("pmap_emulate_modified: can't find PTE");
#ifdef SMP
	/* It is possible that some other CPU changed m-bit */
	if (!pte_is_valid(pte) || pte_test(pte, PTE_D)) {
		tlb_update(pmap, va, *pte);
		PMAP_UNLOCK(pmap);
		return (0);
	}
#else
	if (!pte_is_valid(pte) || pte_test(pte, PTE_D)) {
		tlb_update(pmap, va, *pte);
		PMAP_UNLOCK(pmap);
		return (0);
	}
#endif
	if (pte_test(pte, PTE_RO)) {
		PMAP_UNLOCK(pmap);
		return (1);
	}
	pte_atomic_set(pte, PTE_D); /* mark it referenced and modified */
	pte_ref_atomic_set(pte);
	tlb_update(pmap, va, *pte);
	if (!pte_test(pte, PTE_MANAGED))
		panic("pmap_emulate_modified: unmanaged page");
	PMAP_UNLOCK(pmap);
	return (0);
}

/*
 * pmap_emulate_referenced: do reference bit emulation
 *
 * Returns 0 in case of success.  Returns 1 if we need to fault.
 */
int
pmap_emulate_referenced(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t *pte;

	if (is_kernel_pmap(pmap))
		return (1);
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, va);
	if (pde == NULL || *pde == NULL) {
		/* Invalid page directory. */
		goto dofault;
	}
	if (pde_is_1m_superpage(pde)) {
		pte = (pt_entry_t *)pde;
	} else
		pte = pmap_pde_to_pte(pde, va);
	if (pte == NULL) {
		/* Invalid page table. */
		goto dofault;
	}
	if (!pte_is_valid(pte)) {
		/* Invalid PTE. */
		goto dofault;
	}
	/* Check to see if already marked by other CPU.  */
	if (!pte_is_ref(pte))
		pte_ref_atomic_set(pte);

	tlb_update(pmap, va, *pte);
	PMAP_UNLOCK(pmap);

	return (0);

dofault:
	PMAP_UNLOCK(pmap);
	return (1);
}

void
pmap_flush_pvcache(vm_page_t m)
{
	pv_entry_t pv;

	if (m != NULL) {
		for (pv = TAILQ_FIRST(&m->md.pv_list); pv;
		    pv = TAILQ_NEXT(pv, pv_next)) {
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);
		}
	}
}
