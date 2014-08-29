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

#define	AMD64_NPT_AWARE

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

//#include "opt_pmap.h"
//#include "opt_vm.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
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
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/uma.h>

#include <machine/machdep.h>
#include <machine/pcb.h>

#if 0
#include <machine/intr_machdep.h>
#include <x86/apicvar.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#endif

#define	NPDEPG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NUPDE			(NPDEPG * NPDEPG)
#define	NUSERPGTBLS		(NUPDE + NPDEPG)

#if 0
static __inline boolean_t
pmap_emulate_ad_bits(pmap_t pmap)
{

	return ((pmap->pm_flags & PMAP_EMULATE_AD_BITS) != 0);
}

static __inline pt_entry_t
pmap_valid_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = X86_PG_V;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_EMUL_V;
		else
			mask = EPT_PG_READ;
		break;
	default:
		panic("pmap_valid_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_rw_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = X86_PG_RW;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_EMUL_RW;
		else
			mask = EPT_PG_WRITE;
		break;
	default:
		panic("pmap_rw_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_global_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = X86_PG_G;
		break;
	case PT_EPT:
		mask = 0;
		break;
	default:
		panic("pmap_global_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_accessed_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = X86_PG_A;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_READ;
		else
			mask = EPT_PG_A;
		break;
	default:
		panic("pmap_accessed_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_modified_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = X86_PG_M;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_WRITE;
		else
			mask = EPT_PG_M;
		break;
	default:
		panic("pmap_modified_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

#endif /* 0 */

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

#if 0
#define	pa_index(pa)	((pa) >> PDRSHIFT)
#define	pa_to_pvh(pa)	(&pv_table[pa_index(pa)])
#endif /* 0 */

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

#if 0
int nkpt;
SYSCTL_INT(_machdep, OID_AUTO, nkpt, CTLFLAG_RD, &nkpt, 0,
    "Number of kernel page table pages allocated on bootup");

static int ndmpdp;
vm_paddr_t dmaplimit;
vm_offset_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;
pt_entry_t pg_nx;

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

static int pat_works = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pat_works, CTLFLAG_RD, &pat_works, 1,
    "Is page attribute table fully functional?");

static int pg_ps_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pg_ps_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pg_ps_enabled, 0, "Are large page mappings enabled?");

#define	PAT_INDEX_SIZE	8
static int pat_index[PAT_INDEX_SIZE];	/* cache mode to PAT index conversion */

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
u_int64_t		KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */

static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */
static int		ndmpdpphys;	/* number of DMPDPphys pages */
#endif

static struct rwlock_padalign pvh_global_lock;

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static struct mtx pv_chunks_mutex;
static struct rwlock pv_list_locks[NPV_LIST_LOCKS];
#if 0
static struct md_page *pv_table;

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1 = 0;
caddr_t CADDR1 = 0;

static int pmap_flags = PMAP_PDE_SUPERPAGE;	/* flags for x86 pmaps */

static struct unrhdr pcid_unr;
static struct mtx pcid_mtx;
int pmap_pcid_enabled = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, pcid_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pmap_pcid_enabled, 0, "Is TLB Context ID enabled ?");
int invpcid_works = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, invpcid_works, CTLFLAG_RD, &invpcid_works, 0,
    "Is the invpcid instruction available ?");

static int
pmap_pcid_save_cnt_proc(SYSCTL_HANDLER_ARGS)
{
	int i;
	uint64_t res;

	res = 0;
	CPU_FOREACH(i) {
		res += cpuid_to_pcpu[i]->pc_pm_save_cnt;
	}
	return (sysctl_handle_64(oidp, &res, 0, req));
}
SYSCTL_PROC(_vm_pmap, OID_AUTO, pcid_save_cnt, CTLTYPE_U64 | CTLFLAG_RW |
    CTLFLAG_MPSAFE, NULL, 0, pmap_pcid_save_cnt_proc, "QU",
    "Count of saved TLB context on switch");

/* pmap_copy_pages() over non-DMAP */
static struct mtx cpage_lock;
static vm_offset_t cpage_a;
static vm_offset_t cpage_b;

/*
 * Crashdump maps.
 */
static caddr_t crashdumpmap;
#endif /* 0 */

static void	free_pv_chunk(struct pv_chunk *pc);
static void	free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t pmap, struct rwlock **lockp);
#if 0
static int	popcnt_pc_map_elem(uint64_t elem);
#endif
static vm_page_t reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp);
#if 0
static void	reserve_pv_entries(pmap_t pmap, int needed,
		    struct rwlock **lockp);
static void	pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
		    struct rwlock **lockp);
static boolean_t pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
		    struct rwlock **lockp);
static void	pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
		    struct rwlock **lockp);
#endif /* 0 */
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
		    vm_offset_t va);
#if 0

static int pmap_change_attr_locked(vm_offset_t va, vm_size_t size, int mode);
static boolean_t pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va);
static boolean_t pmap_demote_pde_locked(pmap_t pmap, pd_entry_t *pde,
    vm_offset_t va, struct rwlock **lockp);
static boolean_t pmap_demote_pdpe(pmap_t pmap, pdp_entry_t *pdpe,
    vm_offset_t va);
static boolean_t pmap_enter_pde(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, struct rwlock **lockp);
#endif /* 0 */
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp);
#if 0
static void pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte);
static int pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte);
static void pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode);
static vm_page_t pmap_lookup_pt_page(pmap_t pmap, vm_offset_t va);
static void pmap_pde_attr(pd_entry_t *pde, int cache_bits, int mask);
static void pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp);
static boolean_t pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva,
    vm_prot_t prot);
static void pmap_pte_attr(pt_entry_t *pte, int cache_bits, int mask);
static int pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp);
#endif /* 0 */
static int pmap_remove_l3(pmap_t pmap, pt_entry_t *l3, vm_offset_t sva,
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp);
#if 0
static void pmap_remove_pt_page(pmap_t pmap, vm_page_t mpte);
static void pmap_remove_page(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    struct spglist *free);
#endif /* 0 */
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m, struct rwlock **lockp);
#if 0
static void pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    pd_entry_t newpde);
static void pmap_update_pde_invalidate(pmap_t, vm_offset_t va, pd_entry_t pde);
#endif /* 0 */

static vm_page_t _pmap_alloc_l3(pmap_t pmap, vm_pindex_t ptepindex,
		struct rwlock **lockp);
#if 0
static vm_page_t pmap_allocpde(pmap_t pmap, vm_offset_t va,
		struct rwlock **lockp);
static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va,
		struct rwlock **lockp);
#endif /* 0 */

static void _pmap_unwire_l3(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free);
static int pmap_unuse_l3(pmap_t, vm_offset_t, pd_entry_t, struct spglist *);
#if 0
static vm_offset_t pmap_kmem_choose(vm_offset_t addr);

/*
 * Move the kernel virtual free pointer to the next
 * 2MB.  This is used to help improve performance
 * by using a large (2MB) page for much of the kernel
 * (.text, .data, .bss)
 */
static vm_offset_t
pmap_kmem_choose(vm_offset_t addr)
{
	vm_offset_t newaddr = addr;

	newaddr = (addr + (NBPDR - 1)) & ~(NBPDR - 1);
	return (newaddr);
}
#endif /* 0 */

/********************/
/* Inline functions */
/********************/

static __inline void
pagecopy(void *s, void *d)
{

	memcpy(d, s, PAGE_SIZE);
}

static __inline void
pagezero(void *p)
{

	bzero(p, PAGE_SIZE);
}

#define	pmap_l1_index(va)	(((va) >> L1_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l2_index(va)	(((va) >> L2_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l3_index(va)	(((va) >> L3_SHIFT) & Ln_ADDR_MASK)

static __inline pd_entry_t *
pmap_l1(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_l1[pmap_l1_index(va)]);
}

static __inline pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = (pd_entry_t *)PHYS_TO_DMAP(*l1 & ~ATTR_MASK);
	return (&l2[pmap_l2_index(va)]);
}

static __inline pd_entry_t *
pmap_l2(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = pmap_l1(pmap, va);
	if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
		return (NULL);

	return (pmap_l1_to_l2(l1, va));
}

static __inline pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
	pt_entry_t *l3;

	l3 = (pd_entry_t *)PHYS_TO_DMAP(*l2 & ~ATTR_MASK);
	return (&l3[pmap_l3_index(va)]);
}

static __inline pt_entry_t *
pmap_l3(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = pmap_l2(pmap, va);
	if (l2 == NULL || (*l2 & ATTR_DESCR_MASK) != L2_TABLE)
		return (NULL);

	return (pmap_l2_to_l3(l2, va));
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

#if 0
PMAP_INLINE pt_entry_t *
vtopte(vm_offset_t va)
{
	u_int64_t mask = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT + NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1);

	KASSERT(va >= VM_MAXUSER_ADDRESS, ("vtopte on a uva/gpa 0x%0lx", va));

	return (PTmap + ((va >> PAGE_SHIFT) & mask));
}

static __inline pd_entry_t *
vtopde(vm_offset_t va)
{
	u_int64_t mask = ((1ul << (NPDEPGSHIFT + NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1);

	KASSERT(va >= VM_MAXUSER_ADDRESS, ("vtopde on a uva/gpa 0x%0lx", va));

	return (PDmap + ((va >> PDRSHIFT) & mask));
}

CTASSERT(powerof2(NDMPML4E));

/* number of kernel PDP slots */
#define	NKPDPE(ptpgs)		howmany((ptpgs), NPDEPG)

static void
nkpt_init(vm_paddr_t addr)
{
	int pt_pages;
	
#ifdef NKPT
	pt_pages = NKPT;
#else
	pt_pages = howmany(addr, 1 << PDRSHIFT);
	pt_pages += NKPDPE(pt_pages);

	/*
	 * Add some slop beyond the bare minimum required for bootstrapping
	 * the kernel.
	 *
	 * This is quite important when allocating KVA for kernel modules.
	 * The modules are required to be linked in the negative 2GB of
	 * the address space.  If we run out of KVA in this region then
	 * pmap_growkernel() will need to allocate page table pages to map
	 * the entire 512GB of KVA space which is an unnecessary tax on
	 * physical memory.
	 */
	pt_pages += 8;		/* 16MB additional slop for kernel modules */
#endif
	nkpt = pt_pages;
}

static void
create_pagetables(vm_paddr_t *firstaddr)
{
	int i, j, ndm1g, nkpdpe;
	pt_entry_t *pt_p;
	pd_entry_t *pd_p;
	pdp_entry_t *pdp_p;
	pml4_entry_t *p4_p;

	/* Allocate page table pages for the direct map */
	ndmpdp = (ptoa(Maxmem) + NBPDP - 1) >> PDPSHIFT;
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	ndmpdpphys = howmany(ndmpdp, NPDPEPG);
	if (ndmpdpphys > NDMPML4E) {
		/*
		 * Each NDMPML4E allows 512 GB, so limit to that,
		 * and then readjust ndmpdp and ndmpdpphys.
		 */
		printf("NDMPML4E limits system to %d GB\n", NDMPML4E * 512);
		Maxmem = atop(NDMPML4E * NBPML4);
		ndmpdpphys = NDMPML4E;
		ndmpdp = NDMPML4E * NPDEPG;
	}
	DMPDPphys = allocpages(firstaddr, ndmpdpphys);
	ndm1g = 0;
	if ((amd_feature & AMDID_PAGE1GB) != 0)
		ndm1g = ptoa(Maxmem) >> PDPSHIFT;
	if (ndm1g < ndmpdp)
		DMPDphys = allocpages(firstaddr, ndmpdp - ndm1g);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;

	/* Allocate pages */
	KPML4phys = allocpages(firstaddr, 1);
	KPDPphys = allocpages(firstaddr, NKPML4E);

	/*
	 * Allocate the initial number of kernel page table pages required to
	 * bootstrap.  We defer this until after all memory-size dependent
	 * allocations are done (e.g. direct map), so that we don't have to
	 * build in too much slop in our estimate.
	 *
	 * Note that when NKPML4E > 1, we have an empty page underneath
	 * all but the KPML4I'th one, so we need NKPML4E-1 extra (zeroed)
	 * pages.  (pmap_enter requires a PD page to exist for each KPML4E.)
	 */
	nkpt_init(*firstaddr);
	nkpdpe = NKPDPE(nkpt);

	KPTphys = allocpages(firstaddr, nkpt);
	KPDphys = allocpages(firstaddr, nkpdpe);

	/* Fill in the underlying page table pages */
	/* Nominally read-only (but really R/W) from zero to physfree */
	/* XXX not fully used, underneath 2M pages */
	pt_p = (pt_entry_t *)KPTphys;
	for (i = 0; ptoa(i) < *firstaddr; i++)
		pt_p[i] = ptoa(i) | X86_PG_RW | X86_PG_V | X86_PG_G;

	/* Now map the page tables at their location within PTmap */
	pd_p = (pd_entry_t *)KPDphys;
	for (i = 0; i < nkpt; i++)
		pd_p[i] = (KPTphys + ptoa(i)) | X86_PG_RW | X86_PG_V;

	/* Map from zero to end of allocations under 2M pages */
	/* This replaces some of the KPTphys entries above */
	for (i = 0; (i << PDRSHIFT) < *firstaddr; i++)
		pd_p[i] = (i << PDRSHIFT) | X86_PG_RW | X86_PG_V | PG_PS |
		    X86_PG_G;

	/* And connect up the PD to the PDP (leaving room for L4 pages) */
	pdp_p = (pdp_entry_t *)(KPDPphys + ptoa(KPML4I - KPML4BASE));
	for (i = 0; i < nkpdpe; i++)
		pdp_p[i + KPDPI] = (KPDphys + ptoa(i)) | X86_PG_RW | X86_PG_V |
		    PG_U;

	/*
	 * Now, set up the direct map region using 2MB and/or 1GB pages.  If
	 * the end of physical memory is not aligned to a 1GB page boundary,
	 * then the residual physical memory is mapped with 2MB pages.  Later,
	 * if pmap_mapdev{_attr}() uses the direct map for non-write-back
	 * memory, pmap_change_attr() will demote any 2MB or 1GB page mappings
	 * that are partially used. 
	 */
	pd_p = (pd_entry_t *)DMPDphys;
	for (i = NPDEPG * ndm1g, j = 0; i < NPDEPG * ndmpdp; i++, j++) {
		pd_p[j] = (vm_paddr_t)i << PDRSHIFT;
		/* Preset PG_M and PG_A because demotion expects it. */
		pd_p[j] |= X86_PG_RW | X86_PG_V | PG_PS | X86_PG_G |
		    X86_PG_M | X86_PG_A;
	}
	pdp_p = (pdp_entry_t *)DMPDPphys;
	for (i = 0; i < ndm1g; i++) {
		pdp_p[i] = (vm_paddr_t)i << PDPSHIFT;
		/* Preset PG_M and PG_A because demotion expects it. */
		pdp_p[i] |= X86_PG_RW | X86_PG_V | PG_PS | X86_PG_G |
		    X86_PG_M | X86_PG_A;
	}
	for (j = 0; i < ndmpdp; i++, j++) {
		pdp_p[i] = DMPDphys + ptoa(j);
		pdp_p[i] |= X86_PG_RW | X86_PG_V | PG_U;
	}

	/* And recursively map PML4 to itself in order to get PTmap */
	p4_p = (pml4_entry_t *)KPML4phys;
	p4_p[PML4PML4I] = KPML4phys;
	p4_p[PML4PML4I] |= X86_PG_RW | X86_PG_V | PG_U;

	/* Connect the Direct Map slot(s) up to the PML4. */
	for (i = 0; i < ndmpdpphys; i++) {
		p4_p[DMPML4I + i] = DMPDPphys + ptoa(i);
		p4_p[DMPML4I + i] |= X86_PG_RW | X86_PG_V | PG_U;
	}

	/* Connect the KVA slots up to the PML4 */
	for (i = 0; i < NKPML4E; i++) {
		p4_p[KPML4BASE + i] = KPDPphys + ptoa(i);
		p4_p[KPML4BASE + i] |= X86_PG_RW | X86_PG_V | PG_U;
	}
}
#endif /* 0 */

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
	l2 = (pt_entry_t *)PHYS_TO_DMAP(l1[*l1_slot] & ~ATTR_MASK);
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
pmap_bootstrap_dmap(vm_offset_t l1pt)
{
	vm_offset_t va;
	vm_paddr_t pa;
	pd_entry_t *l1;
	u_int l1_slot;

	va = DMAP_MIN_ADDRESS;
	l1 = (pd_entry_t *)l1pt;
	l1_slot = pmap_l1_index(DMAP_MIN_ADDRESS);

	for (pa = 0; va < DMAP_MAX_ADDRESS;
	    pa += L1_SIZE, va += L1_SIZE, l1_slot++) {
		KASSERT(l1_slot < Ln_ENTRIES, ("Invalid L1 index"));

		/*
		 * TODO: Turn the cache on here when we have cache
		 * flushing code.
		 */
		l1[l1_slot] = (pa & ~L1_OFFSET) | ATTR_AF | L1_BLOCK |
		    ATTR_IDX(UNCACHED_MEMORY);
	}
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
		l1[l1_slot] = (pa & ~Ln_TABLE_MASK) | L1_TABLE;
		l2pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l2_start, 0, l2pt - l2_start);

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
	l2 = (pd_entry_t *)((uintptr_t)l2 & ~(PAGE_SIZE - 1));
	l2pt = (vm_offset_t)l2;
	l2_slot = pmap_l2_index(va);
	l3pt = l3_start;

	for (; va < VM_MAX_KERNEL_ADDRESS; l2_slot++, va += L2_SIZE) {
		KASSERT(l2_slot < Ln_ENTRIES, ("Invalid L2 index"));

		pa = pmap_early_vtophys(l1pt, l3pt);
		l2[l2_slot] = (pa & ~Ln_TABLE_MASK) | L2_TABLE;
		l3pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l3_start, 0, l3pt - l3_start);

	return l3pt;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t l1pt, vm_paddr_t kernstart, vm_size_t kernlen)
{
	u_int l1_slot, l2_slot, avail_slot, map_slot, used_map_slot;
	uint64_t kern_delta;
	pt_entry_t *l2;
	vm_offset_t va, freemempos;
	vm_offset_t dpcpu;
	vm_paddr_t pa;

	kern_delta = KERNBASE - kernstart;
	physmem = 0;

	printf("pmap_bootstrap %llx %llx %llx\n", l1pt, kernstart, kernlen);
	printf("%llx\n", l1pt);
	printf("%lx\n", (KERNBASE >> L1_SHIFT) & Ln_ADDR_MASK);

	/* Set this early so we can use the pagetable walking functions */
	kernel_pmap_store.pm_l1 = (pd_entry_t *)l1pt;
	PMAP_LOCK_INIT(kernel_pmap);

 	/*
	 * Initialize the global pv list lock.
	 */
	rw_init(&pvh_global_lock, "pmap pv global");

	/* Create a direct map region early so we can use it for pa -> va */
	pmap_bootstrap_dmap(l1pt);

	va = KERNBASE;
	pa = KERNBASE - kern_delta;

	/*
	 * Start to initialise phys_avail by copying from physmap
	 * up to the physical address KERNBASE points at.
	 */
	map_slot = avail_slot = 0;
	for (; map_slot < (physmap_idx * 2); map_slot += 2) {
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
	if (physmap[avail_slot] < pa) {
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

	/* And map the rest of L2 table */
	for (; l2_slot < Ln_ENTRIES; l2_slot++) {
		KASSERT(l2[l2_slot] == 0, ("Invalid bootstrap L2 table"));
		KASSERT(((va >> L2_SHIFT) & Ln_ADDR_MASK) == l2_slot,
		    ("VA inconsistency detected"));

		/*
		 * Check if we can use the current pa, some of it
		 * may fall out side the current physmap slot.
		 */
		if (pa + L2_SIZE > physmap[map_slot] + physmap[map_slot + 1]) {
			map_slot += 2;
			pa = physmap[map_slot];
			pa = roundup2(pa, L2_SIZE);

			/* TODO: should we wrap if we hit this? */
			KASSERT(map_slot < physmap_idx,
			    ("Attempting to use invalid physical memory"));
			/* TODO: This should be easy to fix */
			KASSERT(pa + L2_SIZE <
			    physmap[map_slot] + physmap[map_slot + 1],
			    ("Physical slot too small"));
		}

		/*
		 * TODO: Turn the cache on here when we have cache
		 * flushing code.
		 */
		l2[l2_slot] = (pa & ~L2_OFFSET) | ATTR_AF | L2_BLOCK |
		    ATTR_IDX(UNCACHED_MEMORY);

		va += L2_SIZE;
		pa += L2_SIZE;
	}

	freemempos = KERNBASE + kernlen;
	freemempos = roundup2(freemempos, PAGE_SIZE);
	/* Create the l2 tables up to VM_MAX_KERNEL_ADDRESS */
	freemempos = pmap_bootstrap_l2(l1pt, va, freemempos);
	/* And the l3 tables for the early devmap */
	freemempos = pmap_bootstrap_l3(l1pt,
	    VM_MAX_KERNEL_ADDRESS - L2_SIZE, freemempos);

	/* Flush the cache and tlb to ensure the new entries are valid */
	/* TODO: Flush the cache, we are relying on it being off */
	/* TODO: Move this to a function */
	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vmalle1	\n"
	    "dsb  sy		\n"
	    "isb		\n");

#define alloc_pages(var, np)						\
	(var) = freemempos;						\
	freemempos += (np * PAGE_SIZE);					\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	/* Allocate dynamic per-cpu area. */
	alloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu, 0);

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

	/* Flush the cache and tlb to ensure the new entries are valid */
	/* TODO: Flush the cache, we are relying on it being off */
	/* TODO: Move this to a function */
	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vmalle1is	\n"
	    "dsb  sy		\n"
	    "isb		\n");
}

#if 0
/*
 * Setup the PAT MSR.
 */
void
pmap_init_pat(void)
{
	int pat_table[PAT_INDEX_SIZE];
	uint64_t pat_msr;
	u_long cr0, cr4;
	int i;

	/* Bail if this CPU doesn't implement PAT. */
	if ((cpu_feature & CPUID_PAT) == 0)
		panic("no PAT??");

	/* Set default PAT index table. */
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		pat_table[i] = -1;
	pat_table[PAT_WRITE_BACK] = 0;
	pat_table[PAT_WRITE_THROUGH] = 1;
	pat_table[PAT_UNCACHEABLE] = 3;
	pat_table[PAT_WRITE_COMBINING] = 3;
	pat_table[PAT_WRITE_PROTECTED] = 3;
	pat_table[PAT_UNCACHED] = 3;

	/* Initialize default PAT entries. */
	pat_msr = PAT_VALUE(0, PAT_WRITE_BACK) |
	    PAT_VALUE(1, PAT_WRITE_THROUGH) |
	    PAT_VALUE(2, PAT_UNCACHED) |
	    PAT_VALUE(3, PAT_UNCACHEABLE) |
	    PAT_VALUE(4, PAT_WRITE_BACK) |
	    PAT_VALUE(5, PAT_WRITE_THROUGH) |
	    PAT_VALUE(6, PAT_UNCACHED) |
	    PAT_VALUE(7, PAT_UNCACHEABLE);

	if (pat_works) {
		/*
		 * Leave the indices 0-3 at the default of WB, WT, UC-, and UC.
		 * Program 5 and 6 as WP and WC.
		 * Leave 4 and 7 as WB and UC.
		 */
		pat_msr &= ~(PAT_MASK(5) | PAT_MASK(6));
		pat_msr |= PAT_VALUE(5, PAT_WRITE_PROTECTED) |
		    PAT_VALUE(6, PAT_WRITE_COMBINING);
		pat_table[PAT_UNCACHED] = 2;
		pat_table[PAT_WRITE_PROTECTED] = 5;
		pat_table[PAT_WRITE_COMBINING] = 6;
	} else {
		/*
		 * Just replace PAT Index 2 with WC instead of UC-.
		 */
		pat_msr &= ~PAT_MASK(2);
		pat_msr |= PAT_VALUE(2, PAT_WRITE_COMBINING);
		pat_table[PAT_WRITE_COMBINING] = 2;
	}

	/* Disable PGE. */
	cr4 = rcr4();
	load_cr4(cr4 & ~CR4_PGE);

	/* Disable caches (CD = 1, NW = 0). */
	cr0 = rcr0();
	load_cr0((cr0 & ~CR0_NW) | CR0_CD);

	/* Flushes caches and TLBs. */
	wbinvd();
	invltlb();

	/* Update PAT and index table. */
	wrmsr(MSR_PAT, pat_msr);
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		pat_index[i] = pat_table[i];

	/* Flush caches and TLBs again. */
	wbinvd();
	invltlb();

	/* Restore caches and PGE. */
	load_cr0(cr0);
	load_cr4(cr4);
}
#endif /* 0 */

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_memattr = VM_MEMATTR_UNCACHEABLE;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	//vm_page_t mpte;
	//vm_size_t s;
	int i;//, pv_npg;

#if 0
	/*
	 * Initialize the vm page array entries for the kernel pmap's
	 * page table pages.
	 */ 
	for (i = 0; i < nkpt; i++) {
		mpte = PHYS_TO_VM_PAGE(KPTphys + (i << PAGE_SHIFT));
		KASSERT(mpte >= vm_page_array &&
		    mpte < &vm_page_array[vm_page_array_size],
		    ("pmap_init: page table page is out of range"));
		mpte->pindex = pmap_pde_pindex(KERNBASE) + i;
		mpte->phys_addr = KPTphys + (i << PAGE_SHIFT);
	}

	/*
	 * If the kernel is running on a virtual machine, then it must assume
	 * that MCA is enabled by the hypervisor.  Moreover, the kernel must
	 * be prepared for the hypervisor changing the vendor and family that
	 * are reported by CPUID.  Consequently, the workaround for AMD Family
	 * 10h Erratum 383 is enabled if the processor's feature set does not
	 * include at least one feature that is only supported by older Intel
	 * or newer AMD processors.
	 */
	if (vm_guest == VM_GUEST_VM && (cpu_feature & CPUID_SS) == 0 &&
	    (cpu_feature2 & (CPUID2_SSSE3 | CPUID2_SSE41 | CPUID2_AESNI |
	    CPUID2_AVX | CPUID2_XSAVE)) == 0 && (amd_feature2 & (AMDID2_XOP |
	    AMDID2_FMA4)) == 0)
		workaround_erratum383 = 1;

	/*
	 * Are large page mappings enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.pg_ps_enabled", &pg_ps_enabled);
	if (pg_ps_enabled) {
		KASSERT(MAXPAGESIZES > 1 && pagesizes[1] == 0,
		    ("pmap_init: can't assign to pagesizes[1]"));
		pagesizes[1] = NBPDR;
	}
#endif

	/*
	 * Initialize the pv chunk list mutex.
	 */
	mtx_init(&pv_chunks_mutex, "pmap pv chunk list", NULL, MTX_DEF);

	/*
	 * Initialize the pool of pv list locks.
	 */
	for (i = 0; i < NPV_LIST_LOCKS; i++)
		rw_init(&pv_list_locks[i], "pmap pv list");

#if 0
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

	mtx_init(&cpage_lock, "cpage", NULL, MTX_DEF);
	cpage_a = kva_alloc(PAGE_SIZE);
	cpage_b = kva_alloc(PAGE_SIZE);
#endif
}

#if 0
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

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pdpe, CTLFLAG_RD, 0,
    "1GB page mapping counters");

static u_long pmap_pdpe_demotions;
SYSCTL_ULONG(_vm_pmap_pdpe, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_pdpe_demotions, 0, "1GB page demotions");

/***************************************************
 * Low level helper routines.....
 ***************************************************/

static pt_entry_t
pmap_swap_pat(pmap_t pmap, pt_entry_t entry)
{
	int x86_pat_bits = X86_PG_PTE_PAT | X86_PG_PDE_PAT;

	switch (pmap->pm_type) {
	case PT_X86:
		/* Verify that both PAT bits are not set at the same time */
		KASSERT((entry & x86_pat_bits) != x86_pat_bits,
		    ("Invalid PAT bits in entry %#lx", entry));

		/* Swap the PAT bits if one of them is set */
		if ((entry & x86_pat_bits) != 0)
			entry ^= x86_pat_bits;
		break;
	case PT_EPT:
		/*
		 * Nothing to do - the memory attributes are represented
		 * the same way for regular pages and superpages.
		 */
		break;
	default:
		panic("pmap_switch_pat_bits: bad pm_type %d", pmap->pm_type);
	}

	return (entry);
}

/*
 * Determine the appropriate bits to set in a PTE or PDE for a specified
 * caching mode.
 */
static int
pmap_cache_bits(pmap_t pmap, int mode, boolean_t is_pde)
{
	int cache_bits, pat_flag, pat_idx;

	if (mode < 0 || mode >= PAT_INDEX_SIZE || pat_index[mode] < 0)
		panic("Unknown caching mode %d\n", mode);

	switch (pmap->pm_type) {
	case PT_X86:
		/* The PAT bit is different for PTE's and PDE's. */
		pat_flag = is_pde ? X86_PG_PDE_PAT : X86_PG_PTE_PAT;

		/* Map the caching mode to a PAT index. */
		pat_idx = pat_index[mode];

		/* Map the 3-bit index value into the PAT, PCD, and PWT bits. */
		cache_bits = 0;
		if (pat_idx & 0x4)
			cache_bits |= pat_flag;
		if (pat_idx & 0x2)
			cache_bits |= PG_NC_PCD;
		if (pat_idx & 0x1)
			cache_bits |= PG_NC_PWT;
		break;

	case PT_EPT:
		cache_bits = EPT_PG_IGNORE_PAT | EPT_PG_MEMORY_TYPE(mode);
		break;

	default:
		panic("unsupported pmap type %d", pmap->pm_type);
	}

	return (cache_bits);
}

static int
pmap_cache_mask(pmap_t pmap, boolean_t is_pde)
{
	int mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = is_pde ? X86_PG_PDE_CACHE : X86_PG_PTE_CACHE;
		break;
	case PT_EPT:
		mask = EPT_PG_IGNORE_PAT | EPT_PG_MEMORY_TYPE(0x7);
		break;
	default:
		panic("pmap_cache_mask: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline boolean_t
pmap_ps_enabled(pmap_t pmap)
{

	return (pg_ps_enabled && (pmap->pm_flags & PMAP_PDE_SUPERPAGE) != 0);
}

static void
pmap_update_pde_store(pmap_t pmap, pd_entry_t *pde, pd_entry_t newpde)
{

	switch (pmap->pm_type) {
	case PT_X86:
		break;
	case PT_EPT:
		/*
		 * XXX
		 * This is a little bogus since the generation number is
		 * supposed to be bumped up when a region of the address
		 * space is invalidated in the page tables.
		 *
		 * In this case the old PDE entry is valid but yet we want
		 * to make sure that any mappings using the old entry are
		 * invalidated in the TLB.
		 *
		 * The reason this works as expected is because we rendezvous
		 * "all" host cpus and force any vcpu context to exit as a
		 * side-effect.
		 */
		atomic_add_acq_long(&pmap->pm_eptgen, 1);
		break;
	default:
		panic("pmap_update_pde_store: bad pm_type %d", pmap->pm_type);
	}
	pde_store(pde, newpde);
}

/*
 * After changing the page size for the specified virtual address in the page
 * table, flush the corresponding entries from the processor's TLB.  Only the
 * calling processor's TLB is affected.
 *
 * The calling thread must be pinned to a processor.
 */
static void
pmap_update_pde_invalidate(pmap_t pmap, vm_offset_t va, pd_entry_t newpde)
{
	pt_entry_t PG_G;

	if (pmap->pm_type == PT_EPT)
		return;

	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_update_pde_invalidate: invalid type %d", pmap->pm_type));

	PG_G = pmap_global_bit(pmap);

	if ((newpde & PG_PS) == 0)
		/* Demotion: flush a specific 2MB page mapping. */
		invlpg(va);
	else if ((newpde & PG_G) == 0)
		/*
		 * Promotion: flush every 4KB page mapping from the TLB
		 * because there are too many to flush individually.
		 */
		invltlb();
	else {
		/*
		 * Promotion: flush every 4KB page mapping from the TLB,
		 * including any global (PG_G) mappings.
		 */
		invltlb_globpcid();
	}
}
#endif /* 0 */

/*
 * Normal, non-SMP, invalidation functions.
 * We inline these within pmap.c for speed.
 */
PMAP_INLINE void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{

	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vae1, %0	\n"
	    "dsb  sy		\n"
	    "isb		\n"
	    : : "r"(va));
}

PMAP_INLINE void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	if (pmap == kernel_pmap) {
		__asm __volatile("dsb	sy");
		for (addr = sva; addr < eva; addr += PAGE_SIZE) {
			__asm __volatile(
			    "tlbi vae1, %0" : : "r"(addr));
		}
		__asm __volatile(
		    "dsb  sy	\n"
		    "isb	\n");
	}
}

PMAP_INLINE void
pmap_invalidate_all(pmap_t pmap)
{

	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vmalle1	\n"
	    "dsb  sy		\n"
	    "isb		\n");
}

#if 0
PMAP_INLINE void
pmap_invalidate_cache(void)
{

	wbinvd();
}

static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pd_entry_t newpde)
{

	pmap_update_pde_store(pmap, pde, newpde);
	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		pmap_update_pde_invalidate(pmap, va, newpde);
	else
		CPU_ZERO(&pmap->pm_save);
}

#define PMAP_CLFLUSH_THRESHOLD   (2 * 1024 * 1024)

void
pmap_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva)
{

	KASSERT((sva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: sva not page-aligned"));
	KASSERT((eva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: eva not page-aligned"));

	if (cpu_feature & CPUID_SS)
		; /* If "Self Snoop" is supported, do nothing. */
	else if ((cpu_feature & CPUID_CLFSH) != 0 &&
	    eva - sva < PMAP_CLFLUSH_THRESHOLD) {

		/*
		 * XXX: Some CPUs fault, hang, or trash the local APIC
		 * registers if we use CLFLUSH on the local APIC
		 * range.  The local APIC is always uncached, so we
		 * don't need to flush for that range anyway.
		 */
		if (pmap_kextract(sva) == lapic_paddr)
			return;

		/*
		 * Otherwise, do per-cache line flush.  Use the mfence
		 * instruction to insure that previous stores are
		 * included in the write-back.  The processor
		 * propagates flush to other processors in the cache
		 * coherence domain.
		 */
		mfence();
		for (; sva < eva; sva += cpu_clflush_line_size)
			clflush(sva);
		mfence();
	} else {

		/*
		 * No targeted cache flush methods are supported by CPU,
		 * or the supplied range is bigger than 2MB.
		 * Globally invalidate cache.
		 */
		pmap_invalidate_cache();
	}
}

/*
 * Remove the specified set of pages from the data and instruction caches.
 *
 * In contrast to pmap_invalidate_cache_range(), this function does not
 * rely on the CPU's self-snoop feature, because it is intended for use
 * when moving pages into a different cache domain.
 */
void
pmap_invalidate_cache_pages(vm_page_t *pages, int count)
{
	vm_offset_t daddr, eva;
	int i;

	if (count >= PMAP_CLFLUSH_THRESHOLD / PAGE_SIZE ||
	    (cpu_feature & CPUID_CLFSH) == 0)
		pmap_invalidate_cache();
	else {
		mfence();
		for (i = 0; i < count; i++) {
			daddr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pages[i]));
			eva = daddr + PAGE_SIZE;
			for (; daddr < eva; daddr += cpu_clflush_line_size)
				clflush(daddr);
		}
		mfence();
	}
}
#endif /* 0 */

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t 
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	panic("pmap_extract");
#if 0
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	vm_paddr_t pa;

	pa = 0;
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);
	pdpe = pmap_pdpe(pmap, va);
	if (pdpe != NULL && (*pdpe & PG_V) != 0) {
		if ((*pdpe & PG_PS) != 0)
			pa = (*pdpe & PG_PS_FRAME) | (va & PDPMASK);
		else {
			pde = pmap_pdpe_to_pde(pdpe, va);
			if ((*pde & PG_V) != 0) {
				if ((*pde & PG_PS) != 0) {
					pa = (*pde & PG_PS_FRAME) |
					    (va & PDRMASK);
				} else {
					pte = pmap_pde_to_pte(pde, va);
					pa = (*pte & PG_FRAME) |
					    (va & PAGE_MASK);
				}
			}
		}
	}
	PMAP_UNLOCK(pmap);
	return (pa);
#endif
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
	panic("pmap_extract_and_hold");
#if 0
	pd_entry_t pde, *pdep;
	pt_entry_t pte, PG_RW, PG_V;
	vm_paddr_t pa;
	vm_page_t m;

	pa = 0;
	m = NULL;
	PG_RW = pmap_rw_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);
retry:
	pdep = pmap_pde(pmap, va);
	if (pdep != NULL && (pde = *pdep)) {
		if (pde & PG_PS) {
			if ((pde & PG_RW) || (prot & VM_PROT_WRITE) == 0) {
				if (vm_page_pa_tryrelock(pmap, (pde &
				    PG_PS_FRAME) | (va & PDRMASK), &pa))
					goto retry;
				m = PHYS_TO_VM_PAGE((pde & PG_PS_FRAME) |
				    (va & PDRMASK));
				vm_page_hold(m);
			}
		} else {
			pte = *pmap_pde_to_pte(pdep, va);
			if ((pte & PG_V) &&
			    ((pte & PG_RW) || (prot & VM_PROT_WRITE) == 0)) {
				if (vm_page_pa_tryrelock(pmap, pte & PG_FRAME,
				    &pa))
					goto retry;
				m = PHYS_TO_VM_PAGE(pte & PG_FRAME);
				vm_page_hold(m);
			}
		}
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
#endif /* 0 */
}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	pd_entry_t *l2;
	pt_entry_t *l3;
	vm_paddr_t pa;

	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS) {
		pa = DMAP_TO_PHYS(va);
	} else {
		l2 = pmap_l2(kernel_pmap, va);
		if (l2 == NULL)
			panic("pmap_kextract: No l2");
		if ((*l2 & ATTR_DESCR_MASK) == L2_BLOCK)
			return ((*l2 & ~ATTR_MASK) | (va & L2_OFFSET));

		l3 = pmap_l2_to_l3(l2, va);
		if (l3 == NULL)
			panic("pmap_kextract: No l3...");
		pa = (*l3 & ~ATTR_MASK) | (va & PAGE_MASK);
	}
	return (pa);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

#if 0
/*
 * Add a wired page to the kva.
 * Note: not SMP coherent.
 */
PMAP_INLINE void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store(pte, pa | X86_PG_RW | X86_PG_V | X86_PG_G);
}
#endif /* 0 */

void
pmap_kenter_device(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *l3;

	KASSERT((pa & L3_OFFSET) == 0,
	   ("pmap_kenter_device: Invalid physical address"));
	KASSERT((va & L3_OFFSET) == 0,
	   ("pmap_kenter_device: Invalid virtual address"));
	l3 = pmap_l3(kernel_pmap, va);
	KASSERT(l3 != NULL, ("Invalid page table"));
	*l3 = (pa & ~L3_OFFSET) | ATTR_AF | L3_PAGE | ATTR_IDX(DEVICE_MEMORY);
}

#if 0
static __inline void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode)
{
	pt_entry_t *pte;
	int cache_bits;

	pte = vtopte(va);
	cache_bits = pmap_cache_bits(kernel_pmap, mode, 0);
	pte_store(pte, pa | X86_PG_RW | X86_PG_V | X86_PG_G | cache_bits);
}
#endif /* 0 */

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *l3;

	l3 = pmap_l3(kernel_pmap, va);
	KASSERT(l3 != NULL, ("pmap_kremove: Invalid address"));
	*l3 = 0;
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
	pt_entry_t *l3, pa;
	vm_offset_t va;
	vm_page_t m;
	int i;

	va = sva;
	for (i = 0; i < count; i++) {
		m = ma[i];
		pa = VM_PAGE_TO_PHYS(m) | ATTR_AF |
		    ATTR_IDX(m->md.pv_memattr) | ATTR_AP(ATTR_AP_RW) | L3_PAGE;
		l3 = pmap_l3(kernel_pmap, va);
		*l3 = pa;

		va += L3_SIZE;
	}
}

/*
 * This routine tears out page mappings from the
 * kernel -- it is meant only for temporary mappings.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;

	va = sva;
	while (count-- > 0) {
		KASSERT(va >= VM_MIN_KERNEL_ADDRESS, ("usermode va %lx", va));
		pmap_kremove(va);
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
	
#if 0
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
#endif /* 0 */

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
	if (m->pindex >= NUPDE) {
		/* PD page */
		pd_entry_t *l1;
		l1 = pmap_l1(pmap, va);
		*l1 = 0;
	} else {
		/* PTE page */
		pd_entry_t *l2;
		l2 = pmap_l2(pmap, va);
		*l2 = 0;
	}
	pmap_resident_count_dec(pmap, 1);
	if (m->pindex < NUPDE) {
		/* We just released a PT, unhold the matching PD */
		vm_page_t pdpg;

		pdpg = PHYS_TO_VM_PAGE(*pmap_l1(pmap, va) & ~ATTR_MASK);
		pmap_unwire_l3(pmap, va, pdpg, free);
	}

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
	pmap->pm_l1 = kernel_pmap->pm_l1;
}

#if 0
/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit_type(pmap_t pmap, enum pmap_type pm_type, int flags)
{
	vm_page_t pml4pg;
	vm_paddr_t pml4phys;
	int i;

	/*
	 * allocate the page directory page
	 */
	while ((pml4pg = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL)
		VM_WAIT;

	pml4phys = VM_PAGE_TO_PHYS(pml4pg);
	pmap->pm_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(pml4phys);
	pmap->pm_pcid = -1;
	pmap->pm_cr3 = ~0;	/* initialize to an invalid value */

	if ((pml4pg->flags & PG_ZERO) == 0)
		pagezero(pmap->pm_pml4);

	/*
	 * Do not install the host kernel mappings in the nested page
	 * tables. These mappings are meaningless in the guest physical
	 * address space.
	 */
	if ((pmap->pm_type = pm_type) == PT_X86) {
		pmap->pm_cr3 = pml4phys;

		/* Wire in kernel global address entries. */
		for (i = 0; i < NKPML4E; i++) {
			pmap->pm_pml4[KPML4BASE + i] = (KPDPphys + ptoa(i)) |
			    X86_PG_RW | X86_PG_V | PG_U;
		}
		for (i = 0; i < ndmpdpphys; i++) {
			pmap->pm_pml4[DMPML4I + i] = (DMPDPphys + ptoa(i)) |
			    X86_PG_RW | X86_PG_V | PG_U;
		}

		/* install self-referential address mapping entry(s) */
		pmap->pm_pml4[PML4PML4I] = VM_PAGE_TO_PHYS(pml4pg) |
		    X86_PG_V | X86_PG_RW | X86_PG_A | X86_PG_M;

		if (pmap_pcid_enabled) {
			pmap->pm_pcid = alloc_unr(&pcid_unr);
			if (pmap->pm_pcid != -1)
				pmap->pm_cr3 |= pmap->pm_pcid;
		}
	}

	pmap->pm_root.rt_root = 0;
	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_flags = flags;
	pmap->pm_eptgen = 0;
	CPU_ZERO(&pmap->pm_save);

	return (1);
}
#endif /* 0 */

int
pmap_pinit(pmap_t pmap)
{
	vm_paddr_t l1phys;
	vm_page_t l1pt;

	/*
	 * allocate the l1 page
	 */
	while ((l1pt = vm_page_alloc(NULL, 0xdeadbeef, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL)
		VM_WAIT;

	l1phys = VM_PAGE_TO_PHYS(l1pt);
	pmap->pm_l1 = (pd_entry_t *)PHYS_TO_DMAP(l1phys);

	if ((l1pt->flags & PG_ZERO) == 0)
		pagezero(pmap->pm_l1);

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
	vm_page_t m, /*pdppg, */pdpg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, VM_ALLOC_NOOBJ |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if (lockp != NULL) {
			RELEASE_PV_LIST_LOCK(lockp);
			PMAP_UNLOCK(pmap);
			rw_runlock(&pvh_global_lock);
			VM_WAIT;
			rw_rlock(&pvh_global_lock);
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

	if (ptepindex >= NUPDE) {
		pd_entry_t *l1;
		vm_pindex_t l1index;

		l1index = ptepindex - NUPDE;
		l1 = &pmap->pm_l1[l1index];
		*l1 = VM_PAGE_TO_PHYS(m) | L1_TABLE;

	} else {
		vm_pindex_t l1index;
		pd_entry_t *l1, *l2;
#if 0
		vm_pindex_t pdpindex;
		pml4_entry_t *pml4;
		pdp_entry_t *pdp;
		pd_entry_t *pd;
#endif

		l1index = ptepindex >> (L1_SHIFT - L2_SHIFT);
		l1 = &pmap->pm_l1[l1index];
		if (*l1 == 0) {
			/* recurse for allocating page dir */
			if (_pmap_alloc_l3(pmap, NUPDE + l1index,
			    lockp) == NULL) {
				--m->wire_count;
				atomic_subtract_int(&vm_cnt.v_wire_count, 1);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			pdpg = PHYS_TO_VM_PAGE(*l1 & ~ATTR_MASK);
			pdpg->wire_count++;
		}

		l2 = (pd_entry_t *)PHYS_TO_DMAP(*l1 & ~ATTR_MASK);
		l2 = &l2[ptepindex & Ln_ADDR_MASK];
		*l2 = VM_PAGE_TO_PHYS(m) | ATTR_AF | ATTR_IDX(1) | L2_TABLE;
	}

	pmap_resident_count_inc(pmap, 1);

	printf("_pmap_alloc_l3: %p %u\n", m, m->wire_count);
	return (m);
}

#if 0
static vm_page_t
pmap_allocpde(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t pdpindex, ptepindex;
	pdp_entry_t *pdpe, PG_V;
	vm_page_t pdpg;

	PG_V = pmap_valid_bit(pmap);

retry:
	pdpe = pmap_pdpe(pmap, va);
	if (pdpe != NULL && (*pdpe & PG_V) != 0) {
		/* Add a reference to the pd page. */
		pdpg = PHYS_TO_VM_PAGE(*pdpe & PG_FRAME);
		pdpg->wire_count++;
	} else {
		/* Allocate a pd page. */
		ptepindex = pmap_pde_pindex(va);
		pdpindex = ptepindex >> NPDPEPGSHIFT;
		pdpg = _pmap_allocpte(pmap, NUPDE + pdpindex, lockp);
		if (pdpg == NULL && lockp != NULL)
			goto retry;
	}
	return (pdpg);
}
#endif

static vm_page_t
pmap_alloc_l3(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t ptepindex;
	pd_entry_t *l2;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_l2_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	l2 = pmap_l2(pmap, va);

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (l2 != NULL && *l2 != 0) {
		m = PHYS_TO_VM_PAGE(*l2 & ~ATTR_MASK);
		m->wire_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		m = _pmap_alloc_l3(pmap, ptepindex, lockp);
		if (m == NULL && lockp != NULL)
			goto retry;
	}
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

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pmap->pm_l1));

	m->wire_count--;
	atomic_subtract_int(&vm_cnt.v_wire_count, 1);
	vm_page_free_zero(m);
}

#if 0
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
#endif /* 0 */

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_paddr_t paddr;
	vm_page_t nkpg;
	pd_entry_t *l1, *l2;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);

#if 0
	/*
	 * Return if "addr" is within the range of kernel page table pages
	 * that were preallocated during pmap bootstrap.  Moreover, leave
	 * "kernel_vm_end" and the kernel page table as they were.
	 *
	 * The correctness of this action is based on the following
	 * argument: vm_map_findspace() allocates contiguous ranges of the
	 * kernel virtual address space.  It calls this function if a range
	 * ends after "kernel_vm_end".  If the kernel is mapped between
	 * "kernel_vm_end" and "addr", then the range cannot begin at
	 * "kernel_vm_end".  In fact, its beginning address cannot be less
	 * than the kernel.  Thus, there is no immediate need to allocate
	 * any new kernel page table pages between "kernel_vm_end" and
	 * "KERNBASE".
	 */
	if (KERNBASE < addr && addr <= KERNBASE + nkpt * NBPDR)
		return;
#endif

	addr = roundup2(addr, L2_SIZE);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		l1 = pmap_l1(kernel_pmap, kernel_vm_end);
		if (*l1 == 0) {
			/* We need a new PDP entry */
			nkpg = vm_page_alloc(NULL, kernel_vm_end >> L1_SHIFT,
			    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ |
			    VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (nkpg == NULL)
				panic("pmap_growkernel: no memory to grow kernel");
			if ((nkpg->flags & PG_ZERO) == 0)
				pmap_zero_page(nkpg);
			paddr = VM_PAGE_TO_PHYS(nkpg);
			*l1 = paddr | L1_TABLE;
			continue; /* try again */
		}
		l2 = pmap_l1_to_l2(l1, kernel_vm_end);
		if ((*l2 & ATTR_AF) != 0) {
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
		*l2 = paddr | L2_TABLE;

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

#if 0
static const uint64_t pc_freemask[_NPCM] = { PC_FREE0, PC_FREE1, PC_FREE2 };

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
#if 0
	struct pch new_tail;
	struct pv_chunk *pc;
	struct md_page *pvh;
	pd_entry_t *pde;
	pmap_t pmap;
	pt_entry_t *pte, tpte;
	pt_entry_t PG_G, PG_A, PG_M, PG_RW;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m, m_pc;
	struct spglist free;
	uint64_t inuse;
	int bit, field, freed;
#endif

	panic("reclaim_pv_chunk");
#if 0
	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reclaim_pv_chunk: lockp is NULL"));
	pmap = NULL;
	m_pc = NULL;
	PG_G = PG_A = PG_M = PG_RW = 0;
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
			} else if (pmap != locked_pmap &&
			    !PMAP_TRYLOCK(pmap)) {
				pmap = NULL;
				TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
				mtx_lock(&pv_chunks_mutex);
				continue;
			}
			PG_G = pmap_global_bit(pmap);
			PG_A = pmap_accessed_bit(pmap);
			PG_M = pmap_modified_bit(pmap);
			PG_RW = pmap_rw_bit(pmap);
		}

		/*
		 * Destroy every non-wired, 4 KB page mapping in the chunk.
		 */
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			for (inuse = ~pc->pc_map[field] & pc_freemask[field];
			    inuse != 0; inuse &= ~(1UL << bit)) {
				bit = bsfq(inuse);
				pv = &pc->pc_pventry[field * 64 + bit];
				va = pv->pv_va;
				pde = pmap_pde(pmap, va);
				if ((*pde & PG_PS) != 0)
					continue;
				pte = pmap_pde_to_pte(pde, va);
				if ((*pte & PG_W) != 0)
					continue;
				tpte = pte_load_clear(pte);
				if ((tpte & PG_G) != 0)
					pmap_invalidate_page(pmap, va);
				m = PHYS_TO_VM_PAGE(tpte & PG_FRAME);
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
					vm_page_dirty(m);
				if ((tpte & PG_A) != 0)
					vm_page_aflag_set(m, PGA_REFERENCED);
				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
				m->md.pv_gen++;
				if (TAILQ_EMPTY(&m->md.pv_list) &&
				    (m->flags & PG_FICTITIOUS) == 0) {
					pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
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
			m_pc = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
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
#endif /* 0 */
}

/*
 * free the pv_entry back to the free list
 */
static void
free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int idx, field, bit;

	rw_assert(&pvh_global_lock, RA_LOCKED);
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
#if 0 /* TODO: For minidump */
	dump_drop_page(m->phys_addr);
#endif
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
	int bit, field;
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
#if 0 /* TODO: This is for minidump */
	dump_add_page(m->phys_addr);
#endif
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

#if 0
/*
 * Returns the number of one bits within the given PV chunk map element.
 */
static int
popcnt_pc_map_elem(uint64_t elem)
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
		if ((cpu_feature2 & CPUID2_POPCNT) == 0) {
			free = popcnt_pc_map_elem(pc->pc_map[0]);
			free += popcnt_pc_map_elem(pc->pc_map[1]);
			free += popcnt_pc_map_elem(pc->pc_map[2]);
		} else {
			free = popcntq(pc->pc_map[0]);
			free += popcntq(pc->pc_map[1]);
			free += popcntq(pc->pc_map[2]);
		}
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
#endif /* 0 */

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

#if 0
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
	    ("pmap_pv_demote_pde: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the 2mpage's pv entry for this mapping to the first
	 * page's pv list.  Once this transfer begins, the pv list lock
	 * must not be released until the last pv entry is reinstantiated.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_pde: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
	m->md.pv_gen++;
	/* Instantiate the remaining NPTEPG - 1 pv entries. */
	PV_STAT(atomic_add_long(&pv_entry_allocs, NPTEPG - 1));
	va_last = va + NBPDR - PAGE_SIZE;
	for (;;) {
		pc = TAILQ_FIRST(&pmap->pm_pvchunk);
		KASSERT(pc->pc_map[0] != 0 || pc->pc_map[1] != 0 ||
		    pc->pc_map[2] != 0, ("pmap_pv_demote_pde: missing spare"));
		for (field = 0; field < _NPCM; field++) {
			while (pc->pc_map[field]) {
				bit = bsfq(pc->pc_map[field]);
				pc->pc_map[field] &= ~(1ul << bit);
				pv = &pc->pc_pventry[field * 64 + bit];
				va += PAGE_SIZE;
				pv->pv_va = va;
				m++;
				KASSERT((m->oflags & VPO_UNMANAGED) == 0,
			    ("pmap_pv_demote_pde: page %p is not managed", m));
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
	    ("pmap_pv_promote_pde: pa is not 2mpage aligned"));
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
	KASSERT(pv != NULL, ("pmap_pv_promote_pde: pv not found"));
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
#endif /* 0 */

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

	rw_assert(&pvh_global_lock, RA_LOCKED);
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

#if 0
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
		newpte += PAGE_SIZE;
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
	pt_entry_t *firstpte, newpte;
	pt_entry_t PG_A, PG_G, PG_M, PG_RW, PG_V;
	vm_paddr_t mptepa;
	vm_page_t mpte;
	struct spglist free;
	int PG_PTE_CACHE;

	PG_G = pmap_global_bit(pmap);
	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_PTE_CACHE = pmap_cache_mask(pmap, 0);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpde = *pde;
	KASSERT((oldpde & (PG_PS | PG_V)) == (PG_PS | PG_V),
	    ("pmap_demote_pde: oldpde is missing PG_PS and/or PG_V"));
	if ((oldpde & PG_A) != 0 && (mpte = pmap_lookup_pt_page(pmap, va)) !=
	    NULL)
		pmap_remove_pt_page(pmap, mpte);
	else {
		KASSERT((oldpde & PG_W) == 0,
		    ("pmap_demote_pde: page table page for a wired mapping"
		    " is missing"));

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
		if ((oldpde & PG_A) == 0 || (mpte = vm_page_alloc(NULL,
		    pmap_pde_pindex(va), (va >= DMAP_MIN_ADDRESS && va <
		    DMAP_MAX_ADDRESS ? VM_ALLOC_INTERRUPT : VM_ALLOC_NORMAL) |
		    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
			SLIST_INIT(&free);
			pmap_remove_pde(pmap, pde, trunc_2mpage(va), &free,
			    lockp);
			pmap_invalidate_page(pmap, trunc_2mpage(va));
			pmap_free_zero_pages(&free);
			CTR2(KTR_PMAP, "pmap_demote_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (FALSE);
		}
		if (va < VM_MAXUSER_ADDRESS)
			pmap_resident_count_inc(pmap, 1);
	}
	mptepa = VM_PAGE_TO_PHYS(mpte);
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(mptepa);
	newpde = mptepa | PG_M | PG_A | (oldpde & PG_U) | PG_RW | PG_V;
	KASSERT((oldpde & PG_A) != 0,
	    ("pmap_demote_pde: oldpde is missing PG_A"));
	KASSERT((oldpde & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pde: oldpde is missing PG_M"));
	newpte = oldpde & ~PG_PS;
	newpte = pmap_swap_pat(pmap, newpte);

	/*
	 * If the page table page is new, initialize it.
	 */
	if (mpte->wire_count == 1) {
		mpte->wire_count = NPTEPG;
		pmap_fill_ptp(firstpte, newpte);
	}
	KASSERT((*firstpte & PG_FRAME) == (newpte & PG_FRAME),
	    ("pmap_demote_pde: firstpte and newpte map different physical"
	    " addresses"));

	/*
	 * If the mapping has changed attributes, update the page table
	 * entries.
	 */
	if ((*firstpte & PG_PTE_PROMOTE) != (newpte & PG_PTE_PROMOTE))
		pmap_fill_ptp(firstpte, newpte);

	/*
	 * The spare PV entries must be reserved prior to demoting the
	 * mapping, that is, prior to changing the PDE.  Otherwise, the state
	 * of the PDE and the PV lists will be inconsistent, which can result
	 * in reclaim_pv_chunk() attempting to remove a PV entry from the
	 * wrong PV list and pmap_pv_demote_pde() failing to find the expected
	 * PV entry for the 2MB page mapping that is being demoted.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		reserve_pv_entries(pmap, NPTEPG - 1, lockp);

	/*
	 * Demote the mapping.  This pmap is locked.  The old PDE has
	 * PG_A set.  If the old PDE has PG_RW set, it also has PG_M
	 * set.  Thus, there is no danger of a race with another
	 * processor changing the setting of PG_A and/or PG_M between
	 * the read above and the store below. 
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else
		pde_store(pde, newpde);

	/*
	 * Invalidate a stale recursive mapping of the page table page.
	 */
	if (va >= VM_MAXUSER_ADDRESS)
		pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));

	/*
	 * Demote the PV entry.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		pmap_pv_demote_pde(pmap, va, oldpde & PG_PS_FRAME, lockp);

	atomic_add_long(&pmap_pde_demotions, 1);
	CTR2(KTR_PMAP, "pmap_demote_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

/*
 * pmap_remove_kernel_pde: Remove a kernel superpage mapping.
 */
static void
pmap_remove_kernel_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	pd_entry_t newpde;
	vm_paddr_t mptepa;
	vm_page_t mpte;

	KASSERT(pmap == kernel_pmap, ("pmap %p is not kernel_pmap", pmap));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mpte = pmap_lookup_pt_page(pmap, va);
	if (mpte == NULL)
		panic("pmap_remove_kernel_pde: Missing pt page.");

	pmap_remove_pt_page(pmap, mpte);
	mptepa = VM_PAGE_TO_PHYS(mpte);
	newpde = mptepa | X86_PG_M | X86_PG_A | X86_PG_RW | X86_PG_V;

	/*
	 * Initialize the page table page.
	 */
	pagezero((void *)PHYS_TO_DMAP(mptepa));

	/*
	 * Demote the mapping.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else
		pde_store(pde, newpde);

	/*
	 * Invalidate a stale recursive mapping of the page table page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));
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
	pt_entry_t PG_G, PG_A, PG_M, PG_RW;

	PG_G = pmap_global_bit(pmap);
	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_remove_pde: sva is not 2mpage aligned"));
	oldpde = pte_load_clear(pdq);
	if (oldpde & PG_W)
		pmap->pm_stats.wired_count -= NBPDR / PAGE_SIZE;

	/*
	 * Machines that don't support invlpg, also don't support
	 * PG_G.
	 */
	if (oldpde & PG_G)
		pmap_invalidate_page(kernel_pmap, sva);
	pmap_resident_count_dec(pmap, NBPDR / PAGE_SIZE);
	if (oldpde & PG_MANAGED) {
		CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, oldpde & PG_PS_FRAME);
		pvh = pa_to_pvh(oldpde & PG_PS_FRAME);
		pmap_pvh_free(pvh, pmap, sva);
		eva = sva + NBPDR;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++) {
			if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(m);
			if (oldpde & PG_A)
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
	return (pmap_unuse_pt(pmap, sva, *pmap_pdpe(pmap, sva), free));
}
#endif /* 0 */

/*
 * pmap_remove_l3: do the things to unmap a page in a process
 */
static int
pmap_remove_l3(pmap_t pmap, pt_entry_t *l3, vm_offset_t va, 
    pd_entry_t l2e, struct spglist *free, struct rwlock **lockp)
{
	//struct md_page *pvh;
	pt_entry_t old_l3;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* TODO: Does this need to be atomic like amd64? */
	old_l3 = *l3;
	*l3 = 0;
	if (old_l3 & ATTR_SW_W)
		pmap->pm_stats.wired_count -= 1;
	pmap_resident_count_dec(pmap, 1);
	if (old_l3 & ATTR_SW_MANAGED) {
		m = PHYS_TO_VM_PAGE(old_l3 & ~ATTR_MASK);
#if 0
		if ((oldpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if (oldpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);
#endif
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		pmap_pvh_free(&m->md, pmap, va);
#if 0
		if (TAILQ_EMPTY(&m->md.pv_list) &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
			if (TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
#endif
	}
	return (pmap_unuse_l3(pmap, va, l2e, free));
}

#if 0
/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    struct spglist *free)
{
	struct rwlock *lock;
	pt_entry_t *pte, PG_V;

	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((*pde & PG_V) == 0)
		return;
	pte = pmap_pde_to_pte(pde, va);
	if ((*pte & PG_V) == 0)
		return;
	lock = NULL;
	pmap_remove_pte(pmap, pte, va, *pde, free, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_page(pmap, va);
}
#endif /* 0 */

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
	//pml4_entry_t *pml4e;
	//pdp_entry_t *pdpe;
	pd_entry_t *l1, *l2;
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

	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);

#if 0
	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pde = pmap_pde(pmap, sva);
		if (pde && (*pde & PG_PS) == 0) {
			pmap_remove_page(pmap, sva, pde, &free);
			goto out;
		}
	}
#endif

	lock = NULL;
	for (; sva < eva; sva = va_next) {

		if (pmap->pm_stats.resident_count == 0)
			break;

		l1 = pmap_l1(pmap, sva);
		if (*l1 == 0) {
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

		l3_paddr = *l2;

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
			if (*l3 == 0) {
				if (va != va_next) {
					pmap_invalidate_range(pmap, va, sva);
					va = va_next;
				}
				continue;
			}
#if 0
			if ((*l3 & PG_G) == 0)
				anyvalid = 1;
			else
#endif
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
#if 0
out:
#endif
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
	//struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *l3, tl3;
	pd_entry_t *l2;
	//vm_offset_t va;
	struct spglist free;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	rw_wlock(&pvh_global_lock);
	//if ((m->flags & PG_FICTITIOUS) != 0)
	//	goto small_mappings;
#if 0
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		(void)pmap_demote_pde(pmap, pde, va);
		PMAP_UNLOCK(pmap);
	}
#endif
//small_mappings:
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pmap_resident_count_dec(pmap, 1);
		l2 = pmap_l2(pmap, pv->pv_va);
		KASSERT((*l2 & ATTR_DESCR_MASK) == L2_TABLE,
		    ("pmap_remove_all: found a table when expecting "
		     "a block in %p's pv list", m));
		l3 = pmap_l2_to_l3(l2, pv->pv_va);
		tl3 = *l3;
		*l3 = 0;
		if (tl3 & ATTR_SW_W)
			pmap->pm_stats.wired_count--;
#if 0
		if (tpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
#endif
		pmap_unuse_l3(pmap, pv->pv_va, *l2, &free);
#if 0
		pmap_invalidate_page(pmap, pv->pv_va);
#endif
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(&pvh_global_lock);
	pmap_free_zero_pages(&free);
}

#if 0
/*
 * pmap_protect_pde: do the things to protect a 2mpage in a process
 */
static boolean_t
pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva, vm_prot_t prot)
{
	pd_entry_t newpde, oldpde;
	vm_offset_t eva, va;
	vm_page_t m;
	boolean_t anychanged;
	pt_entry_t PG_G, PG_M, PG_RW;

	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_protect_pde: sva is not 2mpage aligned"));
	anychanged = FALSE;
retry:
	oldpde = newpde = *pde;
	if (oldpde & PG_MANAGED) {
		eva = sva + NBPDR;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++)
			if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(m);
	}
	if ((prot & VM_PROT_WRITE) == 0)
		newpde &= ~(PG_RW | PG_M);
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
	if (newpde != oldpde) {
		if (!atomic_cmpset_long(pde, oldpde, newpde))
			goto retry;
		if (oldpde & PG_G)
			pmap_invalidate_page(pmap, sva);
		else
			anychanged = TRUE;
	}
	return (anychanged);
}
#endif /* 0 */

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
/* TODOandrew: Check if this is correct */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va, va_next;
	pd_entry_t *l1, *l2;
	pt_entry_t *l3;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if ((prot & VM_PROT_WRITE) == VM_PROT_WRITE)
		return;

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {

		l1 = pmap_l1(pmap, sva);
		if (*l1 == 0) {
			va_next = (sva + L1_SIZE) & ~L1_OFFSET;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + L2_SIZE) & ~L2_OFFSET;
		if (va_next < sva)
			va_next = eva;

		l2 = pmap_l1_to_l2(l1, sva);
		if (l2 == NULL || (*l2 & ATTR_DESCR_MASK) != L2_TABLE)
			continue;

		if (va_next > eva)
			va_next = eva;

		va = va_next;
		for (l3 = pmap_l2_to_l3(l2, sva); sva != va_next; l3++,
		    sva += L3_SIZE) {
			*l3 |= ATTR_AP(ATTR_AP_RO);
		}
	}
	PMAP_UNLOCK(pmap);
}

#if 0
/*
 * Tries to promote the 512, contiguous 4KB page mappings that are within a
 * single page table page (PTP) to a single 2MB page mapping.  For promotion
 * to occur, two conditions must be met: (1) the 4KB page mappings must map
 * aligned, contiguous physical memory and (2) the 4KB page mappings must have
 * identical characteristics. 
 */
static void
pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp)
{
	pd_entry_t newpde;
	pt_entry_t *firstpte, oldpte, pa, *pte;
	pt_entry_t PG_G, PG_A, PG_M, PG_RW, PG_V;
	vm_offset_t oldpteva;
	vm_page_t mpte;
	int PG_PTE_CACHE;

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	PG_PTE_CACHE = pmap_cache_mask(pmap, 0);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Examine the first PTE in the specified PTP.  Abort if this PTE is
	 * either invalid, unused, or does not map the first 4KB physical page
	 * within a 2MB page. 
	 */
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(*pde & PG_FRAME);
setpde:
	newpde = *firstpte;
	if ((newpde & ((PG_FRAME & PDRMASK) | PG_A | PG_V)) != (PG_A | PG_V)) {
		atomic_add_long(&pmap_pde_p_failures, 1);
		CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return;
	}
	if ((newpde & (PG_M | PG_RW)) == PG_RW) {
		/*
		 * When PG_M is already clear, PG_RW can be cleared without
		 * a TLB invalidation.
		 */
		if (!atomic_cmpset_long(firstpte, newpde, newpde & ~PG_RW))
			goto setpde;
		newpde &= ~PG_RW;
	}

	/*
	 * Examine each of the other PTEs in the specified PTP.  Abort if this
	 * PTE maps an unexpected 4KB physical page or does not have identical
	 * characteristics to the first PTE.
	 */
	pa = (newpde & (PG_PS_FRAME | PG_A | PG_V)) + NBPDR - PAGE_SIZE;
	for (pte = firstpte + NPTEPG - 1; pte > firstpte; pte--) {
setpte:
		oldpte = *pte;
		if ((oldpte & (PG_FRAME | PG_A | PG_V)) != pa) {
			atomic_add_long(&pmap_pde_p_failures, 1);
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return;
		}
		if ((oldpte & (PG_M | PG_RW)) == PG_RW) {
			/*
			 * When PG_M is already clear, PG_RW can be cleared
			 * without a TLB invalidation.
			 */
			if (!atomic_cmpset_long(pte, oldpte, oldpte & ~PG_RW))
				goto setpte;
			oldpte &= ~PG_RW;
			oldpteva = (oldpte & PG_FRAME & PDRMASK) |
			    (va & ~PDRMASK);
			CTR2(KTR_PMAP, "pmap_promote_pde: protect for va %#lx"
			    " in pmap %p", oldpteva, pmap);
		}
		if ((oldpte & PG_PTE_PROMOTE) != (newpde & PG_PTE_PROMOTE)) {
			atomic_add_long(&pmap_pde_p_failures, 1);
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return;
		}
		pa -= PAGE_SIZE;
	}

	/*
	 * Save the page table page in its current state until the PDE
	 * mapping the superpage is demoted by pmap_demote_pde() or
	 * destroyed by pmap_remove_pde(). 
	 */
	mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("pmap_promote_pde: page table page is out of range"));
	KASSERT(mpte->pindex == pmap_pde_pindex(va),
	    ("pmap_promote_pde: page table page's pindex is wrong"));
	if (pmap_insert_pt_page(pmap, mpte)) {
		atomic_add_long(&pmap_pde_p_failures, 1);
		CTR2(KTR_PMAP,
		    "pmap_promote_pde: failure for va %#lx in pmap %p", va,
		    pmap);
		return;
	}

	/*
	 * Promote the pv entries.
	 */
	if ((newpde & PG_MANAGED) != 0)
		pmap_pv_promote_pde(pmap, va, newpde & PG_PS_FRAME, lockp);

	/*
	 * Propagate the PAT index to its proper position.
	 */
	newpde = pmap_swap_pat(pmap, newpde);

	/*
	 * Map the superpage.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, PG_PS | newpde);
	else
		pde_store(pde, PG_PS | newpde);

	atomic_add_long(&pmap_pde_promotions, 1);
	CTR2(KTR_PMAP, "pmap_promote_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
}
#endif /* 0 */

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
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	struct rwlock *lock;
	pd_entry_t *l1, *l2;
	pt_entry_t new_l3, orig_l3;
	pt_entry_t *l3;
	pv_entry_t pv;
	vm_paddr_t opa, pa, l2_pa, l3_pa;
	vm_page_t mpte, om, l2_m, l3_m;

	va = trunc_page(va);
#if 0
	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%lx)",
	    va));
	KASSERT((m->oflags & VPO_UNMANAGED) != 0 || va < kmi.clean_sva ||
	    va >= kmi.clean_eva,
	    ("pmap_enter: managed mapping within the clean submap"));
#endif
	if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
		VM_OBJECT_ASSERT_WLOCKED(m->object);
	pa = VM_PAGE_TO_PHYS(m);
	new_l3 = (pt_entry_t)(pa | ATTR_AF | ATTR_IDX(1) | L3_PAGE);
#if 0
	if ((access & VM_PROT_WRITE) != 0)
		newpte |= PG_M;
#endif
	if ((prot & VM_PROT_WRITE) == 0)
		new_l3 |= ATTR_AP(ATTR_AP_RO);
	if (wired)
		new_l3 |= ATTR_SW_W;
	if ((va >> 63) == 0)
		new_l3 |= ATTR_AP(ATTR_AP_USER);
	new_l3 |= ATTR_IDX(m->md.pv_memattr);

	mpte = NULL;

	lock = NULL;
	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);

	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_alloc_l3(pmap, va, &lock);
		l3 = pmap_l3(pmap, va);
	} else {
		l3 = pmap_l3(pmap, va);
		/* TODO: This is not optimal, but should mostly work */
		if (l3 == NULL) {
			l2 = pmap_l2(pmap, va);

			if (l2 == NULL) {
				l2_m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
				    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
				    VM_ALLOC_ZERO);
				if (l2_m == NULL)
					panic("pmap_enter: l2 pte_m == NULL");
				if ((l2_m->flags & PG_ZERO) == 0)
					pmap_zero_page(l2_m);

				l2_pa = VM_PAGE_TO_PHYS(l2_m);
				l1 = pmap_l1(pmap, va);
				*l1 = l2_pa | ATTR_AF | L1_TABLE;
				l2 = pmap_l1_to_l2(l1, va);
			}

			KASSERT(l2 != NULL,
			    ("No l2 table after allocating one"));

			l3_m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
			    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (l3_m == NULL)
				panic("pmap_enter: l3 pte_m == NULL");
			if ((l3_m->flags & PG_ZERO) == 0)
				pmap_zero_page(l3_m);

			l3_pa = VM_PAGE_TO_PHYS(l3_m);
			*l2 = l3_pa | ATTR_AF | L2_TABLE;
			l3 = pmap_l2_to_l3(l2, va);
		}
	}

	om = NULL;
	orig_l3 = *l3;
	opa = orig_l3 & ~ATTR_MASK;

	/*
	 * Is the specified virtual address already mapped?
	 */
	if ((orig_l3 & ATTR_AF) != 0) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && (orig_l3 & ATTR_SW_W) == 0)
			pmap->pm_stats.wired_count++;
		else if (!wired && (orig_l3 & ATTR_SW_W) != 0)
			pmap->pm_stats.wired_count--;

		/*
		 * Remove the extra PT page reference.
		 */
		if (mpte != NULL) {
			mpte->wire_count--;
			KASSERT(mpte->wire_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			     " va: 0x%llx", va));
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
	} else {
		/*
		 * Increment the counters.
		 */
		if ((new_l3 & ATTR_SW_W) != 0)
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
		if ((new_l3 & (1 << 7)) == ATTR_AP(ATTR_AP_RW))
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}

	/*
	 * Update the PTE.
	 */
	if ((orig_l3 & ATTR_AF) != 0) {
validate:
		orig_l3 = *l3;
		*l3 = new_l3;
		opa = orig_l3 & ~ATTR_MASK;

		if (opa != pa) {
			if ((orig_l3 & ATTR_SW_MANAGED) != 0) {
				om = PHYS_TO_VM_PAGE(opa);
#if 0
				if ((origpte & (PG_M | PG_RW)) == (PG_M |
				    PG_RW))
					vm_page_dirty(om);
				if ((origpte & PG_A) != 0)
					vm_page_aflag_set(om, PGA_REFERENCED);
#endif
				CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, opa);
				pmap_pvh_free(&om->md, pmap, va);
#if 0
				if ((om->aflags & PGA_WRITEABLE) != 0 &&
				    TAILQ_EMPTY(&om->md.pv_list) &&
				    ((om->flags & PG_FICTITIOUS) != 0 ||
				    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
					vm_page_aflag_clear(om, PGA_WRITEABLE);
#endif
			}
#if 0
		} else if ((newpte & PG_M) == 0 && (origpte & (PG_M |
		    PG_RW)) == (PG_M | PG_RW)) {
			if ((origpte & PG_MANAGED) != 0)
				vm_page_dirty(m);

			/*
			 * Although the PTE may still have PG_RW set, TLB
			 * invalidation may nonetheless be required because
			 * the PTE no longer has PG_M set.
			 */
		} else if ((origpte & PG_NX) != 0 || (newpte & PG_NX) == 0) {
			/*
			 * This PTE change does not require TLB invalidation.
			 */
			goto unchanged;
#endif
		}
#if 0
		if ((origpte & PG_A) != 0)
#endif
			pmap_invalidate_page(pmap, va);
	} else
		*l3 = new_l3;

//unchanged:

#if 0
	/*
	 * If both the page table page and the reservation are fully
	 * populated, then attempt promotion.
	 */
	if ((mpte == NULL || mpte->wire_count == NPTEPG) &&
	    pmap_ps_enabled(pmap) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0)
		pmap_promote_pde(pmap, pde, va, &lock);
#endif

	if (lock != NULL)
		rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

#if 0
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
	pd_entry_t *pde, newpde;
	pt_entry_t PG_V;
	vm_page_t mpde;
	struct spglist free;

	PG_V = pmap_valid_bit(pmap);
	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	if ((mpde = pmap_allocpde(pmap, va, NULL)) == NULL) {
		CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	pde = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mpde));
	pde = &pde[pmap_pde_index(va)];
	if ((*pde & PG_V) != 0) {
		KASSERT(mpde->wire_count > 1,
		    ("pmap_enter_pde: mpde's wire count is too low"));
		mpde->wire_count--;
		CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	newpde = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(pmap, m->md.pat_mode, 1) |
	    PG_PS | PG_V;
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		newpde |= PG_MANAGED;

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
			CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (FALSE);
		}
	}
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
	if (va < VM_MAXUSER_ADDRESS)
		newpde |= PG_U;

	/*
	 * Increment counters.
	 */
	pmap_resident_count_inc(pmap, NBPDR / PAGE_SIZE);

	/*
	 * Map the superpage.
	 */
	pde_store(pde, newpde);

	atomic_add_long(&pmap_pde_mappings, 1);
	CTR2(KTR_PMAP, "pmap_enter_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}
#endif /* 0 */

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
#if 0
		if ((va & PDRMASK) == 0 && va + NBPDR <= end &&
		    m->psind == 1 && pmap_ps_enabled(pmap) &&
		    pmap_enter_pde(pmap, va, m, prot, &lock))
			m = &m[NBPDR / PAGE_SIZE - 1];
		else
#endif /* 0 */
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
	struct spglist free;
	pd_entry_t *l2;
	pt_entry_t *l3;
	vm_paddr_t pa;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	rw_assert(&pvh_global_lock, RA_LOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

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
			l2 = pmap_l2(pmap, va);

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.  Otherwise, we
			 * attempt to allocate a page table page.  If this
			 * attempt fails, we don't retry.  Instead, we give up.
			 */
			if (l2 != NULL && *l2 != 0) {
				mpte = PHYS_TO_VM_PAGE(*l2 & ~ATTR_MASK);
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
		l3 = pmap_l3(kernel_pmap, va);
	}
	if (l3 == NULL)
		panic("pmap_enter_quick_locked: No l3");
	if (*l3) {
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
#if 0
				pmap_invalidate_page(pmap, va);
#endif
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

	pa = VM_PAGE_TO_PHYS(m) | ATTR_AF | ATTR_IDX(m->md.pv_memattr) |
	    ATTR_AP(ATTR_AP_RW) | L3_PAGE;
#if 0
	if ((prot & VM_PROT_EXECUTE) == 0)
		pa |= pg_nx;
#endif

	/*
	 * Now validate mapping with RO protection
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0)
		pa |= ATTR_SW_MANAGED;
	*l3 = pa;
	return (mpte);
}

#if 0
/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	vm_offset_t va;

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	pmap_kenter(va, pa);
	invlpg(va);
	return ((void *)crashdumpmap);
}
#endif /* 0 */

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{
	panic("pmap_object_init_pt");
#if 0
	pd_entry_t *pde;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t pa, ptepa;
	vm_page_t p, pdpg;
	int pat_mode;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));
	if ((addr & (NBPDR - 1)) == 0 && (size & (NBPDR - 1)) == 0) {
		if (!pmap_ps_enabled(pmap))
			return;
		if (!vm_object_populate(object, pindex, pindex + atop(size)))
			return;
		p = vm_page_lookup(object, pindex);
		KASSERT(p->valid == VM_PAGE_BITS_ALL,
		    ("pmap_object_init_pt: invalid page %p", p));
		pat_mode = p->md.pat_mode;

		/*
		 * Abort the mapping if the first page is not physically
		 * aligned to a 2MB page boundary.
		 */
		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1))
			return;

		/*
		 * Skip the first page.  Abort the mapping if the rest of
		 * the pages are not physically contiguous or have differing
		 * memory attributes.
		 */
		p = TAILQ_NEXT(p, listq);
		for (pa = ptepa + PAGE_SIZE; pa < ptepa + size;
		    pa += PAGE_SIZE) {
			KASSERT(p->valid == VM_PAGE_BITS_ALL,
			    ("pmap_object_init_pt: invalid page %p", p));
			if (pa != VM_PAGE_TO_PHYS(p) ||
			    pat_mode != p->md.pat_mode)
				return;
			p = TAILQ_NEXT(p, listq);
		}

		/*
		 * Map using 2MB pages.  Since "ptepa" is 2M aligned and
		 * "size" is a multiple of 2M, adding the PAT setting to "pa"
		 * will not affect the termination of this loop.
		 */ 
		PMAP_LOCK(pmap);
		for (pa = ptepa | pmap_cache_bits(pmap, pat_mode, 1);
		    pa < ptepa + size; pa += NBPDR) {
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
			pde = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pdpg));
			pde = &pde[pmap_pde_index(addr)];
			if ((*pde & PG_V) == 0) {
				pde_store(pde, pa | PG_PS | PG_M | PG_A |
				    PG_U | PG_RW | PG_V);
				pmap_resident_count_inc(pmap, NBPDR / PAGE_SIZE);
				atomic_add_long(&pmap_pde_mappings, 1);
			} else {
				/* Continue on if the PDE is already valid. */
				pdpg->wire_count--;
				KASSERT(pdpg->wire_count > 0,
				    ("pmap_object_init_pt: missing reference "
				    "to page directory page, va: 0x%lx", addr));
			}
			addr += NBPDR;
		}
		PMAP_UNLOCK(pmap);
	}
#endif /* 0 */
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
	panic("pmap_unwire");
#if 0
	vm_offset_t va_next;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	boolean_t pv_lists_locked;

	PG_V = pmap_valid_bit(pmap);
	pv_lists_locked = FALSE;
resume:
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pml4e = pmap_pml4e(pmap, sva);
		if ((*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0) {
			va_next = (sva + NBPDP) & ~PDPMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;
		pde = pmap_pdpe_to_pde(pdpe, sva);
		if ((*pde & PG_V) == 0)
			continue;
		if ((*pde & PG_PS) != 0) {
			if ((*pde & PG_W) == 0)
				panic("pmap_unwire: pde %#jx is missing PG_W",
				    (uintmax_t)*pde);

			/*
			 * Are we unwiring the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				atomic_clear_long(pde, PG_W);
				pmap->pm_stats.wired_count -= NBPDR /
				    PAGE_SIZE;
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
			if ((*pte & PG_V) == 0)
				continue;
			if ((*pte & PG_W) == 0)
				panic("pmap_unwire: pte %#jx is missing PG_W",
				    (uintmax_t)*pte);

			/*
			 * PG_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PG_W, another processor
			 * could be setting PG_M and/or PG_A concurrently.
			 */
			atomic_clear_long(pte, PG_W);
			pmap->pm_stats.wired_count--;
		}
	}
	if (pv_lists_locked)
		rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
#endif /* 0 */
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
#if 0
	struct rwlock *lock;
	struct spglist free;
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t va_next;
	pt_entry_t PG_A, PG_M, PG_V;

	if (dst_addr != src_addr)
		return;

	if (dst_pmap->pm_type != src_pmap->pm_type)
		return;

	/*
	 * EPT page table entries that require emulation of A/D bits are
	 * sensitive to clearing the PG_A bit (aka EPT_PG_READ). Although
	 * we clear PG_M (aka EPT_PG_WRITE) concomitantly, the PG_U bit
	 * (aka EPT_PG_EXECUTE) could still be set. Since some EPT
	 * implementations flag an EPT misconfiguration for exec-only
	 * mappings we skip this function entirely for emulated pmaps.
	 */
	if (pmap_emulate_ad_bits(dst_pmap))
		return;

	lock = NULL;
	rw_rlock(&pvh_global_lock);
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}

	PG_A = pmap_accessed_bit(dst_pmap);
	PG_M = pmap_modified_bit(dst_pmap);
	PG_V = pmap_valid_bit(dst_pmap);

	for (addr = src_addr; addr < end_addr; addr = va_next) {
		pt_entry_t *src_pte, *dst_pte;
		vm_page_t dstmpde, dstmpte, srcmpte;
		pml4_entry_t *pml4e;
		pdp_entry_t *pdpe;
		pd_entry_t srcptepaddr, *pde;

		KASSERT(addr < UPT_MIN_ADDRESS,
		    ("pmap_copy: invalid to pmap_copy page tables"));

		pml4e = pmap_pml4e(src_pmap, addr);
		if ((*pml4e & PG_V) == 0) {
			va_next = (addr + NBPML4) & ~PML4MASK;
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		pdpe = pmap_pml4e_to_pdpe(pml4e, addr);
		if ((*pdpe & PG_V) == 0) {
			va_next = (addr + NBPDP) & ~PDPMASK;
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next < addr)
			va_next = end_addr;

		pde = pmap_pdpe_to_pde(pdpe, addr);
		srcptepaddr = *pde;
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			if ((addr & PDRMASK) != 0 || addr + NBPDR > end_addr)
				continue;
			dstmpde = pmap_allocpde(dst_pmap, addr, NULL);
			if (dstmpde == NULL)
				break;
			pde = (pd_entry_t *)
			    PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dstmpde));
			pde = &pde[pmap_pde_index(addr)];
			if (*pde == 0 && ((srcptepaddr & PG_MANAGED) == 0 ||
			    pmap_pv_insert_pde(dst_pmap, addr, srcptepaddr &
			    PG_PS_FRAME, &lock))) {
				*pde = srcptepaddr & ~PG_W;
				pmap_resident_count_inc(dst_pmap, NBPDR / PAGE_SIZE);
			} else
				dstmpde->wire_count--;
			continue;
		}

		srcptepaddr &= PG_FRAME;
		srcmpte = PHYS_TO_VM_PAGE(srcptepaddr);
		KASSERT(srcmpte->wire_count > 0,
		    ("pmap_copy: source page table page is unused"));

		if (va_next > end_addr)
			va_next = end_addr;

		src_pte = (pt_entry_t *)PHYS_TO_DMAP(srcptepaddr);
		src_pte = &src_pte[pmap_pte_index(addr)];
		dstmpte = NULL;
		while (addr < va_next) {
			pt_entry_t ptetemp;
			ptetemp = *src_pte;
			/*
			 * we only virtual copy managed pages
			 */
			if ((ptetemp & PG_MANAGED) != 0) {
				if (dstmpte != NULL &&
				    dstmpte->pindex == pmap_pde_pindex(addr))
					dstmpte->wire_count++;
				else if ((dstmpte = pmap_allocpte(dst_pmap,
				    addr, NULL)) == NULL)
					goto out;
				dst_pte = (pt_entry_t *)
				    PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dstmpte));
				dst_pte = &dst_pte[pmap_pte_index(addr)];
				if (*dst_pte == 0 &&
				    pmap_try_insert_pv_entry(dst_pmap, addr,
				    PHYS_TO_VM_PAGE(ptetemp & PG_FRAME),
				    &lock)) {
					/*
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					*dst_pte = ptetemp & ~(PG_W | PG_M |
					    PG_A);
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
#endif
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
	panic("pmap_copy_pages");
#if 0
	void *a_cp, *b_cp;
	vm_page_t m_a, m_b;
	vm_paddr_t p_a, p_b;
	pt_entry_t *pte;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;
	boolean_t pinned;

	/*
	 * NB:  The sequence of updating a page table followed by accesses
	 * to the corresponding pages used in the !DMAP case is subject to
	 * the situation described in the "AMD64 Architecture Programmer's
	 * Manual Volume 2: System Programming" rev. 3.23, "7.3.1 Special
	 * Coherency Considerations".  Therefore, issuing the INVLPG right
	 * after modifying the PTE bits is crucial.
	 */
	pinned = FALSE;
	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		m_a = ma[a_offset >> PAGE_SHIFT];
		p_a = m_a->phys_addr;
		b_pg_offset = b_offset & PAGE_MASK;
		m_b = mb[b_offset >> PAGE_SHIFT];
		p_b = m_b->phys_addr;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		if (__predict_false(p_a < DMAP_MIN_ADDRESS ||
		    p_a > DMAP_MIN_ADDRESS + dmaplimit)) {
			mtx_lock(&cpage_lock);
			sched_pin();
			pinned = TRUE;
			pte = vtopte(cpage_a);
			*pte = p_a | X86_PG_A | X86_PG_V |
			    pmap_cache_bits(kernel_pmap, m_a->md.pat_mode, 0);
			invlpg(cpage_a);
			a_cp = (char *)cpage_a + a_pg_offset;
		} else {
			a_cp = (char *)PHYS_TO_DMAP(p_a) + a_pg_offset;
		}
		if (__predict_false(p_b < DMAP_MIN_ADDRESS ||
		    p_b > DMAP_MIN_ADDRESS + dmaplimit)) {
			if (!pinned) {
				mtx_lock(&cpage_lock);
				sched_pin();
				pinned = TRUE;
			}
			pte = vtopte(cpage_b);
			*pte = p_b | X86_PG_A | X86_PG_M | X86_PG_RW |
			    X86_PG_V | pmap_cache_bits(kernel_pmap,
			    m_b->md.pat_mode, 0);
			invlpg(cpage_b);
			b_cp = (char *)cpage_b + b_pg_offset;
		} else {
			b_cp = (char *)PHYS_TO_DMAP(p_b) + b_pg_offset;
		}
		bcopy(a_cp, b_cp, cnt);
		if (__predict_false(pinned)) {
			sched_unpin();
			mtx_unlock(&cpage_lock);
			pinned = FALSE;
		}
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
#endif
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
	panic("pmap_page_exists_quick");
#if 0
	struct md_page *pvh;
	struct rwlock *lock;
	pv_entry_t pv;
	int loops = 0;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
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
#endif /* 0 */
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
	//struct md_page *pvh;
	pmap_t pmap;
	pt_entry_t *l3;
	pv_entry_t pv;
	int count, md_gen; //, pvh_gen;

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
		l3 = pmap_l3(pmap, pv->pv_va);
		if (l3 != NULL && (*l3 & ATTR_SW_W) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
#if 0
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
			pte = pmap_pde(pmap, pv->pv_va);
			if ((*pte & PG_W) != 0)
				count++;
			PMAP_UNLOCK(pmap);
		}
	}
#endif
	rw_runlock(lock);
	rw_runlock(&pvh_global_lock);
	return (count);
}

#if 0
/*
 * Returns TRUE if the given page is mapped individually or as part of
 * a 2mpage.  Otherwise, returns FALSE.
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
#endif /* 0 */

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
	panic("pmap_remove_pages");
#if 0
	pd_entry_t ptepde;
	pt_entry_t *pte, tpte;
	pt_entry_t PG_M, PG_RW, PG_V;
	struct spglist free;
	vm_page_t m, mpte, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct rwlock *lock;
	int64_t bit;
	uint64_t inuse, bitmask;
	int allfree, field, freed, idx;
	boolean_t superpage;
	vm_paddr_t pa;

	/*
	 * Assert that the given pmap is only active on the current
	 * CPU.  Unfortunately, we cannot block another CPU from
	 * activating the pmap while this function is executing.
	 */
	KASSERT(pmap == PCPU_GET(curpmap), ("non-current pmap %p", pmap));
#ifdef INVARIANTS
	{
		cpuset_t other_cpus;

		other_cpus = all_cpus;
		critical_enter();
		CPU_CLR(PCPU_GET(cpuid), &other_cpus);
		CPU_AND(&other_cpus, &pmap->pm_active);
		critical_exit();
		KASSERT(CPU_EMPTY(&other_cpus), ("pmap active %p", pmap));
	}
#endif

	lock = NULL;
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	SLIST_INIT(&free);
	rw_rlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = bsfq(inuse);
				bitmask = 1UL << bit;
				idx = field * 64 + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pte = pmap_pdpe(pmap, pv->pv_va);
				ptepde = *pte;
				pte = pmap_pdpe_to_pde(pte, pv->pv_va);
				tpte = *pte;
				if ((tpte & (PG_PS | PG_V)) == PG_V) {
					superpage = FALSE;
					ptepde = tpte;
					pte = (pt_entry_t *)PHYS_TO_DMAP(tpte &
					    PG_FRAME);
					pte = &pte[pmap_pte_index(pv->pv_va)];
					tpte = *pte;
				} else {
					/*
					 * Keep track whether 'tpte' is a
					 * superpage explicitly instead of
					 * relying on PG_PS being set.
					 *
					 * This is because PG_PS is numerically
					 * identical to PG_PTE_PAT and thus a
					 * regular page could be mistaken for
					 * a superpage.
					 */
					superpage = TRUE;
				}

				if ((tpte & PG_V) == 0) {
					panic("bad pte va %lx pte %lx",
					    pv->pv_va, tpte);
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (tpte & PG_W) {
					allfree = 0;
					continue;
				}

				if (superpage)
					pa = tpte & PG_PS_FRAME;
				else
					pa = tpte & PG_FRAME;

				m = PHYS_TO_VM_PAGE(pa);
				KASSERT(m->phys_addr == pa,
				    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
				    m, (uintmax_t)m->phys_addr,
				    (uintmax_t)tpte));

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("pmap_remove_pages: bad tpte %#jx",
				    (uintmax_t)tpte));

				pte_clear(pte);

				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
					if (superpage) {
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
					pvh = pa_to_pvh(tpte & PG_PS_FRAME);
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
					pvh->pv_gen++;
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							if ((mt->aflags & PGA_WRITEABLE) != 0 &&
							    TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					mpte = pmap_lookup_pt_page(pmap, pv->pv_va);
					if (mpte != NULL) {
						pmap_remove_pt_page(pmap, mpte);
						pmap_resident_count_dec(pmap, 1);
						KASSERT(mpte->wire_count == NPTEPG,
						    ("pmap_remove_pages: pte page wire count error"));
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
	rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(&free);
#endif
}

#if 0
static boolean_t
pmap_page_test_mappings(vm_page_t m, boolean_t accessed, boolean_t modified)
{
	struct rwlock *lock;
	pv_entry_t pv;
	struct md_page *pvh;
	pt_entry_t *pte, mask;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
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
		mask = 0;
		if (modified) {
			PG_M = pmap_modified_bit(pmap);
			PG_RW = pmap_rw_bit(pmap);
			mask |= PG_RW | PG_M;
		}
		if (accessed) {
			PG_A = pmap_accessed_bit(pmap);
			PG_V = pmap_valid_bit(pmap);
			mask |= PG_V | PG_A;
		}
		rv = (*pte & mask) == mask;
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
			pte = pmap_pde(pmap, pv->pv_va);
			mask = 0;
			if (modified) {
				PG_M = pmap_modified_bit(pmap);
				PG_RW = pmap_rw_bit(pmap);
				mask |= PG_RW | PG_M;
			}
			if (accessed) {
				PG_A = pmap_accessed_bit(pmap);
				PG_V = pmap_valid_bit(pmap);
				mask |= PG_V | PG_A;
			}
			rv = (*pte & mask) == mask;
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
#endif /* 0 */

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_page_t m)
{

	panic("pmap_is_modified");
#if 0
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
#endif
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
	pt_entry_t *l3;
	boolean_t rv;

	rv = FALSE;
	PMAP_LOCK(pmap);
	l3 = pmap_l3(pmap, addr);
	if (l3 != NULL && *l3 != 0) {
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
	panic("pmap_is_referenced");
//	return (pmap_page_test_mappings(m, TRUE, FALSE));
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	panic("pmap_remove_write");
#if 0
	struct md_page *pvh;
	pmap_t pmap;
	struct rwlock *lock;
	pv_entry_t next_pv, pv;
	pd_entry_t *pde;
	pt_entry_t oldpte, *pte, PG_M, PG_RW;
	vm_offset_t va;
	int pvh_gen, md_gen;

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
		PG_RW = pmap_rw_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		if ((*pde & PG_RW) != 0)
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
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0,
		    ("pmap_remove_write: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
retry:
		oldpte = *pte;
		if (oldpte & PG_RW) {
			if (!atomic_cmpset_long(pte, oldpte, oldpte &
			    ~(PG_RW | PG_M)))
				goto retry;
			if ((oldpte & PG_M) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_runlock(&pvh_global_lock);
#endif
}

#if 0
static __inline boolean_t
safe_to_clear_referenced(pmap_t pmap, pt_entry_t pte)
{

	if (!pmap_emulate_ad_bits(pmap))
		return (TRUE);

	KASSERT(pmap->pm_type == PT_EPT, ("invalid pm_type %d", pmap->pm_type));

	/*
	 * XWR = 010 or 110 will cause an unconditional EPT misconfiguration
	 * so we don't let the referenced (aka EPT_PG_READ) bit to be cleared
	 * if the EPT_PG_WRITE bit is set.
	 */
	if ((pte & EPT_PG_WRITE) != 0)
		return (FALSE);

	/*
	 * XWR = 100 is allowed only if the PMAP_SUPPORTS_EXEC_ONLY is set.
	 */
	if ((pte & EPT_PG_EXECUTE) == 0 ||
	    ((pmap->pm_flags & PMAP_SUPPORTS_EXEC_ONLY) != 0))
		return (TRUE);
	else
		return (FALSE);
}
#endif /* 0 */

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
	panic("pmap_ts_referenced");
#if 0
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte, PG_A;
	vm_offset_t va;
	vm_paddr_t pa;
	int cleared, md_gen, not_cleared, pvh_gen;
	struct spglist free;
	boolean_t demoted;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	SLIST_INIT(&free);
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
		PG_A = pmap_accessed_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, pv->pv_va);
		oldpde = *pde;
		if ((*pde & PG_A) != 0) {
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
			    (*pde & PG_W) == 0) {
				if (safe_to_clear_referenced(pmap, oldpde)) {
					atomic_clear_long(pde, PG_A);
					pmap_invalidate_page(pmap, pv->pv_va);
					demoted = FALSE;
				} else if (pmap_demote_pde_locked(pmap, pde,
				    pv->pv_va, &lock)) {
					/*
					 * Remove the mapping to a single page
					 * so that a subsequent access may
					 * repromote.  Since the underlying
					 * page table page is fully populated,
					 * this removal never frees a page
					 * table page.
					 */
					demoted = TRUE;
					va += VM_PAGE_TO_PHYS(m) - (oldpde &
					    PG_PS_FRAME);
					pte = pmap_pde_to_pte(pde, va);
					pmap_remove_pte(pmap, pte, va, *pde,
					    NULL, &lock);
					pmap_invalidate_page(pmap, va);
				} else
					demoted = TRUE;

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
		PG_A = pmap_accessed_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0,
		    ("pmap_ts_referenced: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if ((*pte & PG_A) != 0) {
			if (safe_to_clear_referenced(pmap, *pte)) {
				atomic_clear_long(pte, PG_A);
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
			} else if ((*pte & PG_W) == 0) {
				/*
				 * Wired pages cannot be paged out so
				 * doing accessed bit emulation for
				 * them is wasted effort. We do the
				 * hard work for unwired pages only.
				 */
				pmap_remove_pte(pmap, pte, pv->pv_va,
				    *pde, &free, &lock);
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
	rw_runlock(&pvh_global_lock);
	pmap_free_zero_pages(&free);
	return (cleared + not_cleared);
#endif
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	panic("pmap_advise");
#if 0
	struct rwlock *lock;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte, PG_A, PG_G, PG_M, PG_RW, PG_V;
	vm_offset_t va_next;
	vm_page_t m;
	boolean_t anychanged, pv_lists_locked;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;

	/*
	 * A/D bit emulation requires an alternate code path when clearing
	 * the modified and accessed bits below. Since this function is
	 * advisory in nature we skip it entirely for pmaps that require
	 * A/D bit emulation.
	 */
	if (pmap_emulate_ad_bits(pmap))
		return;

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	pv_lists_locked = FALSE;
resume:
	anychanged = FALSE;
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pml4e = pmap_pml4e(pmap, sva);
		if ((*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0) {
			va_next = (sva + NBPDP) & ~PDPMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;
		pde = pmap_pdpe_to_pde(pdpe, sva);
		oldpde = *pde;
		if ((oldpde & PG_V) == 0)
			continue;
		else if ((oldpde & PG_PS) != 0) {
			if ((oldpde & PG_MANAGED) == 0)
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
				 * The large page mapping was destroyed.
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
			if ((oldpde & PG_W) == 0) {
				pte = pmap_pde_to_pte(pde, sva);
				KASSERT((*pte & PG_V) != 0,
				    ("pmap_advise: invalid PTE"));
				pmap_remove_pte(pmap, pte, sva, *pde, NULL,
				    &lock);
				anychanged = TRUE;
			}
			if (lock != NULL)
				rw_wunlock(lock);
		}
		if (va_next > eva)
			va_next = eva;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & (PG_MANAGED | PG_V)) != (PG_MANAGED |
			    PG_V))
				continue;
			else if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Future calls to pmap_is_modified()
					 * can be avoided by making the page
					 * dirty now.
					 */
					m = PHYS_TO_VM_PAGE(*pte & PG_FRAME);
					vm_page_dirty(m);
				}
				atomic_clear_long(pte, PG_M | PG_A);
			} else if ((*pte & PG_A) != 0)
				atomic_clear_long(pte, PG_A);
			else
				continue;
			if ((*pte & PG_G) != 0)
				pmap_invalidate_page(pmap, sva);
			else
				anychanged = TRUE;
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	if (pv_lists_locked)
		rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
#endif /* 0 */
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	panic("pmap_clear_modify");
#if 0
	struct md_page *pvh;
	pmap_t pmap;
	pv_entry_t next_pv, pv;
	pd_entry_t oldpde, *pde;
	pt_entry_t oldpte, *pte, PG_M, PG_RW, PG_V;
	struct rwlock *lock;
	vm_offset_t va;
	int md_gen, pvh_gen;

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
		PG_M = pmap_modified_bit(pmap);
		PG_V = pmap_valid_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		oldpde = *pde;
		if ((oldpde & PG_RW) != 0) {
			if (pmap_demote_pde_locked(pmap, pde, va, &lock)) {
				if ((oldpde & PG_W) == 0) {
					/*
					 * Write protect the mapping to a
					 * single page so that a subsequent
					 * write access may repromote.
					 */
					va += VM_PAGE_TO_PHYS(m) - (oldpde &
					    PG_PS_FRAME);
					pte = pmap_pde_to_pte(pde, va);
					oldpte = *pte;
					if ((oldpte & PG_V) != 0) {
						while (!atomic_cmpset_long(pte,
						    oldpte,
						    oldpte & ~(PG_M | PG_RW)))
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
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_clear_modify: found"
		    " a 2mpage in page %p's pv list", m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			atomic_clear_long(pte, PG_M);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	rw_runlock(&pvh_global_lock);
#endif
}

#if 0
/*
 * Miscellaneous support routines follow
 */

/* Adjust the cache mode for a 4KB page mapped via a PTE. */
static __inline void
pmap_pte_attr(pt_entry_t *pte, int cache_bits, int mask)
{
	u_int opte, npte;

	/*
	 * The cache mode bits are all in the low 32-bits of the
	 * PTE, so we can just spin on updating the low 32-bits.
	 */
	do {
		opte = *(u_int *)pte;
		npte = opte & ~mask;
		npte |= cache_bits;
	} while (npte != opte && !atomic_cmpset_int((u_int *)pte, opte, npte));
}

/* Adjust the cache mode for a 2MB page mapped via a PDE. */
static __inline void
pmap_pde_attr(pd_entry_t *pde, int cache_bits, int mask)
{
	u_int opde, npde;

	/*
	 * The cache mode bits are all in the low 32-bits of the
	 * PDE, so we can just spin on updating the low 32-bits.
	 */
	do {
		opde = *(u_int *)pde;
		npde = opde & ~mask;
		npde |= cache_bits;
	} while (npde != opde && !atomic_cmpset_int((u_int *)pde, opde, npde));
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, int mode)
{
	vm_offset_t va, offset;
	vm_size_t tmpsize;

	/*
	 * If the specified range of physical addresses fits within the direct
	 * map window, use the direct map. 
	 */
	if (pa < dmaplimit && pa + size < dmaplimit) {
		va = PHYS_TO_DMAP(pa);
		if (!pmap_change_attr(va, size, mode))
			return ((void *)va);
	}
	offset = pa & PAGE_MASK;
	size = round_page(offset + size);
	va = kva_alloc(size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");
	pa = trunc_page(pa);
	for (tmpsize = 0; tmpsize < size; tmpsize += PAGE_SIZE)
		pmap_kenter_attr(va + tmpsize, pa + tmpsize, mode);
	pmap_invalidate_range(kernel_pmap, va, va + tmpsize);
	pmap_invalidate_cache_range(va, va + tmpsize);
	return ((void *)(va + offset));
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_attr(pa, size, PAT_UNCACHEABLE));
}

void *
pmap_mapbios(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_attr(pa, size, PAT_WRITE_BACK));
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	vm_offset_t base, offset;

	/* If we gave a direct map region in pmap_mapdev, do nothing */
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS)
		return;
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);
	kva_free(base, size);
}

/*
 * Tries to demote a 1GB page mapping.
 */
static boolean_t
pmap_demote_pdpe(pmap_t pmap, pdp_entry_t *pdpe, vm_offset_t va)
{
	pdp_entry_t newpdpe, oldpdpe;
	pd_entry_t *firstpde, newpde, *pde;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t mpdepa;
	vm_page_t mpde;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpdpe = *pdpe;
	KASSERT((oldpdpe & (PG_PS | PG_V)) == (PG_PS | PG_V),
	    ("pmap_demote_pdpe: oldpdpe is missing PG_PS and/or PG_V"));
	if ((mpde = vm_page_alloc(NULL, va >> PDPSHIFT, VM_ALLOC_INTERRUPT |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
		CTR2(KTR_PMAP, "pmap_demote_pdpe: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	mpdepa = VM_PAGE_TO_PHYS(mpde);
	firstpde = (pd_entry_t *)PHYS_TO_DMAP(mpdepa);
	newpdpe = mpdepa | PG_M | PG_A | (oldpdpe & PG_U) | PG_RW | PG_V;
	KASSERT((oldpdpe & PG_A) != 0,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_A"));
	KASSERT((oldpdpe & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_M"));
	newpde = oldpdpe;

	/*
	 * Initialize the page directory page.
	 */
	for (pde = firstpde; pde < firstpde + NPDEPG; pde++) {
		*pde = newpde;
		newpde += NBPDR;
	}

	/*
	 * Demote the mapping.
	 */
	*pdpe = newpdpe;

	/*
	 * Invalidate a stale recursive mapping of the page directory page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopde(va));

	pmap_pdpe_demotions++;
	CTR2(KTR_PMAP, "pmap_demote_pdpe: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}
#endif

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	panic("pmap_page_set_memattr");
#if 0
	m->md.pat_mode = ma;

	/*
	 * If "m" is a normal page, update its direct mapping.  This update
	 * can be relied upon to perform any cache operations that are
	 * required for data coherence.
	 */
	if ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_change_attr(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)), PAGE_SIZE,
	    m->md.pat_mode))
		panic("memory attribute change on the direct map failed");
#endif
}

#if 0
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
int
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
	vm_paddr_t pa_start, pa_end;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	int cache_bits_pte, cache_bits_pde, error;
	boolean_t changed;

	PMAP_LOCK_ASSERT(kernel_pmap, MA_OWNED);
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	/*
	 * Only supported on kernel virtual addresses, including the direct
	 * map but excluding the recursive map.
	 */
	if (base < DMAP_MIN_ADDRESS)
		return (EINVAL);

	cache_bits_pde = pmap_cache_bits(kernel_pmap, mode, 1);
	cache_bits_pte = pmap_cache_bits(kernel_pmap, mode, 0);
	changed = FALSE;

	/*
	 * Pages that aren't mapped aren't supported.  Also break down 2MB pages
	 * into 4KB pages if required.
	 */
	for (tmpva = base; tmpva < base + size; ) {
		pdpe = pmap_pdpe(kernel_pmap, tmpva);
		if (*pdpe == 0)
			return (EINVAL);
		if (*pdpe & PG_PS) {
			/*
			 * If the current 1GB page already has the required
			 * memory type, then we need not demote this page. Just
			 * increment tmpva to the next 1GB page frame.
			 */
			if ((*pdpe & X86_PG_PDE_CACHE) == cache_bits_pde) {
				tmpva = trunc_1gpage(tmpva) + NBPDP;
				continue;
			}

			/*
			 * If the current offset aligns with a 1GB page frame
			 * and there is at least 1GB left within the range, then
			 * we need not break down this page into 2MB pages.
			 */
			if ((tmpva & PDPMASK) == 0 &&
			    tmpva + PDPMASK < base + size) {
				tmpva += NBPDP;
				continue;
			}
			if (!pmap_demote_pdpe(kernel_pmap, pdpe, tmpva))
				return (ENOMEM);
		}
		pde = pmap_pdpe_to_pde(pdpe, tmpva);
		if (*pde == 0)
			return (EINVAL);
		if (*pde & PG_PS) {
			/*
			 * If the current 2MB page already has the required
			 * memory type, then we need not demote this page. Just
			 * increment tmpva to the next 2MB page frame.
			 */
			if ((*pde & X86_PG_PDE_CACHE) == cache_bits_pde) {
				tmpva = trunc_2mpage(tmpva) + NBPDR;
				continue;
			}

			/*
			 * If the current offset aligns with a 2MB page frame
			 * and there is at least 2MB left within the range, then
			 * we need not break down this page into 4KB pages.
			 */
			if ((tmpva & PDRMASK) == 0 &&
			    tmpva + PDRMASK < base + size) {
				tmpva += NBPDR;
				continue;
			}
			if (!pmap_demote_pde(kernel_pmap, pde, tmpva))
				return (ENOMEM);
		}
		pte = pmap_pde_to_pte(pde, tmpva);
		if (*pte == 0)
			return (EINVAL);
		tmpva += PAGE_SIZE;
	}
	error = 0;

	/*
	 * Ok, all the pages exist, so run through them updating their
	 * cache mode if required.
	 */
	pa_start = pa_end = 0;
	for (tmpva = base; tmpva < base + size; ) {
		pdpe = pmap_pdpe(kernel_pmap, tmpva);
		if (*pdpe & PG_PS) {
			if ((*pdpe & X86_PG_PDE_CACHE) != cache_bits_pde) {
				pmap_pde_attr(pdpe, cache_bits_pde,
				    X86_PG_PDE_CACHE);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pdpe & PG_PS_FRAME;
					pa_end = pa_start + NBPDP;
				} else if (pa_end == (*pdpe & PG_PS_FRAME))
					pa_end += NBPDP;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pdpe & PG_PS_FRAME;
					pa_end = pa_start + NBPDP;
				}
			}
			tmpva = trunc_1gpage(tmpva) + NBPDP;
			continue;
		}
		pde = pmap_pdpe_to_pde(pdpe, tmpva);
		if (*pde & PG_PS) {
			if ((*pde & X86_PG_PDE_CACHE) != cache_bits_pde) {
				pmap_pde_attr(pde, cache_bits_pde,
				    X86_PG_PDE_CACHE);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pde & PG_PS_FRAME;
					pa_end = pa_start + NBPDR;
				} else if (pa_end == (*pde & PG_PS_FRAME))
					pa_end += NBPDR;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pde & PG_PS_FRAME;
					pa_end = pa_start + NBPDR;
				}
			}
			tmpva = trunc_2mpage(tmpva) + NBPDR;
		} else {
			pte = pmap_pde_to_pte(pde, tmpva);
			if ((*pte & X86_PG_PTE_CACHE) != cache_bits_pte) {
				pmap_pte_attr(pte, cache_bits_pte,
				    X86_PG_PTE_CACHE);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pte & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				} else if (pa_end == (*pte & PG_FRAME))
					pa_end += PAGE_SIZE;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pte & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				}
			}
			tmpva += PAGE_SIZE;
		}
	}
	if (error == 0 && pa_start != pa_end)
		error = pmap_change_attr_locked(PHYS_TO_DMAP(pa_start),
		    pa_end - pa_start, mode);

	/*
	 * Flush CPU caches if required to make sure any data isn't cached that
	 * shouldn't be, etc.
	 */
	if (changed) {
		pmap_invalidate_range(kernel_pmap, base, tmpva);
		pmap_invalidate_cache_range(base, tmpva);
	}
	return (error);
}

/*
 * Demotes any mapping within the direct map region that covers more than the
 * specified range of physical addresses.  This range's size must be a power
 * of two and its starting address must be a multiple of its size.  Since the
 * demotion does not change any attributes of the mapping, a TLB invalidation
 * is not mandatory.  The caller may, however, request a TLB invalidation.
 */
void
pmap_demote_DMAP(vm_paddr_t base, vm_size_t len, boolean_t invalidate)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	vm_offset_t va;
	boolean_t changed;

	if (len == 0)
		return;
	KASSERT(powerof2(len), ("pmap_demote_DMAP: len is not a power of 2"));
	KASSERT((base & (len - 1)) == 0,
	    ("pmap_demote_DMAP: base is not a multiple of len"));
	if (len < NBPDP && base < dmaplimit) {
		va = PHYS_TO_DMAP(base);
		changed = FALSE;
		PMAP_LOCK(kernel_pmap);
		pdpe = pmap_pdpe(kernel_pmap, va);
		if ((*pdpe & X86_PG_V) == 0)
			panic("pmap_demote_DMAP: invalid PDPE");
		if ((*pdpe & PG_PS) != 0) {
			if (!pmap_demote_pdpe(kernel_pmap, pdpe, va))
				panic("pmap_demote_DMAP: PDPE failed");
			changed = TRUE;
		}
		if (len < NBPDR) {
			pde = pmap_pdpe_to_pde(pdpe, va);
			if ((*pde & X86_PG_V) == 0)
				panic("pmap_demote_DMAP: invalid PDE");
			if ((*pde & PG_PS) != 0) {
				if (!pmap_demote_pde(kernel_pmap, pde, va))
					panic("pmap_demote_DMAP: PDE failed");
				changed = TRUE;
			}
		}
		if (changed && invalidate)
			pmap_invalidate_page(kernel_pmap, va);
		PMAP_UNLOCK(kernel_pmap);
	}
}
#endif /* 0 */

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	panic("pmap_mincore");
#if 0
	pd_entry_t *pdep;
	pt_entry_t pte, PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t pa;
	int val;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK(pmap);
retry:
	pdep = pmap_pde(pmap, addr);
	if (pdep != NULL && (*pdep & PG_V)) {
		if (*pdep & PG_PS) {
			pte = *pdep;
			/* Compute the physical address of the 4KB page. */
			pa = ((*pdep & PG_PS_FRAME) | (addr & PDRMASK)) &
			    PG_FRAME;
			val = MINCORE_SUPER;
		} else {
			pte = *pmap_pde_to_pte(pdep, addr);
			pa = pte & PG_FRAME;
			val = 0;
		}
	} else {
		pte = 0;
		pa = 0;
		val = 0;
	}
	if ((pte & PG_V) != 0) {
		val |= MINCORE_INCORE;
		if ((pte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((pte & PG_A) != 0)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	}
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) &&
	    (pte & (PG_MANAGED | PG_V)) == (PG_MANAGED | PG_V)) {
		/* Ensure that "PHYS_TO_VM_PAGE(pa)->object" doesn't change. */
		if (vm_page_pa_tryrelock(pmap, pa, locked_pa))
			goto retry;
	} else
		PA_UNLOCK_COND(*locked_pa);
	PMAP_UNLOCK(pmap);
	return (val);
#endif
}

void
pmap_activate(struct thread *td)
{
	pmap_t	pmap; //, oldpmap;
	//u_int	cpuid;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
#if 0
	oldpmap = PCPU_GET(curpmap);
	cpuid = PCPU_GET(cpuid);
#ifdef SMP
	CPU_CLR_ATOMIC(cpuid, &oldpmap->pm_active);
	CPU_SET_ATOMIC(cpuid, &pmap->pm_active);
	CPU_SET_ATOMIC(cpuid, &pmap->pm_save);
#else
	CPU_CLR(cpuid, &oldpmap->pm_active);
	CPU_SET(cpuid, &pmap->pm_active);
	CPU_SET(cpuid, &pmap->pm_save);
#endif
#endif
	td->td_pcb->pcb_l1addr = vtophys(pmap->pm_l1);
	__asm __volatile("msr ttbr0_el1, %0" : : "r"(td->td_pcb->pcb_l1addr));
#if 0
	load_cr3(pmap->pm_cr3);
	PCPU_SET(curpmap, pmap);
#endif
	critical_exit();
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{

	panic("pmap_sync_icache");
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
#if 0
	vm_offset_t superpage_offset;

	if (size < NBPDR)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & PDRMASK;
	if (size - ((NBPDR - superpage_offset) & PDRMASK) < NBPDR ||
	    (*addr & PDRMASK) == superpage_offset)
		return;
	if ((*addr & PDRMASK) < superpage_offset)
		*addr = (*addr & ~PDRMASK) + superpage_offset;
	else
		*addr = ((*addr + PDRMASK) & ~PDRMASK) + superpage_offset;
#endif
}

#if 0
#ifdef INVARIANTS
static unsigned long num_dirty_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_dirty_emulations, CTLFLAG_RW,
	     &num_dirty_emulations, 0, NULL);

static unsigned long num_accessed_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_accessed_emulations, CTLFLAG_RW,
	     &num_accessed_emulations, 0, NULL);

static unsigned long num_superpage_accessed_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_superpage_accessed_emulations, CTLFLAG_RW,
	     &num_superpage_accessed_emulations, 0, NULL);

static unsigned long ad_emulation_superpage_promotions;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, ad_emulation_superpage_promotions, CTLFLAG_RW,
	     &ad_emulation_superpage_promotions, 0, NULL);
#endif	/* INVARIANTS */

int
pmap_emulate_accessed_dirty(pmap_t pmap, vm_offset_t va, int ftype)
{
	int rv;
	struct rwlock *lock;
	vm_page_t m, mpte;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_A, PG_M, PG_RW, PG_V;
	boolean_t pv_lists_locked;

	KASSERT(ftype == VM_PROT_READ || ftype == VM_PROT_WRITE,
	    ("pmap_emulate_accessed_dirty: invalid fault type %d", ftype));

	if (!pmap_emulate_ad_bits(pmap))
		return (-1);

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	rv = -1;
	lock = NULL;
	pv_lists_locked = FALSE;
retry:
	PMAP_LOCK(pmap);

	pde = pmap_pde(pmap, va);
	if (pde == NULL || (*pde & PG_V) == 0)
		goto done;

	if ((*pde & PG_PS) != 0) {
		if (ftype == VM_PROT_READ) {
#ifdef INVARIANTS
			atomic_add_long(&num_superpage_accessed_emulations, 1);
#endif
			*pde |= PG_A;
			rv = 0;
		}
		goto done;
	}

	pte = pmap_pde_to_pte(pde, va);
	if ((*pte & PG_V) == 0)
		goto done;

	if (ftype == VM_PROT_WRITE) {
		if ((*pte & PG_RW) == 0)
			goto done;
		*pte |= PG_M;
	}
	*pte |= PG_A;

	/* try to promote the mapping */
	if (va < VM_MAXUSER_ADDRESS)
		mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
	else
		mpte = NULL;

	m = PHYS_TO_VM_PAGE(*pte & PG_FRAME);

	if ((mpte == NULL || mpte->wire_count == NPTEPG) &&
	    pmap_ps_enabled(pmap) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0) {
		if (!pv_lists_locked) {
			pv_lists_locked = TRUE;
			if (!rw_try_rlock(&pvh_global_lock)) {
				PMAP_UNLOCK(pmap);
				rw_rlock(&pvh_global_lock);
				goto retry;
			}
		}
		pmap_promote_pde(pmap, pde, va, &lock);
#ifdef INVARIANTS
		atomic_add_long(&ad_emulation_superpage_promotions, 1);
#endif
	}
#ifdef INVARIANTS
	if (ftype == VM_PROT_WRITE)
		atomic_add_long(&num_dirty_emulations, 1);
	else
		atomic_add_long(&num_accessed_emulations, 1);
#endif
	rv = 0;		/* success */
done:
	if (lock != NULL)
		rw_wunlock(lock);
	if (pv_lists_locked)
		rw_runlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	return (rv);
}

void
pmap_get_mapping(pmap_t pmap, vm_offset_t va, uint64_t *ptr, int *num)
{
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	int idx;

	idx = 0;
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);

	pml4 = pmap_pml4e(pmap, va);
	ptr[idx++] = *pml4;
	if ((*pml4 & PG_V) == 0)
		goto done;

	pdp = pmap_pml4e_to_pdpe(pml4, va);
	ptr[idx++] = *pdp;
	if ((*pdp & PG_V) == 0 || (*pdp & PG_PS) != 0)
		goto done;

	pde = pmap_pdpe_to_pde(pdp, va);
	ptr[idx++] = *pde;
	if ((*pde & PG_V) == 0 || (*pde & PG_PS) != 0)
		goto done;

	pte = pmap_pde_to_pte(pde, va);
	ptr[idx++] = *pte;

done:
	PMAP_UNLOCK(pmap);
	*num = idx;
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(pte, pmap_print_pte)
{
	pmap_t pmap;
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	vm_offset_t va;

	if (have_addr) {
		va = (vm_offset_t)addr;
		pmap = PCPU_GET(curpmap); /* XXX */
	} else {
		db_printf("show pte addr\n");
		return;
	}
	PG_V = pmap_valid_bit(pmap);
	pml4 = pmap_pml4e(pmap, va);
	db_printf("VA %#016lx pml4e %#016lx", va, *pml4);
	if ((*pml4 & PG_V) == 0) {
		db_printf("\n");
		return;
	}
	pdp = pmap_pml4e_to_pdpe(pml4, va);
	db_printf(" pdpe %#016lx", *pdp);
	if ((*pdp & PG_V) == 0 || (*pdp & PG_PS) != 0) {
		db_printf("\n");
		return;
	}
	pde = pmap_pdpe_to_pde(pdp, va);
	db_printf(" pde %#016lx", *pde);
	if ((*pde & PG_V) == 0 || (*pde & PG_PS) != 0) {
		db_printf("\n");
		return;
	}
	pte = pmap_pde_to_pte(pde, va);
	db_printf(" pte %#016lx\n", *pte);
}

DB_SHOW_COMMAND(phys2dmap, pmap_phys2dmap)
{
	vm_paddr_t a;

	if (have_addr) {
		a = (vm_paddr_t)addr;
		db_printf("0x%jx\n", (uintmax_t)PHYS_TO_DMAP(a));
	} else {
		db_printf("show phys2dmap addr\n");
	}
}
#endif
#endif /* 0 */
