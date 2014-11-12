/* From: $NetBSD: pmap.c,v 1.148 2004/04/03 04:35:48 bsh Exp $ */
/*-
 * Copyright 2011 Semihalf
 * Copyright 2004 Olivier Houchard.
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * From: FreeBSD: src/sys/arm/arm/pmap.c,v 1.113 2009/07/24 13:50:29
 */

/*-
 * Copyright (c) 2002-2003 Wasabi Systems, Inc.
 * Copyright (c) 2001 Richard Earnshaw
 * Copyright (c) 2001-2002 Christopher Gilbert
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *      This product includes software developed by Mark Brinicombe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *
 * RiscBSD kernel project
 *
 * pmap.c
 *
 * Machine dependant vm stuff
 *
 * Created      : 20/09/94
 */

/*
 * Special compilation symbols
 * PMAP_DEBUG           - Build in pmap_debug_level code
 *
 * Note that pmap_mapdev() and pmap_unmapdev() are implemented in arm/devmap.c
*/
/* Include header files */

#include "opt_vm.h"
#include "opt_pmap.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/uma.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_reserv.h>

#include <machine/md_var.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/pcb.h>

#ifdef DEBUG
extern int last_fault_code;
#endif

#ifdef PMAP_DEBUG
#define PDEBUG(_lev_,_stat_) \
        if (pmap_debug_level >= (_lev_)) \
                ((_stat_))
#define dprintf printf

int pmap_debug_level = 0;
#define PMAP_INLINE
#else   /* PMAP_DEBUG */
#define PDEBUG(_lev_,_stat_) /* Nothing */
#define dprintf(x, arg...)
#define PMAP_INLINE __inline
#endif  /* PMAP_DEBUG */

#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

#define	pa_to_pvh(pa)	(&pv_table[pa_index(pa)])

#ifdef ARM_L2_PIPT
#define pmap_l2cache_wbinv_range(va, pa, size) cpu_l2cache_wbinv_range((pa), (size))
#define pmap_l2cache_inv_range(va, pa, size) cpu_l2cache_inv_range((pa), (size))
#else
#define pmap_l2cache_wbinv_range(va, pa, size) cpu_l2cache_wbinv_range((va), (size))
#define pmap_l2cache_inv_range(va, pa, size) cpu_l2cache_inv_range((va), (size))
#endif

extern struct pv_addr systempage;

/*
 * Internal function prototypes
 */

static PMAP_INLINE
struct pv_entry		*pmap_find_pv(struct md_page *, pmap_t, vm_offset_t);
static void		pmap_free_pv_chunk(struct pv_chunk *pc);
static void		pmap_free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t 	pmap_get_pv_entry(pmap_t pmap, boolean_t try);
static vm_page_t 	pmap_pv_reclaim(pmap_t locked_pmap);
static boolean_t	pmap_pv_insert_section(pmap_t, vm_offset_t,
    vm_paddr_t);
static struct pv_entry	*pmap_remove_pv(struct vm_page *, pmap_t, vm_offset_t);
static int		pmap_pvh_wired_mappings(struct md_page *, int);

static int		pmap_enter_locked(pmap_t, vm_offset_t, vm_page_t,
    vm_prot_t, u_int);
static vm_paddr_t	pmap_extract_locked(pmap_t pmap, vm_offset_t va);
static void		pmap_alloc_l1(pmap_t);
static void		pmap_free_l1(pmap_t);

static void		pmap_map_section(pmap_t, vm_offset_t, vm_offset_t,
    vm_prot_t, boolean_t);
static void		pmap_promote_section(pmap_t, vm_offset_t);
static boolean_t	pmap_demote_section(pmap_t, vm_offset_t);
static boolean_t	pmap_enter_section(pmap_t, vm_offset_t, vm_page_t,
    vm_prot_t);
static void		pmap_remove_section(pmap_t, vm_offset_t);

static int		pmap_clearbit(struct vm_page *, u_int);

static struct l2_bucket *pmap_get_l2_bucket(pmap_t, vm_offset_t);
static struct l2_bucket *pmap_alloc_l2_bucket(pmap_t, vm_offset_t);
static void		pmap_free_l2_bucket(pmap_t, struct l2_bucket *, u_int);
static vm_offset_t	kernel_pt_lookup(vm_paddr_t);

static MALLOC_DEFINE(M_VMPMAP, "pmap", "PMAP L1");

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t pmap_curmaxkvaddr;
vm_paddr_t kernel_l1pa;

vm_offset_t kernel_vm_end = 0;

vm_offset_t vm_max_kernel_address;

struct pmap kernel_pmap_store;

/*
 * Resources for quickly copying and zeroing pages using virtual address space
 * and page table entries that are pre-allocated per-CPU by pmap_init().
 */
struct czpages {
	struct	mtx 	lock;
	pt_entry_t	*srcptep;
	pt_entry_t	*dstptep;
	vm_offset_t	srcva;
	vm_offset_t	dstva;
};
static struct czpages cpu_czpages[MAXCPU];

static void		pmap_init_l1(struct l1_ttable *, pd_entry_t *);
/*
 * These routines are called when the CPU type is identified to set up
 * the PTE prototypes, cache modes, etc.
 *
 * The variables are always here, just in case LKMs need to reference
 * them (though, they shouldn't).
 */
static void pmap_set_prot(pt_entry_t *pte, vm_prot_t prot, uint8_t user);
pt_entry_t	pte_l1_s_cache_mode;
pt_entry_t	pte_l1_s_cache_mode_pt;

pt_entry_t	pte_l2_l_cache_mode;
pt_entry_t	pte_l2_l_cache_mode_pt;

pt_entry_t	pte_l2_s_cache_mode;
pt_entry_t	pte_l2_s_cache_mode_pt;

struct msgbuf *msgbufp = 0;

/*
 * Crashdump maps.
 */
static caddr_t crashdumpmap;

extern void bcopy_page(vm_offset_t, vm_offset_t);
extern void bzero_page(vm_offset_t);

char *_tmppt;

/*
 * Metadata for L1 translation tables.
 */
struct l1_ttable {
	/* Entry on the L1 Table list */
	SLIST_ENTRY(l1_ttable) l1_link;

	/* Entry on the L1 Least Recently Used list */
	TAILQ_ENTRY(l1_ttable) l1_lru;

	/* Track how many domains are allocated from this L1 */
	volatile u_int l1_domain_use_count;

	/*
	 * A free-list of domain numbers for this L1.
	 * We avoid using ffs() and a bitmap to track domains since ffs()
	 * is slow on ARM.
	 */
	u_int8_t l1_domain_first;
	u_int8_t l1_domain_free[PMAP_DOMAINS];

	/* Physical address of this L1 page table */
	vm_paddr_t l1_physaddr;

	/* KVA of this L1 page table */
	pd_entry_t *l1_kva;
};

/*
 * Convert a virtual address into its L1 table index. That is, the
 * index used to locate the L2 descriptor table pointer in an L1 table.
 * This is basically used to index l1->l1_kva[].
 *
 * Each L2 descriptor table represents 1MB of VA space.
 */
#define	L1_IDX(va)		(((vm_offset_t)(va)) >> L1_S_SHIFT)

/*
 * L1 Page Tables are tracked using a Least Recently Used list.
 *  - New L1s are allocated from the HEAD.
 *  - Freed L1s are added to the TAIl.
 *  - Recently accessed L1s (where an 'access' is some change to one of
 *    the userland pmaps which owns this L1) are moved to the TAIL.
 */
static TAILQ_HEAD(, l1_ttable) l1_lru_list;
/*
 * A list of all L1 tables
 */
static SLIST_HEAD(, l1_ttable) l1_list;
static struct mtx l1_lru_lock;

/*
 * The l2_dtable tracks L2_BUCKET_SIZE worth of L1 slots.
 *
 * This is normally 16MB worth L2 page descriptors for any given pmap.
 * Reference counts are maintained for L2 descriptors so they can be
 * freed when empty.
 */
struct l2_dtable {
	/* The number of L2 page descriptors allocated to this l2_dtable */
	u_int l2_occupancy;

	/* List of L2 page descriptors */
	struct l2_bucket {
		pt_entry_t *l2b_kva;	/* KVA of L2 Descriptor Table */
		vm_paddr_t l2b_phys;	/* Physical address of same */
		u_short l2b_l1idx;	/* This L2 table's L1 index */
		u_short l2b_occupancy;	/* How many active descriptors */
	} l2_bucket[L2_BUCKET_SIZE];
};

/* pmap_kenter_internal flags */
#define KENTER_CACHE	0x1
#define KENTER_DEVICE	0x2
#define KENTER_USER	0x4

/*
 * Given an L1 table index, calculate the corresponding l2_dtable index
 * and bucket index within the l2_dtable.
 */
#define	L2_IDX(l1idx)		(((l1idx) >> L2_BUCKET_LOG2) & \
				 (L2_SIZE - 1))
#define	L2_BUCKET(l1idx)	((l1idx) & (L2_BUCKET_SIZE - 1))

/*
 * Given a virtual address, this macro returns the
 * virtual address required to drop into the next L2 bucket.
 */
#define	L2_NEXT_BUCKET(va)	(((va) & L1_S_FRAME) + L1_S_SIZE)

/*
 * We try to map the page tables write-through, if possible.  However, not
 * all CPUs have a write-through cache mode, so on those we have to sync
 * the cache when we frob page tables.
 *
 * We try to evaluate this at compile time, if possible.  However, it's
 * not always possible to do that, hence this run-time var.
 */
int	pmap_needs_pte_sync;

/*
 * Macro to determine if a mapping might be resident in the
 * instruction cache and/or TLB
 */
#define	PTE_BEEN_EXECD(pte)  (L2_S_EXECUTABLE(pte) && L2_S_REFERENCED(pte))

/*
 * Macro to determine if a mapping might be resident in the
 * data cache and/or TLB
 */
#define	PTE_BEEN_REFD(pte)   (L2_S_REFERENCED(pte))

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#define pmap_is_current(pm)	((pm) == pmap_kernel() || \
            curproc->p_vmspace->vm_map.pmap == (pm))

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static int pv_entry_count, pv_entry_max, pv_entry_high_water;
static struct md_page *pv_table;
static int shpgperproc = PMAP_SHPGPERPROC;

struct pv_chunk *pv_chunkbase;		/* KVA block for pv_chunks */
int pv_maxchunks;			/* How many chunks we have KVA for */
vm_offset_t pv_vafree;			/* Freelist stored in the PTE */

static __inline struct pv_chunk *
pv_to_chunk(pv_entry_t pv)
{

	return ((struct pv_chunk *)((uintptr_t)pv & ~(uintptr_t)PAGE_MASK));
}

#define PV_PMAP(pv) (pv_to_chunk(pv)->pc_pmap)

CTASSERT(sizeof(struct pv_chunk) == PAGE_SIZE);
CTASSERT(_NPCM == 8);
CTASSERT(_NPCPV == 252);

#define	PC_FREE0_6	0xfffffffful	/* Free values for index 0 through 6 */
#define	PC_FREE7	0x0ffffffful	/* Free values for index 7 */

static const uint32_t pc_freemask[_NPCM] = {
	PC_FREE0_6, PC_FREE0_6, PC_FREE0_6,
	PC_FREE0_6, PC_FREE0_6, PC_FREE0_6,
	PC_FREE0_6, PC_FREE7
};

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

/* Superpages utilization enabled = 1 / disabled = 0 */
static int sp_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, sp_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &sp_enabled, 0,
    "Are large page mappings enabled?");

SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_count, CTLFLAG_RD, &pv_entry_count, 0,
    "Current number of pv entries");

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

static long pv_entry_frees, pv_entry_allocs;
static int pv_entry_spare;

SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_frees, CTLFLAG_RD, &pv_entry_frees, 0,
    "Current number of pv entry frees");
SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_allocs, CTLFLAG_RD, &pv_entry_allocs, 0,
    "Current number of pv entry allocs");
SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_spare, CTLFLAG_RD, &pv_entry_spare, 0,
    "Current number of spare pv entries");
#endif

uma_zone_t l2zone;
static uma_zone_t l2table_zone;
static vm_offset_t pmap_kernel_l2dtable_kva;
static vm_offset_t pmap_kernel_l2ptp_kva;
static vm_paddr_t pmap_kernel_l2ptp_phys;
static struct rwlock pvh_global_lock;

int l1_mem_types[] = {
	ARM_L1S_STRONG_ORD,
	ARM_L1S_DEVICE_NOSHARE,
	ARM_L1S_DEVICE_SHARE,
	ARM_L1S_NRML_NOCACHE,
	ARM_L1S_NRML_IWT_OWT,
	ARM_L1S_NRML_IWB_OWB,
	ARM_L1S_NRML_IWBA_OWBA
};

int l2l_mem_types[] = {
	ARM_L2L_STRONG_ORD,
	ARM_L2L_DEVICE_NOSHARE,
	ARM_L2L_DEVICE_SHARE,
	ARM_L2L_NRML_NOCACHE,
	ARM_L2L_NRML_IWT_OWT,
	ARM_L2L_NRML_IWB_OWB,
	ARM_L2L_NRML_IWBA_OWBA
};

int l2s_mem_types[] = {
	ARM_L2S_STRONG_ORD,
	ARM_L2S_DEVICE_NOSHARE,
	ARM_L2S_DEVICE_SHARE,
	ARM_L2S_NRML_NOCACHE,
	ARM_L2S_NRML_IWT_OWT,
	ARM_L2S_NRML_IWB_OWB,
	ARM_L2S_NRML_IWBA_OWBA
};

/*
 * This list exists for the benefit of pmap_map_chunk().  It keeps track
 * of the kernel L2 tables during bootstrap, so that pmap_map_chunk() can
 * find them as necessary.
 *
 * Note that the data on this list MUST remain valid after initarm() returns,
 * as pmap_bootstrap() uses it to contruct L2 table metadata.
 */
SLIST_HEAD(, pv_addr) kernel_pt_list = SLIST_HEAD_INITIALIZER(kernel_pt_list);

static void
pmap_init_l1(struct l1_ttable *l1, pd_entry_t *l1pt)
{
	int i;

	l1->l1_kva = l1pt;
	l1->l1_domain_use_count = 0;
	l1->l1_domain_first = 0;

	for (i = 0; i < PMAP_DOMAINS; i++)
		l1->l1_domain_free[i] = i + 1;

	/*
	 * Copy the kernel's L1 entries to each new L1.
	 */
	if (l1pt != pmap_kernel()->pm_l1->l1_kva)
		memcpy(l1pt, pmap_kernel()->pm_l1->l1_kva, L1_TABLE_SIZE);

	if ((l1->l1_physaddr = pmap_extract(pmap_kernel(), (vm_offset_t)l1pt)) == 0)
		panic("pmap_init_l1: can't get PA of L1 at %p", l1pt);
	SLIST_INSERT_HEAD(&l1_list, l1, l1_link);
	TAILQ_INSERT_TAIL(&l1_lru_list, l1, l1_lru);
}

static vm_offset_t
kernel_pt_lookup(vm_paddr_t pa)
{
	struct pv_addr *pv;

	SLIST_FOREACH(pv, &kernel_pt_list, pv_list) {
		if (pv->pv_pa == pa)
			return (pv->pv_va);
	}
	return (0);
}

void
pmap_pte_init_mmu_v6(void)
{

	if (PTE_PAGETABLE >= 3)
		pmap_needs_pte_sync = 1;
	pte_l1_s_cache_mode = l1_mem_types[PTE_CACHE];
	pte_l2_l_cache_mode = l2l_mem_types[PTE_CACHE];
	pte_l2_s_cache_mode = l2s_mem_types[PTE_CACHE];

	pte_l1_s_cache_mode_pt = l1_mem_types[PTE_PAGETABLE];
	pte_l2_l_cache_mode_pt = l2l_mem_types[PTE_PAGETABLE];
	pte_l2_s_cache_mode_pt = l2s_mem_types[PTE_PAGETABLE];

}

/*
 * Allocate an L1 translation table for the specified pmap.
 * This is called at pmap creation time.
 */
static void
pmap_alloc_l1(pmap_t pmap)
{
	struct l1_ttable *l1;
	u_int8_t domain;

	/*
	 * Remove the L1 at the head of the LRU list
	 */
	mtx_lock(&l1_lru_lock);
	l1 = TAILQ_FIRST(&l1_lru_list);
	TAILQ_REMOVE(&l1_lru_list, l1, l1_lru);

	/*
	 * Pick the first available domain number, and update
	 * the link to the next number.
	 */
	domain = l1->l1_domain_first;
	l1->l1_domain_first = l1->l1_domain_free[domain];

	/*
	 * If there are still free domain numbers in this L1,
	 * put it back on the TAIL of the LRU list.
	 */
	if (++l1->l1_domain_use_count < PMAP_DOMAINS)
		TAILQ_INSERT_TAIL(&l1_lru_list, l1, l1_lru);

	mtx_unlock(&l1_lru_lock);

	/*
	 * Fix up the relevant bits in the pmap structure
	 */
	pmap->pm_l1 = l1;
	pmap->pm_domain = domain + 1;
}

/*
 * Free an L1 translation table.
 * This is called at pmap destruction time.
 */
static void
pmap_free_l1(pmap_t pmap)
{
	struct l1_ttable *l1 = pmap->pm_l1;

	mtx_lock(&l1_lru_lock);

	/*
	 * If this L1 is currently on the LRU list, remove it.
	 */
	if (l1->l1_domain_use_count < PMAP_DOMAINS)
		TAILQ_REMOVE(&l1_lru_list, l1, l1_lru);

	/*
	 * Free up the domain number which was allocated to the pmap
	 */
	l1->l1_domain_free[pmap->pm_domain - 1] = l1->l1_domain_first;
	l1->l1_domain_first = pmap->pm_domain - 1;
	l1->l1_domain_use_count--;

	/*
	 * The L1 now must have at least 1 free domain, so add
	 * it back to the LRU list. If the use count is zero,
	 * put it at the head of the list, otherwise it goes
	 * to the tail.
	 */
	if (l1->l1_domain_use_count == 0) {
		TAILQ_INSERT_HEAD(&l1_lru_list, l1, l1_lru);
	}	else
		TAILQ_INSERT_TAIL(&l1_lru_list, l1, l1_lru);

	mtx_unlock(&l1_lru_lock);
}

/*
 * Returns a pointer to the L2 bucket associated with the specified pmap
 * and VA, or NULL if no L2 bucket exists for the address.
 */
static PMAP_INLINE struct l2_bucket *
pmap_get_l2_bucket(pmap_t pmap, vm_offset_t va)
{
	struct l2_dtable *l2;
	struct l2_bucket *l2b;
	u_short l1idx;

	l1idx = L1_IDX(va);

	if ((l2 = pmap->pm_l2[L2_IDX(l1idx)]) == NULL ||
	    (l2b = &l2->l2_bucket[L2_BUCKET(l1idx)])->l2b_kva == NULL)
		return (NULL);

	return (l2b);
}

/*
 * Returns a pointer to the L2 bucket associated with the specified pmap
 * and VA.
 *
 * If no L2 bucket exists, perform the necessary allocations to put an L2
 * bucket/page table in place.
 *
 * Note that if a new L2 bucket/page was allocated, the caller *must*
 * increment the bucket occupancy counter appropriately *before*
 * releasing the pmap's lock to ensure no other thread or cpu deallocates
 * the bucket/page in the meantime.
 */
static struct l2_bucket *
pmap_alloc_l2_bucket(pmap_t pmap, vm_offset_t va)
{
	struct l2_dtable *l2;
	struct l2_bucket *l2b;
	u_short l1idx;

	l1idx = L1_IDX(va);

	PMAP_ASSERT_LOCKED(pmap);
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if ((l2 = pmap->pm_l2[L2_IDX(l1idx)]) == NULL) {
		/*
		 * No mapping at this address, as there is
		 * no entry in the L1 table.
		 * Need to allocate a new l2_dtable.
		 */
		PMAP_UNLOCK(pmap);
		rw_wunlock(&pvh_global_lock);
		if ((l2 = uma_zalloc(l2table_zone, M_NOWAIT)) == NULL) {
			rw_wlock(&pvh_global_lock);
			PMAP_LOCK(pmap);
			return (NULL);
		}
		rw_wlock(&pvh_global_lock);
		PMAP_LOCK(pmap);
		if (pmap->pm_l2[L2_IDX(l1idx)] != NULL) {
			/*
			 * Someone already allocated the l2_dtable while
			 * we were doing the same.
			 */
			uma_zfree(l2table_zone, l2);
			l2 = pmap->pm_l2[L2_IDX(l1idx)];
		} else {
			bzero(l2, sizeof(*l2));
			/*
			 * Link it into the parent pmap
			 */
			pmap->pm_l2[L2_IDX(l1idx)] = l2;
		}
	}

	l2b = &l2->l2_bucket[L2_BUCKET(l1idx)];

	/*
	 * Fetch pointer to the L2 page table associated with the address.
	 */
	if (l2b->l2b_kva == NULL) {
		pt_entry_t *ptep;

		/*
		 * No L2 page table has been allocated. Chances are, this
		 * is because we just allocated the l2_dtable, above.
		 */
		PMAP_UNLOCK(pmap);
		rw_wunlock(&pvh_global_lock);
		ptep = uma_zalloc(l2zone, M_NOWAIT);
		rw_wlock(&pvh_global_lock);
		PMAP_LOCK(pmap);
		if (l2b->l2b_kva != 0) {
			/* We lost the race. */
			uma_zfree(l2zone, ptep);
			return (l2b);
		}
		l2b->l2b_phys = vtophys(ptep);
		if (ptep == NULL) {
			/*
			 * Oops, no more L2 page tables available at this
			 * time. We may need to deallocate the l2_dtable
			 * if we allocated a new one above.
			 */
			if (l2->l2_occupancy == 0) {
				pmap->pm_l2[L2_IDX(l1idx)] = NULL;
				uma_zfree(l2table_zone, l2);
			}
			return (NULL);
		}

		l2->l2_occupancy++;
		l2b->l2b_kva = ptep;
		l2b->l2b_l1idx = l1idx;
	}

	return (l2b);
}

static PMAP_INLINE void
pmap_free_l2_ptp(pt_entry_t *l2)
{
	uma_zfree(l2zone, l2);
}
/*
 * One or more mappings in the specified L2 descriptor table have just been
 * invalidated.
 *
 * Garbage collect the metadata and descriptor table itself if necessary.
 *
 * The pmap lock must be acquired when this is called (not necessary
 * for the kernel pmap).
 */
static void
pmap_free_l2_bucket(pmap_t pmap, struct l2_bucket *l2b, u_int count)
{
	struct l2_dtable *l2;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep;
	u_short l1idx;


	/*
	 * Update the bucket's reference count according to how many
	 * PTEs the caller has just invalidated.
	 */
	l2b->l2b_occupancy -= count;

	/*
	 * Note:
	 *
	 * Level 2 page tables allocated to the kernel pmap are never freed
	 * as that would require checking all Level 1 page tables and
	 * removing any references to the Level 2 page table. See also the
	 * comment elsewhere about never freeing bootstrap L2 descriptors.
	 *
	 * We make do with just invalidating the mapping in the L2 table.
	 *
	 * This isn't really a big deal in practice and, in fact, leads
	 * to a performance win over time as we don't need to continually
	 * alloc/free.
	 */
	if (l2b->l2b_occupancy > 0 || pmap == pmap_kernel())
		return;

	/*
	 * There are no more valid mappings in this level 2 page table.
	 * Go ahead and NULL-out the pointer in the bucket, then
	 * free the page table.
	 */
	l1idx = l2b->l2b_l1idx;
	ptep = l2b->l2b_kva;
	l2b->l2b_kva = NULL;

	pl1pd = &pmap->pm_l1->l1_kva[l1idx];

	/*
	 * If the L1 slot matches the pmap's domain
	 * number, then invalidate it.
	 */
	l1pd = *pl1pd & (L1_TYPE_MASK | L1_C_DOM_MASK);
	if (l1pd == (L1_C_DOM(pmap->pm_domain) | L1_TYPE_C)) {
		*pl1pd = 0;
		PTE_SYNC(pl1pd);
		cpu_tlb_flushD_SE((vm_offset_t)ptep);
		cpu_cpwait();
	}

	/*
	 * Release the L2 descriptor table back to the pool cache.
	 */
	pmap_free_l2_ptp(ptep);

	/*
	 * Update the reference count in the associated l2_dtable
	 */
	l2 = pmap->pm_l2[L2_IDX(l1idx)];
	if (--l2->l2_occupancy > 0)
		return;

	/*
	 * There are no more valid mappings in any of the Level 1
	 * slots managed by this l2_dtable. Go ahead and NULL-out
	 * the pointer in the parent pmap and free the l2_dtable.
	 */
	pmap->pm_l2[L2_IDX(l1idx)] = NULL;
	uma_zfree(l2table_zone, l2);
}

/*
 * Pool cache constructors for L2 descriptor tables, metadata and pmap
 * structures.
 */
static int
pmap_l2ptp_ctor(void *mem, int size, void *arg, int flags)
{
	struct l2_bucket *l2b;
	pt_entry_t *ptep, pte;
	vm_offset_t va = (vm_offset_t)mem & ~PAGE_MASK;

	/*
	 * The mappings for these page tables were initially made using
	 * pmap_kenter() by the pool subsystem. Therefore, the cache-
	 * mode will not be right for page table mappings. To avoid
	 * polluting the pmap_kenter() code with a special case for
	 * page tables, we simply fix up the cache-mode here if it's not
	 * correct.
	 */
	l2b = pmap_get_l2_bucket(pmap_kernel(), va);
	ptep = &l2b->l2b_kva[l2pte_index(va)];
	pte = *ptep;

	cpu_idcache_wbinv_range(va, PAGE_SIZE);
	pmap_l2cache_wbinv_range(va, pte & L2_S_FRAME, PAGE_SIZE);
	if ((pte & L2_S_CACHE_MASK) != pte_l2_s_cache_mode_pt) {
		/*
		 * Page tables must have the cache-mode set to
		 * Write-Thru.
		 */
		*ptep = (pte & ~L2_S_CACHE_MASK) | pte_l2_s_cache_mode_pt;
		PTE_SYNC(ptep);
		cpu_tlb_flushD_SE(va);
		cpu_cpwait();
	}

	memset(mem, 0, L2_TABLE_SIZE_REAL);
	return (0);
}

/*
 * Modify pte bits for all ptes corresponding to the given physical address.
 * We use `maskbits' rather than `clearbits' because we're always passing
 * constants and the latter would require an extra inversion at run-time.
 */
static int
pmap_clearbit(struct vm_page *m, u_int maskbits)
{
	struct l2_bucket *l2b;
	struct pv_entry *pv, *pve, *next_pv;
	struct md_page *pvh;
	pd_entry_t *pl1pd;
	pt_entry_t *ptep, npte, opte;
	pmap_t pmap;
	vm_offset_t va;
	u_int oflags;
	int count = 0;

	rw_wlock(&pvh_global_lock);
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;

	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_list, next_pv) {
		va = pv->pv_va;
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
		KASSERT((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO,
		    ("pmap_clearbit: valid section mapping expected"));
		if ((maskbits & PVF_WRITE) && (pv->pv_flags & PVF_WRITE))
			(void)pmap_demote_section(pmap, va);
		else if ((maskbits & PVF_REF) && L1_S_REFERENCED(*pl1pd)) {
			if (pmap_demote_section(pmap, va)) {
				if ((pv->pv_flags & PVF_WIRED) == 0) {
					/*
					 * Remove the mapping to a single page
					 * so that a subsequent access may
					 * repromote. Since the underlying
					 * l2_bucket is fully populated, this
					 * removal never frees an entire
					 * l2_bucket.
					 */
					va += (VM_PAGE_TO_PHYS(m) &
					    L1_S_OFFSET);
					l2b = pmap_get_l2_bucket(pmap, va);
					KASSERT(l2b != NULL,
					    ("pmap_clearbit: no l2 bucket for "
					     "va 0x%#x, pmap 0x%p", va, pmap));
					ptep = &l2b->l2b_kva[l2pte_index(va)];
					*ptep = 0;
					PTE_SYNC(ptep);
					pmap_free_l2_bucket(pmap, l2b, 1);
					pve = pmap_remove_pv(m, pmap, va);
					KASSERT(pve != NULL, ("pmap_clearbit: "
					    "no PV entry for managed mapping"));
					pmap_free_pv_entry(pmap, pve);

				}
			}
		} else if ((maskbits & PVF_MOD) && L1_S_WRITABLE(*pl1pd)) {
			if (pmap_demote_section(pmap, va)) {
				if ((pv->pv_flags & PVF_WIRED) == 0) {
					/*
					 * Write protect the mapping to a
					 * single page so that a subsequent
					 * write access may repromote.
					 */
					va += (VM_PAGE_TO_PHYS(m) &
					    L1_S_OFFSET);
					l2b = pmap_get_l2_bucket(pmap, va);
					KASSERT(l2b != NULL,
					    ("pmap_clearbit: no l2 bucket for "
					     "va 0x%#x, pmap 0x%p", va, pmap));
					ptep = &l2b->l2b_kva[l2pte_index(va)];
					if ((*ptep & L2_S_PROTO) != 0) {
						pve = pmap_find_pv(&m->md,
						    pmap, va);
						KASSERT(pve != NULL,
						    ("pmap_clearbit: no PV "
						    "entry for managed mapping"));
						pve->pv_flags &= ~PVF_WRITE;
						*ptep |= L2_APX;
						PTE_SYNC(ptep);
					}
				}
			}
		}
		PMAP_UNLOCK(pmap);
	}

small_mappings:
	if (TAILQ_EMPTY(&m->md.pv_list)) {
		rw_wunlock(&pvh_global_lock);
		return (0);
	}

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		va = pv->pv_va;
		pmap = PV_PMAP(pv);
		oflags = pv->pv_flags;
		pv->pv_flags &= ~maskbits;

		PMAP_LOCK(pmap);

		l2b = pmap_get_l2_bucket(pmap, va);
		KASSERT(l2b != NULL, ("pmap_clearbit: no l2 bucket for "
		    "va 0x%#x, pmap 0x%p", va, pmap));

		ptep = &l2b->l2b_kva[l2pte_index(va)];
		npte = opte = *ptep;

		if (maskbits & (PVF_WRITE | PVF_MOD)) {
			/* make the pte read only */
			npte |= L2_APX;
		}

		if (maskbits & PVF_REF) {
			/*
			 * Clear referenced flag in PTE so that we
			 * will take a flag fault the next time the mapping
			 * is referenced.
			 */
			npte &= ~L2_S_REF;
		}

		CTR4(KTR_PMAP,"clearbit: pmap:%p bits:%x pte:%x->%x",
		    pmap, maskbits, opte, npte);
		if (npte != opte) {
			count++;
			*ptep = npte;
			PTE_SYNC(ptep);
			/* Flush the TLB entry if a current pmap. */
			if (PTE_BEEN_EXECD(opte))
				cpu_tlb_flushID_SE(pv->pv_va);
			else if (PTE_BEEN_REFD(opte))
				cpu_tlb_flushD_SE(pv->pv_va);
			cpu_cpwait();
		}

		PMAP_UNLOCK(pmap);

	}

	if (maskbits & PVF_WRITE)
		vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(&pvh_global_lock);
	return (count);
}

/*
 * main pv_entry manipulation functions:
 *   pmap_enter_pv: enter a mapping onto a vm_page list
 *   pmap_remove_pv: remove a mappiing from a vm_page list
 *
 * NOTE: pmap_enter_pv expects to lock the pvh itself
 *       pmap_remove_pv expects the caller to lock the pvh before calling
 */

/*
 * pmap_enter_pv: enter a mapping onto a vm_page's PV list
 *
 * => caller should hold the proper lock on pvh_global_lock
 * => caller should have pmap locked
 * => we will (someday) gain the lock on the vm_page's PV list
 * => caller should adjust ptp's wire_count before calling
 * => caller should not adjust pmap's wire_count
 */
static void
pmap_enter_pv(struct vm_page *m, struct pv_entry *pve, pmap_t pmap,
    vm_offset_t va, u_int flags)
{

	rw_assert(&pvh_global_lock, RA_WLOCKED);

	PMAP_ASSERT_LOCKED(pmap);
	pve->pv_va = va;
	pve->pv_flags = flags;

	TAILQ_INSERT_HEAD(&m->md.pv_list, pve, pv_list);
	if (pve->pv_flags & PVF_WIRED)
		++pmap->pm_stats.wired_count;
}

/*
 *
 * pmap_find_pv: Find a pv entry
 *
 * => caller should hold lock on vm_page
 */
static PMAP_INLINE struct pv_entry *
pmap_find_pv(struct md_page *md, pmap_t pmap, vm_offset_t va)
{
	struct pv_entry *pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	TAILQ_FOREACH(pv, &md->pv_list, pv_list)
		if (pmap == PV_PMAP(pv) && va == pv->pv_va)
			break;

	return (pv);
}

/*
 * vector_page_setprot:
 *
 *	Manipulate the protection of the vector page.
 */
void
vector_page_setprot(int prot)
{
	struct l2_bucket *l2b;
	pt_entry_t *ptep;

	l2b = pmap_get_l2_bucket(pmap_kernel(), vector_page);

	ptep = &l2b->l2b_kva[l2pte_index(vector_page)];
	/*
	 * Set referenced flag.
	 * Vectors' page is always desired
	 * to be allowed to reside in TLB. 
	 */
	*ptep |= L2_S_REF;

	pmap_set_prot(ptep, prot|VM_PROT_EXECUTE, 0);
	PTE_SYNC(ptep);
	cpu_tlb_flushID_SE(vector_page);
	cpu_cpwait();
}

static void
pmap_set_prot(pt_entry_t *ptep, vm_prot_t prot, uint8_t user)
{

	*ptep &= ~(L2_S_PROT_MASK | L2_XN);

	if (!(prot & VM_PROT_EXECUTE))
		*ptep |= L2_XN;

	/* Set defaults first - kernel read access */
	*ptep |= L2_APX;
	*ptep |= L2_S_PROT_R;
	/* Now tune APs as desired */
	if (user)
		*ptep |= L2_S_PROT_U;

	if (prot & VM_PROT_WRITE)
		*ptep &= ~(L2_APX);
}

/*
 * pmap_remove_pv: try to remove a mapping from a pv_list
 *
 * => caller should hold proper lock on pmap_main_lock
 * => pmap should be locked
 * => caller should hold lock on vm_page [so that attrs can be adjusted]
 * => caller should adjust ptp's wire_count and free PTP if needed
 * => caller should NOT adjust pmap's wire_count
 * => we return the removed pve
 */
static struct pv_entry *
pmap_remove_pv(struct vm_page *m, pmap_t pmap, vm_offset_t va)
{
	struct pv_entry *pve;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_ASSERT_LOCKED(pmap);

	pve = pmap_find_pv(&m->md, pmap, va);	/* find corresponding pve */
	if (pve != NULL) {
		TAILQ_REMOVE(&m->md.pv_list, pve, pv_list);
		if (pve->pv_flags & PVF_WIRED)
			--pmap->pm_stats.wired_count;
	}
	if (TAILQ_EMPTY(&m->md.pv_list))
		vm_page_aflag_clear(m, PGA_WRITEABLE);

	return(pve);				/* return removed pve */
}

/*
 *
 * pmap_modify_pv: Update pv flags
 *
 * => caller should hold lock on vm_page [so that attrs can be adjusted]
 * => caller should NOT adjust pmap's wire_count
 * => we return the old flags
 *
 * Modify a physical-virtual mapping in the pv table
 */
static u_int
pmap_modify_pv(struct vm_page *m, pmap_t pmap, vm_offset_t va,
    u_int clr_mask, u_int set_mask)
{
	struct pv_entry *npv;
	u_int flags, oflags;

	PMAP_ASSERT_LOCKED(pmap);
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if ((npv = pmap_find_pv(&m->md, pmap, va)) == NULL)
		return (0);

	/*
	 * There is at least one VA mapping this page.
	 */
	oflags = npv->pv_flags;
	npv->pv_flags = flags = (oflags & ~clr_mask) | set_mask;

	if ((flags ^ oflags) & PVF_WIRED) {
		if (flags & PVF_WIRED)
			++pmap->pm_stats.wired_count;
		else
			--pmap->pm_stats.wired_count;
	}

	return (oflags);
}

/* Function to set the debug level of the pmap code */
#ifdef PMAP_DEBUG
void
pmap_debug(int level)
{
	pmap_debug_level = level;
	dprintf("pmap_debug: level=%d\n", pmap_debug_level);
}
#endif  /* PMAP_DEBUG */

void
pmap_pinit0(struct pmap *pmap)
{
	PDEBUG(1, printf("pmap_pinit0: pmap = %08x\n", (u_int32_t) pmap));

	bcopy(kernel_pmap, pmap, sizeof(*pmap));
	bzero(&pmap->pm_mtx, sizeof(pmap->pm_mtx));
	PMAP_LOCK_INIT(pmap);
	TAILQ_INIT(&pmap->pm_pvchunk);
}

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_memattr = VM_MEMATTR_DEFAULT;
}

static vm_offset_t
pmap_ptelist_alloc(vm_offset_t *head)
{
	pt_entry_t *pte;
	vm_offset_t va;

	va = *head;
	if (va == 0)
		return (va);	/* Out of memory */
	pte = vtopte(va);
	*head = *pte;
	if ((*head & L2_TYPE_MASK) != L2_TYPE_INV)
		panic("%s: va is not L2_TYPE_INV!", __func__);
	*pte = 0;
	return (va);
}

static void
pmap_ptelist_free(vm_offset_t *head, vm_offset_t va)
{
	pt_entry_t *pte;

	if ((va & L2_TYPE_MASK) != L2_TYPE_INV)
		panic("%s: freeing va that is not L2_TYPE INV!", __func__);
	pte = vtopte(va);
	*pte = *head;		/* virtual! L2_TYPE is L2_TYPE_INV though */
	*head = va;
}

static void
pmap_ptelist_init(vm_offset_t *head, void *base, int npages)
{
	int i;
	vm_offset_t va;

	*head = 0;
	for (i = npages - 1; i >= 0; i--) {
		va = (vm_offset_t)base + i * PAGE_SIZE;
		pmap_ptelist_free(head, va);
	}
}

/*
 *      Initialize the pmap module.
 *      Called by vm_init, to initialize any structures that the pmap
 *      system needs to map virtual memory.
 */
void
pmap_init(void)
{
	vm_size_t s;
	int i, pv_npg;

	l2zone = uma_zcreate("L2 Table", L2_TABLE_SIZE_REAL, pmap_l2ptp_ctor,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM | UMA_ZONE_NOFREE);
	l2table_zone = uma_zcreate("L2 Table", sizeof(struct l2_dtable), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM | UMA_ZONE_NOFREE);

	/*
	 * Are large page mappings supported and enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.sp_enabled", &sp_enabled);
	if (sp_enabled) {
		KASSERT(MAXPAGESIZES > 1 && pagesizes[1] == 0,
		    ("pmap_init: can't assign to pagesizes[1]"));
		pagesizes[1] = NBPDR;
	}

	/*
	 * Calculate the size of the pv head table for superpages.
	 */
	for (i = 0; phys_avail[i + 1]; i += 2);
	pv_npg = round_1mpage(phys_avail[(i - 2) + 1]) / NBPDR;

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)(pv_npg * sizeof(struct md_page));
	s = round_page(s);
	pv_table = (struct md_page *)kmem_malloc(kernel_arena, s,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);

	/*
	 * Initialize the address space for the pv chunks.
	 */

	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + vm_cnt.v_page_count;
	TUNABLE_INT_FETCH("vm.pmap.pv_entries", &pv_entry_max);
	pv_entry_max = roundup(pv_entry_max, _NPCPV);
	pv_entry_high_water = 9 * (pv_entry_max / 10);

	pv_maxchunks = MAX(pv_entry_max / _NPCPV, maxproc);
	pv_chunkbase = (struct pv_chunk *)kva_alloc(PAGE_SIZE * pv_maxchunks);

	if (pv_chunkbase == NULL)
		panic("pmap_init: not enough kvm for pv chunks");

	pmap_ptelist_init(&pv_vafree, pv_chunkbase, pv_maxchunks);

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	PDEBUG(1, printf("pmap_init: done!\n"));
}

SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_max, CTLFLAG_RD, &pv_entry_max, 0,
	"Max number of PV entries");
SYSCTL_INT(_vm_pmap, OID_AUTO, shpgperproc, CTLFLAG_RD, &shpgperproc, 0,
	"Page share factor per proc");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, section, CTLFLAG_RD, 0,
    "1MB page mapping counters");

static u_long pmap_section_demotions;
SYSCTL_ULONG(_vm_pmap_section, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_section_demotions, 0, "1MB page demotions");

static u_long pmap_section_mappings;
SYSCTL_ULONG(_vm_pmap_section, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_section_mappings, 0, "1MB page mappings");

static u_long pmap_section_p_failures;
SYSCTL_ULONG(_vm_pmap_section, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_section_p_failures, 0, "1MB page promotion failures");

static u_long pmap_section_promotions;
SYSCTL_ULONG(_vm_pmap_section, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_section_promotions, 0, "1MB page promotions");

int
pmap_fault_fixup(pmap_t pmap, vm_offset_t va, vm_prot_t ftype, int user)
{
	struct l2_dtable *l2;
	struct l2_bucket *l2b;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	u_int l1idx;
	int rv = 0;

	l1idx = L1_IDX(va);
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	/*
	 * Check and possibly fix-up L1 section mapping
	 * only when superpage mappings are enabled to speed up.
	 */
	if (sp_enabled) {
		pl1pd = &pmap->pm_l1->l1_kva[l1idx];
		l1pd = *pl1pd;
		if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
			/* Catch an access to the vectors section */
			if (l1idx == L1_IDX(vector_page))
				goto out;
			/*
			 * Stay away from the kernel mappings.
			 * None of them should fault from L1 entry.
			 */
			if (pmap == pmap_kernel())
				goto out;
			/*
			 * Catch a forbidden userland access
			 */
			if (user && !(l1pd & L1_S_PROT_U))
				goto out;
			/*
			 * Superpage is always either mapped read only
			 * or it is modified and permitted to be written
			 * by default. Therefore, process only reference
			 * flag fault and demote page in case of write fault.
			 */
			if ((ftype & VM_PROT_WRITE) && !L1_S_WRITABLE(l1pd) &&
			    L1_S_REFERENCED(l1pd)) {
				(void)pmap_demote_section(pmap, va);
				goto out;
			} else if (!L1_S_REFERENCED(l1pd)) {
				/* Mark the page "referenced" */
				*pl1pd = l1pd | L1_S_REF;
				PTE_SYNC(pl1pd);
				goto l1_section_out;
			} else
				goto out;
		}
	}
	/*
	 * If there is no l2_dtable for this address, then the process
	 * has no business accessing it.
	 *
	 * Note: This will catch userland processes trying to access
	 * kernel addresses.
	 */
	l2 = pmap->pm_l2[L2_IDX(l1idx)];
	if (l2 == NULL)
		goto out;

	/*
	 * Likewise if there is no L2 descriptor table
	 */
	l2b = &l2->l2_bucket[L2_BUCKET(l1idx)];
	if (l2b->l2b_kva == NULL)
		goto out;

	/*
	 * Check the PTE itself.
	 */
	ptep = &l2b->l2b_kva[l2pte_index(va)];
	pte = *ptep;
	if (pte == 0)
		goto out;

	/*
	 * Catch a userland access to the vector page mapped at 0x0
	 */
	if (user && !(pte & L2_S_PROT_U))
		goto out;
	if (va == vector_page)
		goto out;

	pa = l2pte_pa(pte);
	CTR5(KTR_PMAP, "pmap_fault_fix: pmap:%p va:%x pte:0x%x ftype:%x user:%x",
	    pmap, va, pte, ftype, user);
	if ((ftype & VM_PROT_WRITE) && !(L2_S_WRITABLE(pte)) &&
	    L2_S_REFERENCED(pte)) {
		/*
		 * This looks like a good candidate for "page modified"
		 * emulation...
		 */
		struct pv_entry *pv;
		struct vm_page *m;

		/* Extract the physical address of the page */
		if ((m = PHYS_TO_VM_PAGE(pa)) == NULL) {
			goto out;
		}
		/* Get the current flags for this page. */

		pv = pmap_find_pv(&m->md, pmap, va);
		if (pv == NULL) {
			goto out;
		}

		/*
		 * Do the flags say this page is writable? If not then it
		 * is a genuine write fault. If yes then the write fault is
		 * our fault as we did not reflect the write access in the
		 * PTE. Now we know a write has occurred we can correct this
		 * and also set the modified bit
		 */
		if ((pv->pv_flags & PVF_WRITE) == 0) {
			goto out;
		}

		vm_page_dirty(m);

		/* Re-enable write permissions for the page */
		*ptep = (pte & ~L2_APX);
		PTE_SYNC(ptep);
		rv = 1;
		CTR1(KTR_PMAP, "pmap_fault_fix: new pte:0x%x", *ptep);
	} else if (!L2_S_REFERENCED(pte)) {
		/*
		 * This looks like a good candidate for "page referenced"
		 * emulation.
		 */
		struct pv_entry *pv;
		struct vm_page *m;

		/* Extract the physical address of the page */
		if ((m = PHYS_TO_VM_PAGE(pa)) == NULL)
			goto out;
		/* Get the current flags for this page. */
		pv = pmap_find_pv(&m->md, pmap, va);
		if (pv == NULL)
			goto out;

		vm_page_aflag_set(m, PGA_REFERENCED);

		/* Mark the page "referenced" */
		*ptep = pte | L2_S_REF;
		PTE_SYNC(ptep);
		rv = 1;
		CTR1(KTR_PMAP, "pmap_fault_fix: new pte:0x%x", *ptep);
	}

	/*
	 * We know there is a valid mapping here, so simply
	 * fix up the L1 if necessary.
	 */
	pl1pd = &pmap->pm_l1->l1_kva[l1idx];
	l1pd = l2b->l2b_phys | L1_C_DOM(pmap->pm_domain) | L1_C_PROTO;
	if (*pl1pd != l1pd) {
		*pl1pd = l1pd;
		PTE_SYNC(pl1pd);
		rv = 1;
	}

#ifdef DEBUG
	/*
	 * If 'rv == 0' at this point, it generally indicates that there is a
	 * stale TLB entry for the faulting address. This happens when two or
	 * more processes are sharing an L1. Since we don't flush the TLB on
	 * a context switch between such processes, we can take domain faults
	 * for mappings which exist at the same VA in both processes. EVEN IF
	 * WE'VE RECENTLY FIXED UP THE CORRESPONDING L1 in pmap_enter(), for
	 * example.
	 *
	 * This is extremely likely to happen if pmap_enter() updated the L1
	 * entry for a recently entered mapping. In this case, the TLB is
	 * flushed for the new mapping, but there may still be TLB entries for
	 * other mappings belonging to other processes in the 1MB range
	 * covered by the L1 entry.
	 *
	 * Since 'rv == 0', we know that the L1 already contains the correct
	 * value, so the fault must be due to a stale TLB entry.
	 *
	 * Since we always need to flush the TLB anyway in the case where we
	 * fixed up the L1, or frobbed the L2 PTE, we effectively deal with
	 * stale TLB entries dynamically.
	 *
	 * However, the above condition can ONLY happen if the current L1 is
	 * being shared. If it happens when the L1 is unshared, it indicates
	 * that other parts of the pmap are not doing their job WRT managing
	 * the TLB.
	 */
	if (rv == 0 && pmap->pm_l1->l1_domain_use_count == 1) {
		printf("fixup: pmap %p, va 0x%08x, ftype %d - nothing to do!\n",
		    pmap, va, ftype);
		printf("fixup: l2 %p, l2b %p, ptep %p, pl1pd %p\n",
		    l2, l2b, ptep, pl1pd);
		printf("fixup: pte 0x%x, l1pd 0x%x, last code 0x%x\n",
		    pte, l1pd, last_fault_code);
#ifdef DDB
		Debugger();
#endif
	}
#endif

l1_section_out:
	cpu_tlb_flushID_SE(va);
	cpu_cpwait();

	rv = 1;

out:
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	return (rv);
}

void
pmap_postinit(void)
{
	struct l2_bucket *l2b;
	struct l1_ttable *l1;
	pd_entry_t *pl1pt;
	pt_entry_t *ptep, pte;
	vm_offset_t va, eva;
	u_int loop, needed;

	needed = (maxproc / PMAP_DOMAINS) + ((maxproc % PMAP_DOMAINS) ? 1 : 0);
	needed -= 1;
	l1 = malloc(sizeof(*l1) * needed, M_VMPMAP, M_WAITOK);

	for (loop = 0; loop < needed; loop++, l1++) {
		/* Allocate a L1 page table */
		va = (vm_offset_t)contigmalloc(L1_TABLE_SIZE, M_VMPMAP, 0, 0x0,
		    0xffffffff, L1_TABLE_SIZE, 0);

		if (va == 0)
			panic("Cannot allocate L1 KVM");

		eva = va + L1_TABLE_SIZE;
		pl1pt = (pd_entry_t *)va;

		while (va < eva) {
				l2b = pmap_get_l2_bucket(pmap_kernel(), va);
				ptep = &l2b->l2b_kva[l2pte_index(va)];
				pte = *ptep;
				pte = (pte & ~L2_S_CACHE_MASK) | pte_l2_s_cache_mode_pt;
				*ptep = pte;
				PTE_SYNC(ptep);
				cpu_tlb_flushID_SE(va);
				cpu_cpwait();
				va += PAGE_SIZE;
		}
		pmap_init_l1(l1, pl1pt);
	}
#ifdef DEBUG
	printf("pmap_postinit: Allocated %d static L1 descriptor tables\n",
	    needed);
#endif
}

/*
 * This is used to stuff certain critical values into the PCB where they
 * can be accessed quickly from cpu_switch() et al.
 */
void
pmap_set_pcb_pagedir(pmap_t pmap, struct pcb *pcb)
{
	struct l2_bucket *l2b;

	pcb->pcb_pagedir = pmap->pm_l1->l1_physaddr;
	pcb->pcb_dacr = (DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) |
	    (DOMAIN_CLIENT << (pmap->pm_domain * 2));

	if (vector_page < KERNBASE) {
		pcb->pcb_pl1vec = &pmap->pm_l1->l1_kva[L1_IDX(vector_page)];
		l2b = pmap_get_l2_bucket(pmap, vector_page);
		pcb->pcb_l1vec = l2b->l2b_phys | L1_C_PROTO |
		    L1_C_DOM(pmap->pm_domain) | L1_C_DOM(PMAP_DOMAIN_KERNEL);
	} else
		pcb->pcb_pl1vec = NULL;
}

void
pmap_activate(struct thread *td)
{
	pmap_t pmap;
	struct pcb *pcb;

	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	pcb = td->td_pcb;

	critical_enter();
	pmap_set_pcb_pagedir(pmap, pcb);

	if (td == curthread) {
		u_int cur_dacr, cur_ttb;

		__asm __volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(cur_ttb));
		__asm __volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(cur_dacr));

		cur_ttb &= ~(L1_TABLE_SIZE - 1);

		if (cur_ttb == (u_int)pcb->pcb_pagedir &&
		    cur_dacr == pcb->pcb_dacr) {
			/*
			 * No need to switch address spaces.
			 */
			critical_exit();
			return;
		}


		/*
		 * We MUST, I repeat, MUST fix up the L1 entry corresponding
		 * to 'vector_page' in the incoming L1 table before switching
		 * to it otherwise subsequent interrupts/exceptions (including
		 * domain faults!) will jump into hyperspace.
		 */
		if (pcb->pcb_pl1vec) {
			*pcb->pcb_pl1vec = pcb->pcb_l1vec;
		}

		cpu_domains(pcb->pcb_dacr);
		cpu_setttb(pcb->pcb_pagedir);
	}
	critical_exit();
}

static int
pmap_set_pt_cache_mode(pd_entry_t *kl1, vm_offset_t va)
{
	pd_entry_t *pdep, pde;
	pt_entry_t *ptep, pte;
	vm_offset_t pa;
	int rv = 0;

	/*
	 * Make sure the descriptor itself has the correct cache mode
	 */
	pdep = &kl1[L1_IDX(va)];
	pde = *pdep;

	if (l1pte_section_p(pde)) {
		if ((pde & L1_S_CACHE_MASK) != pte_l1_s_cache_mode_pt) {
			*pdep = (pde & ~L1_S_CACHE_MASK) |
			    pte_l1_s_cache_mode_pt;
			PTE_SYNC(pdep);
			rv = 1;
		}
	} else {
		pa = (vm_paddr_t)(pde & L1_C_ADDR_MASK);
		ptep = (pt_entry_t *)kernel_pt_lookup(pa);
		if (ptep == NULL)
			panic("pmap_bootstrap: No L2 for L2 @ va %p\n", ptep);

		ptep = &ptep[l2pte_index(va)];
		pte = *ptep;
		if ((pte & L2_S_CACHE_MASK) != pte_l2_s_cache_mode_pt) {
			*ptep = (pte & ~L2_S_CACHE_MASK) |
			    pte_l2_s_cache_mode_pt;
			PTE_SYNC(ptep);
			rv = 1;
		}
	}

	return (rv);
}

static void
pmap_alloc_specials(vm_offset_t *availp, int pages, vm_offset_t *vap,
    pt_entry_t **ptep)
{
	vm_offset_t va = *availp;
	struct l2_bucket *l2b;

	if (ptep) {
		l2b = pmap_get_l2_bucket(pmap_kernel(), va);
		if (l2b == NULL)
			panic("pmap_alloc_specials: no l2b for 0x%x", va);

		*ptep = &l2b->l2b_kva[l2pte_index(va)];
	}

	*vap = va;
	*availp = va + (PAGE_SIZE * pages);
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On the arm this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
#define PMAP_STATIC_L2_SIZE 16

void
pmap_bootstrap(vm_offset_t firstaddr, struct pv_addr *l1pt)
{
	static struct l1_ttable static_l1;
	static struct l2_dtable static_l2[PMAP_STATIC_L2_SIZE];
	struct l1_ttable *l1 = &static_l1;
	struct l2_dtable *l2;
	struct l2_bucket *l2b;
	struct czpages *czp;
	pd_entry_t pde;
	pd_entry_t *kernel_l1pt = (pd_entry_t *)l1pt->pv_va;
	pt_entry_t *ptep;
	vm_paddr_t pa;
	vm_offset_t va;
	vm_size_t size;
	int i, l1idx, l2idx, l2next = 0;

	PDEBUG(1, printf("firstaddr = %08x, lastaddr = %08x\n",
	    firstaddr, vm_max_kernel_address));

	virtual_avail = firstaddr;
	kernel_pmap->pm_l1 = l1;
	kernel_l1pa = l1pt->pv_pa;

	/*
	 * Scan the L1 translation table created by initarm() and create
	 * the required metadata for all valid mappings found in it.
	 */
	for (l1idx = 0; l1idx < (L1_TABLE_SIZE / sizeof(pd_entry_t)); l1idx++) {
		pde = kernel_l1pt[l1idx];

		/*
		 * We're only interested in Coarse mappings.
		 * pmap_extract() can deal with section mappings without
		 * recourse to checking L2 metadata.
		 */
		if ((pde & L1_TYPE_MASK) != L1_TYPE_C)
			continue;

		/*
		 * Lookup the KVA of this L2 descriptor table
		 */
		pa = (vm_paddr_t)(pde & L1_C_ADDR_MASK);
		ptep = (pt_entry_t *)kernel_pt_lookup(pa);

		if (ptep == NULL) {
			panic("pmap_bootstrap: No L2 for va 0x%x, pa 0x%lx",
			    (u_int)l1idx << L1_S_SHIFT, (long unsigned int)pa);
		}

		/*
		 * Fetch the associated L2 metadata structure.
		 * Allocate a new one if necessary.
		 */
		if ((l2 = kernel_pmap->pm_l2[L2_IDX(l1idx)]) == NULL) {
			if (l2next == PMAP_STATIC_L2_SIZE)
				panic("pmap_bootstrap: out of static L2s");
			kernel_pmap->pm_l2[L2_IDX(l1idx)] = l2 =
			    &static_l2[l2next++];
		}

		/*
		 * One more L1 slot tracked...
		 */
		l2->l2_occupancy++;

		/*
		 * Fill in the details of the L2 descriptor in the
		 * appropriate bucket.
		 */
		l2b = &l2->l2_bucket[L2_BUCKET(l1idx)];
		l2b->l2b_kva = ptep;
		l2b->l2b_phys = pa;
		l2b->l2b_l1idx = l1idx;

		/*
		 * Establish an initial occupancy count for this descriptor
		 */
		for (l2idx = 0;
		    l2idx < (L2_TABLE_SIZE_REAL / sizeof(pt_entry_t));
		    l2idx++) {
			if ((ptep[l2idx] & L2_TYPE_MASK) != L2_TYPE_INV) {
				l2b->l2b_occupancy++;
			}
		}

		/*
		 * Make sure the descriptor itself has the correct cache mode.
		 * If not, fix it, but whine about the problem. Port-meisters
		 * should consider this a clue to fix up their initarm()
		 * function. :)
		 */
		if (pmap_set_pt_cache_mode(kernel_l1pt, (vm_offset_t)ptep)) {
			printf("pmap_bootstrap: WARNING! wrong cache mode for "
			    "L2 pte @ %p\n", ptep);
		}
	}


	/*
	 * Ensure the primary (kernel) L1 has the correct cache mode for
	 * a page table. Bitch if it is not correctly set.
	 */
	for (va = (vm_offset_t)kernel_l1pt;
	    va < ((vm_offset_t)kernel_l1pt + L1_TABLE_SIZE); va += PAGE_SIZE) {
		if (pmap_set_pt_cache_mode(kernel_l1pt, va))
			printf("pmap_bootstrap: WARNING! wrong cache mode for "
			    "primary L1 @ 0x%x\n", va);
	}

	cpu_dcache_wbinv_all();
	cpu_l2cache_wbinv_all();
	cpu_tlb_flushID();
	cpu_cpwait();

	PMAP_LOCK_INIT(kernel_pmap);
	CPU_FILL(&kernel_pmap->pm_active);
	kernel_pmap->pm_domain = PMAP_DOMAIN_KERNEL;
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);

	/*
	 * Initialize the global pv list lock.
	 */
	rw_init(&pvh_global_lock, "pmap pv global");

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages that are being copied or zeroed.
	 */
	for (czp = cpu_czpages, i = 0; i < MAXCPU; ++i, ++czp) {
		mtx_init(&czp->lock, "czpages", NULL, MTX_DEF);
		pmap_alloc_specials(&virtual_avail, 1, &czp->srcva, &czp->srcptep);
		pmap_set_pt_cache_mode(kernel_l1pt, (vm_offset_t)czp->srcptep);
		pmap_alloc_specials(&virtual_avail, 1, &czp->dstva, &czp->dstptep);
		pmap_set_pt_cache_mode(kernel_l1pt, (vm_offset_t)czp->dstptep);
	}

	size = ((vm_max_kernel_address - pmap_curmaxkvaddr) + L1_S_OFFSET) /
	    L1_S_SIZE;
	pmap_alloc_specials(&virtual_avail,
	    round_page(size * L2_TABLE_SIZE_REAL) / PAGE_SIZE,
	    &pmap_kernel_l2ptp_kva, NULL);

	size = (size + (L2_BUCKET_SIZE - 1)) / L2_BUCKET_SIZE;
	pmap_alloc_specials(&virtual_avail,
	    round_page(size * sizeof(struct l2_dtable)) / PAGE_SIZE,
	    &pmap_kernel_l2dtable_kva, NULL);

	pmap_alloc_specials(&virtual_avail,
	    1, (vm_offset_t*)&_tmppt, NULL);
	pmap_alloc_specials(&virtual_avail,
	    MAXDUMPPGS, (vm_offset_t *)&crashdumpmap, NULL);
	SLIST_INIT(&l1_list);
	TAILQ_INIT(&l1_lru_list);
	mtx_init(&l1_lru_lock, "l1 list lock", NULL, MTX_DEF);
	pmap_init_l1(l1, kernel_l1pt);
	cpu_dcache_wbinv_all();
	cpu_l2cache_wbinv_all();
	cpu_tlb_flushID();
	cpu_cpwait();

	virtual_avail = round_page(virtual_avail);
	virtual_end = vm_max_kernel_address;
	kernel_vm_end = pmap_curmaxkvaddr;

	pmap_set_pcb_pagedir(kernel_pmap, thread0.td_pcb);
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
	struct pcb *pcb;

	cpu_tlb_flushID();
	cpu_cpwait();
	if (vector_page < KERNBASE) {
		struct pcb *curpcb = PCPU_GET(curpcb);
		pcb = thread0.td_pcb;
		if (pmap_is_current(pmap)) {
			/*
			 * Frob the L1 entry corresponding to the vector
			 * page so that it contains the kernel pmap's domain
			 * number. This will ensure pmap_remove() does not
			 * pull the current vector page out from under us.
			 */
			critical_enter();
			*pcb->pcb_pl1vec = pcb->pcb_l1vec;
			cpu_domains(pcb->pcb_dacr);
			cpu_setttb(pcb->pcb_pagedir);
			critical_exit();
		}
		pmap_remove(pmap, vector_page, vector_page + PAGE_SIZE);
		/*
		 * Make sure cpu_switch(), et al, DTRT. This is safe to do
		 * since this process has no remaining mappings of its own.
		 */
		curpcb->pcb_pl1vec = pcb->pcb_pl1vec;
		curpcb->pcb_l1vec = pcb->pcb_l1vec;
		curpcb->pcb_dacr = pcb->pcb_dacr;
		curpcb->pcb_pagedir = pcb->pcb_pagedir;

	}
	pmap_free_l1(pmap);

	dprintf("pmap_release()\n");
}



/*
 * Helper function for pmap_grow_l2_bucket()
 */
static __inline int
pmap_grow_map(vm_offset_t va, pt_entry_t cache_mode, vm_paddr_t *pap)
{
	struct l2_bucket *l2b;
	pt_entry_t *ptep;
	vm_paddr_t pa;
	struct vm_page *m;

	m = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ | VM_ALLOC_WIRED);
	if (m == NULL)
		return (1);
	pa = VM_PAGE_TO_PHYS(m);

	if (pap)
		*pap = pa;

	l2b = pmap_get_l2_bucket(pmap_kernel(), va);

	ptep = &l2b->l2b_kva[l2pte_index(va)];
	*ptep = L2_S_PROTO | pa | cache_mode | L2_S_REF;
	pmap_set_prot(ptep, VM_PROT_READ | VM_PROT_WRITE, 0);
	PTE_SYNC(ptep);
	cpu_tlb_flushD_SE(va);
	cpu_cpwait();

	return (0);
}

/*
 * This is the same as pmap_alloc_l2_bucket(), except that it is only
 * used by pmap_growkernel().
 */
static __inline struct l2_bucket *
pmap_grow_l2_bucket(pmap_t pmap, vm_offset_t va)
{
	struct l2_dtable *l2;
	struct l2_bucket *l2b;
	struct l1_ttable *l1;
	pd_entry_t *pl1pd;
	u_short l1idx;
	vm_offset_t nva;

	l1idx = L1_IDX(va);

	if ((l2 = pmap->pm_l2[L2_IDX(l1idx)]) == NULL) {
		/*
		 * No mapping at this address, as there is
		 * no entry in the L1 table.
		 * Need to allocate a new l2_dtable.
		 */
		nva = pmap_kernel_l2dtable_kva;
		if ((nva & PAGE_MASK) == 0) {
			/*
			 * Need to allocate a backing page
			 */
			if (pmap_grow_map(nva, pte_l2_s_cache_mode, NULL))
				return (NULL);
		}

		l2 = (struct l2_dtable *)nva;
		nva += sizeof(struct l2_dtable);

		if ((nva & PAGE_MASK) < (pmap_kernel_l2dtable_kva &
		    PAGE_MASK)) {
			/*
			 * The new l2_dtable straddles a page boundary.
			 * Map in another page to cover it.
			 */
			if (pmap_grow_map(nva, pte_l2_s_cache_mode, NULL))
				return (NULL);
		}

		pmap_kernel_l2dtable_kva = nva;

		/*
		 * Link it into the parent pmap
		 */
		pmap->pm_l2[L2_IDX(l1idx)] = l2;
		memset(l2, 0, sizeof(*l2));
	}

	l2b = &l2->l2_bucket[L2_BUCKET(l1idx)];

	/*
	 * Fetch pointer to the L2 page table associated with the address.
	 */
	if (l2b->l2b_kva == NULL) {
		pt_entry_t *ptep;

		/*
		 * No L2 page table has been allocated. Chances are, this
		 * is because we just allocated the l2_dtable, above.
		 */
		nva = pmap_kernel_l2ptp_kva;
		ptep = (pt_entry_t *)nva;
		if ((nva & PAGE_MASK) == 0) {
			/*
			 * Need to allocate a backing page
			 */
			if (pmap_grow_map(nva, pte_l2_s_cache_mode_pt,
			    &pmap_kernel_l2ptp_phys))
				return (NULL);
		}
		memset(ptep, 0, L2_TABLE_SIZE_REAL);
		l2->l2_occupancy++;
		l2b->l2b_kva = ptep;
		l2b->l2b_l1idx = l1idx;
		l2b->l2b_phys = pmap_kernel_l2ptp_phys;

		pmap_kernel_l2ptp_kva += L2_TABLE_SIZE_REAL;
		pmap_kernel_l2ptp_phys += L2_TABLE_SIZE_REAL;
	}

	/* Distribute new L1 entry to all other L1s */
	SLIST_FOREACH(l1, &l1_list, l1_link) {
			pl1pd = &l1->l1_kva[L1_IDX(va)];
			*pl1pd = l2b->l2b_phys | L1_C_DOM(PMAP_DOMAIN_KERNEL) |
			    L1_C_PROTO;
			PTE_SYNC(pl1pd);
	}
	cpu_tlb_flushID_SE(va);
	cpu_cpwait();

	return (l2b);
}


/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	pmap_t kpmap = pmap_kernel();

	if (addr <= pmap_curmaxkvaddr)
		return;		/* we are OK */

	/*
	 * whoops!   we need to add kernel PTPs
	 */

	/* Map 1MB at a time */
	for (; pmap_curmaxkvaddr < addr; pmap_curmaxkvaddr += L1_S_SIZE)
		pmap_grow_l2_bucket(kpmap, pmap_curmaxkvaddr);

	kernel_vm_end = pmap_curmaxkvaddr;
}

/*
 * Returns TRUE if the given page is mapped individually or as part of
 * a 1MB section.  Otherwise, returns FALSE.
 */
boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	boolean_t rv;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);
	rw_wlock(&pvh_global_lock);
	rv = !TAILQ_EMPTY(&m->md.pv_list) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    !TAILQ_EMPTY(&pa_to_pvh(VM_PAGE_TO_PHYS(m))->pv_list));
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 * Remove all pages from specified address space
 * this aids process exit speeds.  Also, this code
 * is special cased for current process only, but
 * can have the more generic (and slightly slower)
 * mode enabled.  This is much faster than pmap_remove
 * in the case of running down an entire address space.
 */
void
pmap_remove_pages(pmap_t pmap)
{
	struct pv_entry *pv;
 	struct l2_bucket *l2b = NULL;
	struct pv_chunk *pc, *npc;
	struct md_page *pvh;
	pd_entry_t *pl1pd, l1pd;
 	pt_entry_t *ptep;
 	vm_page_t m, mt;
	vm_offset_t va;
	uint32_t inuse, bitmask;
	int allfree, bit, field, idx;
 
 	rw_wlock(&pvh_global_lock);
 	PMAP_LOCK(pmap);

	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = ffs(inuse) - 1;
				bitmask = 1ul << bit;
				idx = field * sizeof(inuse) * NBBY + bit;
				pv = &pc->pc_pventry[idx];
				va = pv->pv_va;
				inuse &= ~bitmask;
				if (pv->pv_flags & PVF_WIRED) {
					/* Cannot remove wired pages now. */
					allfree = 0;
					continue;
				}
				pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
				l1pd = *pl1pd;
				l2b = pmap_get_l2_bucket(pmap, va);
				if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
					pvh = pa_to_pvh(l1pd & L1_S_FRAME);
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_list);
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						m = PHYS_TO_VM_PAGE(l1pd & L1_S_FRAME);
						KASSERT((vm_offset_t)m >= KERNBASE,
						    ("Trying to access non-existent page "
						     "va %x l1pd %x", trunc_1mpage(va), l1pd));
						for (mt = m; mt < &m[L2_PTE_NUM_TOTAL]; mt++) {
							if (TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
						}
					}
					if (l2b != NULL) {
						KASSERT(l2b->l2b_occupancy == L2_PTE_NUM_TOTAL,
						    ("pmap_remove_pages: l2_bucket occupancy error"));
						pmap_free_l2_bucket(pmap, l2b, L2_PTE_NUM_TOTAL);
					}
					pmap->pm_stats.resident_count -= L2_PTE_NUM_TOTAL;
					*pl1pd = 0;
					PTE_SYNC(pl1pd);
				} else {
					KASSERT(l2b != NULL,
					    ("No L2 bucket in pmap_remove_pages"));
					ptep = &l2b->l2b_kva[l2pte_index(va)];
					m = PHYS_TO_VM_PAGE(l2pte_pa(*ptep));
					KASSERT((vm_offset_t)m >= KERNBASE,
					    ("Trying to access non-existent page "
					     "va %x pte %x", va, *ptep));
					TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
					if (TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(l2pte_pa(*ptep));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m, PGA_WRITEABLE);
					}
					*ptep = 0;
					PTE_SYNC(ptep);
					pmap_free_l2_bucket(pmap, l2b, 1);
					pmap->pm_stats.resident_count--;
				}

				/* Mark free */
				PV_STAT(pv_entry_frees++);
				PV_STAT(pv_entry_spare++);
				pv_entry_count--;
				pc->pc_map[field] |= bitmask;
			}
		}
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			pmap_free_pv_chunk(pc);
		}

	}

 	rw_wunlock(&pvh_global_lock);
 	cpu_tlb_flushID();
 	cpu_cpwait();
 	PMAP_UNLOCK(pmap);
}


/***************************************************
 * Low level mapping routines.....
 ***************************************************/

#ifdef ARM_HAVE_SUPERSECTIONS
/* Map a super section into the KVA. */

void
pmap_kenter_supersection(vm_offset_t va, uint64_t pa, int flags)
{
	pd_entry_t pd = L1_S_PROTO | L1_S_SUPERSEC | (pa & L1_SUP_FRAME) |
	    (((pa >> 32) & 0xf) << 20) | L1_S_PROT(PTE_KERNEL,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE) |
	    L1_S_DOM(PMAP_DOMAIN_KERNEL);
	struct l1_ttable *l1;
	vm_offset_t va0, va_end;

	KASSERT(((va | pa) & L1_SUP_OFFSET) == 0,
	    ("Not a valid super section mapping"));
	if (flags & SECTION_CACHE)
		pd |= pte_l1_s_cache_mode;
	else if (flags & SECTION_PT)
		pd |= pte_l1_s_cache_mode_pt;

	va0 = va & L1_SUP_FRAME;
	va_end = va + L1_SUP_SIZE;
	SLIST_FOREACH(l1, &l1_list, l1_link) {
		va = va0;
		for (; va < va_end; va += L1_S_SIZE) {
			l1->l1_kva[L1_IDX(va)] = pd;
			PTE_SYNC(&l1->l1_kva[L1_IDX(va)]);
		}
	}
}
#endif

/* Map a section into the KVA. */

void
pmap_kenter_section(vm_offset_t va, vm_offset_t pa, int flags)
{
	pd_entry_t pd = L1_S_PROTO | pa | L1_S_PROT(PTE_KERNEL,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE) | L1_S_REF |
	    L1_S_DOM(PMAP_DOMAIN_KERNEL);
	struct l1_ttable *l1;

	KASSERT(((va | pa) & L1_S_OFFSET) == 0,
	    ("Not a valid section mapping"));
	if (flags & SECTION_CACHE)
		pd |= pte_l1_s_cache_mode;
	else if (flags & SECTION_PT)
		pd |= pte_l1_s_cache_mode_pt;

	SLIST_FOREACH(l1, &l1_list, l1_link) {
		l1->l1_kva[L1_IDX(va)] = pd;
		PTE_SYNC(&l1->l1_kva[L1_IDX(va)]);
	}
	cpu_tlb_flushID_SE(va);
	cpu_cpwait();
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temp(vm_paddr_t pa, int i)
{
	vm_offset_t va;

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	pmap_kenter(va, pa);
	return ((void *)crashdumpmap);
}

/*
 * add a wired page to the kva
 * note that in order for the mapping to take effect -- you
 * should do a invltlb after doing the pmap_kenter...
 */
static PMAP_INLINE void
pmap_kenter_internal(vm_offset_t va, vm_offset_t pa, int flags)
{
	struct l2_bucket *l2b;
	pt_entry_t *ptep;
	pt_entry_t opte;

	PDEBUG(1, printf("pmap_kenter: va = %08x, pa = %08x\n",
	    (uint32_t) va, (uint32_t) pa));


	l2b = pmap_get_l2_bucket(pmap_kernel(), va);
	if (l2b == NULL)
		l2b = pmap_grow_l2_bucket(pmap_kernel(), va);
	KASSERT(l2b != NULL, ("No L2 Bucket"));

	ptep = &l2b->l2b_kva[l2pte_index(va)];
	opte = *ptep;

	if (flags & KENTER_CACHE)
		*ptep = L2_S_PROTO | l2s_mem_types[PTE_CACHE] | pa | L2_S_REF;
	else if (flags & KENTER_DEVICE)
		*ptep = L2_S_PROTO | l2s_mem_types[PTE_DEVICE] | pa | L2_S_REF;
	else
		*ptep = L2_S_PROTO | l2s_mem_types[PTE_NOCACHE] | pa | L2_S_REF;

	if (flags & KENTER_CACHE) {
		pmap_set_prot(ptep, VM_PROT_READ | VM_PROT_WRITE,
		    flags & KENTER_USER);
	} else {
		pmap_set_prot(ptep, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE,
		    0);
	}

	PTE_SYNC(ptep);
	if (l2pte_valid(opte)) {
		if (L2_S_EXECUTABLE(opte) || L2_S_EXECUTABLE(*ptep))
			cpu_tlb_flushID_SE(va);
		else
			cpu_tlb_flushD_SE(va);
	} else {
		if (opte == 0)
			l2b->l2b_occupancy++;
	}
	cpu_cpwait();

	PDEBUG(1, printf("pmap_kenter: pte = %08x, opte = %08x, npte = %08x\n",
	    (uint32_t) ptep, opte, *ptep));
}

void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pmap_kenter_internal(va, pa, KENTER_CACHE);
}

void
pmap_kenter_nocache(vm_offset_t va, vm_paddr_t pa)
{

	pmap_kenter_internal(va, pa, 0);
}

void
pmap_kenter_device(vm_offset_t va, vm_paddr_t pa)
{

	pmap_kenter_internal(va, pa, KENTER_DEVICE);
}

void
pmap_kenter_user(vm_offset_t va, vm_paddr_t pa)
{

	pmap_kenter_internal(va, pa, KENTER_CACHE|KENTER_USER);
	/*
	 * Call pmap_fault_fixup now, to make sure we'll have no exception
	 * at the first use of the new address, or bad things will happen,
	 * as we use one of these addresses in the exception handlers.
	 */
	pmap_fault_fixup(pmap_kernel(), va, VM_PROT_READ|VM_PROT_WRITE, 1);
}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{

	if (kernel_vm_end == 0)
		return (0);
	return (pmap_extract_locked(kernel_pmap, va));
}

/*
 * remove a page from the kernel pagetables
 */
void
pmap_kremove(vm_offset_t va)
{
	struct l2_bucket *l2b;
	pt_entry_t *ptep, opte;

	l2b = pmap_get_l2_bucket(pmap_kernel(), va);
	if (!l2b)
		return;
	KASSERT(l2b != NULL, ("No L2 Bucket"));
	ptep = &l2b->l2b_kva[l2pte_index(va)];
	opte = *ptep;
	if (l2pte_valid(opte)) {
		va = va & ~PAGE_MASK;
		*ptep = 0;
		PTE_SYNC(ptep);
		if (L2_S_EXECUTABLE(opte))
			cpu_tlb_flushID_SE(va);
		else
			cpu_tlb_flushD_SE(va);
		cpu_cpwait();
	}
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
pmap_map(vm_offset_t *virt, vm_offset_t start, vm_offset_t end, int prot)
{
	vm_offset_t sva = *virt;
	vm_offset_t va = sva;

	PDEBUG(1, printf("pmap_map: virt = %08x, start = %08x, end = %08x, "
	    "prot = %d\n", (uint32_t) *virt, (uint32_t) start, (uint32_t) end,
	    prot));

	while (start < end) {
		pmap_kenter(va, start);
		va += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	*virt = va;
	return (sva);
}

/*
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

	for (i = 0; i < count; i++) {
		pmap_kenter_internal(va, VM_PAGE_TO_PHYS(m[i]),
		    KENTER_CACHE);
		va += PAGE_SIZE;
	}
}


/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (vtophys(va))
			pmap_kremove(va);

		va += PAGE_SIZE;
	}
}


/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
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
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pd_entry_t *pdep;
	pt_entry_t *ptep;

	if (!pmap_get_pde_pte(pmap, addr, &pdep, &ptep))
		return (FALSE);
	KASSERT((pdep != NULL && (l1pte_section_p(*pdep) || ptep != NULL)),
	    ("Valid mapping but no pte ?"));
	if (*pdep != 0 && !l1pte_section_p(*pdep))
		if (*ptep == 0)
			return (TRUE);
	return (FALSE);
}

/*
 * Fetch pointers to the PDE/PTE for the given pmap/VA pair.
 * Returns TRUE if the mapping exists, else FALSE.
 *
 * NOTE: This function is only used by a couple of arm-specific modules.
 * It is not safe to take any pmap locks here, since we could be right
 * in the middle of debugging the pmap anyway...
 *
 * It is possible for this routine to return FALSE even though a valid
 * mapping does exist. This is because we don't lock, so the metadata
 * state may be inconsistent.
 *
 * NOTE: We can return a NULL *ptp in the case where the L1 pde is
 * a "section" mapping.
 */
boolean_t
pmap_get_pde_pte(pmap_t pmap, vm_offset_t va, pd_entry_t **pdp,
    pt_entry_t **ptp)
{
	struct l2_dtable *l2;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep;
	u_short l1idx;

	if (pmap->pm_l1 == NULL)
		return (FALSE);

	l1idx = L1_IDX(va);
	*pdp = pl1pd = &pmap->pm_l1->l1_kva[l1idx];
	l1pd = *pl1pd;

	if (l1pte_section_p(l1pd)) {
		*ptp = NULL;
		return (TRUE);
	}

	if (pmap->pm_l2 == NULL)
		return (FALSE);

	l2 = pmap->pm_l2[L2_IDX(l1idx)];

	if (l2 == NULL ||
	    (ptep = l2->l2_bucket[L2_BUCKET(l1idx)].l2b_kva) == NULL) {
		return (FALSE);
	}

	*ptp = &ptep[l2pte_index(va)];
	return (TRUE);
}

/*
 *      Routine:        pmap_remove_all
 *      Function:
 *              Removes this physical page from
 *              all physical maps in which it resides.
 *              Reflects back modify bits to the pager.
 *
 *      Notes:
 *              Original versions of this routine were very
 *              inefficient because they iteratively called
 *              pmap_remove (slow...)
 */
void
pmap_remove_all(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *ptep;
	struct l2_bucket *l2b;
	boolean_t flush = FALSE;
	pmap_t curpmap;
	u_int is_exec = 0;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	rw_wlock(&pvh_global_lock);
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pd_entry_t *pl1pd;
		pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(pv->pv_va)];
		KASSERT((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO,
		    ("pmap_remove_all: valid section mapping expected"));
		(void)pmap_demote_section(pmap, pv->pv_va);
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	curpmap = vmspace_pmap(curproc->p_vmspace);
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (flush == FALSE && (pmap == curpmap ||
		    pmap == pmap_kernel()))
			flush = TRUE;

		PMAP_LOCK(pmap);
		l2b = pmap_get_l2_bucket(pmap, pv->pv_va);
		KASSERT(l2b != NULL, ("No l2 bucket"));
		ptep = &l2b->l2b_kva[l2pte_index(pv->pv_va)];
		is_exec |= PTE_BEEN_EXECD(*ptep);
		*ptep = 0;
		if (pmap_is_current(pmap))
			PTE_SYNC(ptep);
		pmap_free_l2_bucket(pmap, l2b, 1);
		pmap->pm_stats.resident_count--;
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		if (pv->pv_flags & PVF_WIRED)
			pmap->pm_stats.wired_count--;
		pmap_free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}

	if (flush) {
		if (is_exec)
			cpu_tlb_flushID();
		else
			cpu_tlb_flushD();
		cpu_cpwait();
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(&pvh_global_lock);
}

int
pmap_change_attr(vm_offset_t sva, vm_size_t len, int mode)
{
	vm_offset_t base, offset, tmpva;
	vm_size_t size;
	struct l2_bucket *l2b;
	pt_entry_t *ptep, pte;
	vm_offset_t next_bucket;

	PMAP_LOCK(kernel_pmap);

	base = trunc_page(sva);
	offset = sva & PAGE_MASK;
	size = roundup(offset + len, PAGE_SIZE);

	for (tmpva = base; tmpva < base + size; ) {
		next_bucket = L2_NEXT_BUCKET(tmpva);
		if (next_bucket > base + size)
			next_bucket = base + size;

		l2b = pmap_get_l2_bucket(kernel_pmap, tmpva);
		if (l2b == NULL) {
			tmpva = next_bucket;
			continue;
		}

		ptep = &l2b->l2b_kva[l2pte_index(tmpva)];

		if (*ptep == 0) {
			PMAP_UNLOCK(kernel_pmap);
			return(EINVAL);
		}

		pte = *ptep &~ L2_S_CACHE_MASK;
		cpu_idcache_wbinv_range(tmpva, PAGE_SIZE);
		pmap_l2cache_wbinv_range(tmpva, pte & L2_S_FRAME, PAGE_SIZE);
		*ptep = pte;
		cpu_tlb_flushID_SE(tmpva);
		cpu_cpwait();

		dprintf("%s: for va:%x ptep:%x pte:%x\n",
		    __func__, tmpva, (uint32_t)ptep, pte);
		tmpva += PAGE_SIZE;
	}

	PMAP_UNLOCK(kernel_pmap);

	return (0);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	struct l2_bucket *l2b;
	struct md_page *pvh;
	struct pv_entry *pve;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep, pte;
	vm_offset_t next_bucket;
	u_int is_exec, is_refd;
	int flush;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE) {
		/*
		 * If this is a read->write transition, just ignore it and let
		 * vm_fault() take care of it later.
		 */
		return;
	}

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);

	/*
	 * OK, at this point, we know we're doing write-protect operation.
	 * If the pmap is active, write-back the range.
	 */

	flush = ((eva - sva) >= (PAGE_SIZE * 4)) ? 0 : -1;
	is_exec = is_refd = 0;

	while (sva < eva) {
		next_bucket = L2_NEXT_BUCKET(sva);
		/*
		 * Check for large page.
		 */
		pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(sva)];
		l1pd = *pl1pd;
		if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
			KASSERT(pmap != pmap_kernel(),
			    ("pmap_protect: trying to modify "
			    "kernel section protections"));
			/*
			 * Are we protecting the entire large page? If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L1_S_SIZE == next_bucket &&
			    eva >= next_bucket) {
				l1pd &= ~(L1_S_PROT_MASK | L1_S_XN);
				if (!(prot & VM_PROT_EXECUTE))
					l1pd |= L1_S_XN;
				/*
				 * At this point we are always setting
				 * write-protect bit.
				 */
				l1pd |= L1_S_APX;
				/* All managed superpages are user pages. */
				l1pd |= L1_S_PROT_U;
				*pl1pd = l1pd;
				PTE_SYNC(pl1pd);
				pvh = pa_to_pvh(l1pd & L1_S_FRAME);
				pve = pmap_find_pv(pvh, pmap,
				    trunc_1mpage(sva));
				pve->pv_flags &= ~PVF_WRITE;
				sva = next_bucket;
				continue;
			} else if (!pmap_demote_section(pmap, sva)) {
				/* The large page mapping was destroyed. */
				sva = next_bucket;
				continue;
			}
		}
		if (next_bucket > eva)
			next_bucket = eva;
		l2b = pmap_get_l2_bucket(pmap, sva);
		if (l2b == NULL) {
			sva = next_bucket;
			continue;
		}

		ptep = &l2b->l2b_kva[l2pte_index(sva)];

		while (sva < next_bucket) {
			if ((pte = *ptep) != 0 && L2_S_WRITABLE(pte)) {
				struct vm_page *m;

				m = PHYS_TO_VM_PAGE(l2pte_pa(pte));
				pmap_set_prot(ptep, prot,
				    !(pmap == pmap_kernel()));
				PTE_SYNC(ptep);

				pmap_modify_pv(m, pmap, sva, PVF_WRITE, 0);

				if (flush >= 0) {
					flush++;
					is_exec |= PTE_BEEN_EXECD(pte);
					is_refd |= PTE_BEEN_REFD(pte);
				} else {
					if (PTE_BEEN_EXECD(pte))
						cpu_tlb_flushID_SE(sva);
					else if (PTE_BEEN_REFD(pte))
						cpu_tlb_flushD_SE(sva);
				}
			}

			sva += PAGE_SIZE;
			ptep++;
		}
	}


	if (flush) {
		if (is_exec)
			cpu_tlb_flushID();
		else
		if (is_refd)
			cpu_tlb_flushD();
		cpu_cpwait();
	}
	rw_wunlock(&pvh_global_lock);

	PMAP_UNLOCK(pmap);
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
	struct l2_bucket *l2b;
	int rv;

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	rv = pmap_enter_locked(pmap, va, m, prot, flags);
	if (rv == KERN_SUCCESS) {
		/*
		 * If both the l2b_occupancy and the reservation are fully
		 * populated, then attempt promotion.
		 */
		l2b = pmap_get_l2_bucket(pmap, va);
		if (l2b != NULL && l2b->l2b_occupancy == L2_PTE_NUM_TOTAL &&
		    sp_enabled && (m->flags & PG_FICTITIOUS) == 0 &&
		    vm_reserv_level_iffullpop(m) == 0)
			pmap_promote_section(pmap, va);
	}
	PMAP_UNLOCK(pmap);
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 *	The pvh global and pmap locks must be held.
 */
static int
pmap_enter_locked(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    u_int flags)
{
	struct l2_bucket *l2b = NULL;
	struct vm_page *om;
	struct pv_entry *pve = NULL;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep, npte, opte;
	u_int nflags;
	u_int is_exec, is_refd;
	vm_paddr_t pa;
	u_char user;

	PMAP_ASSERT_LOCKED(pmap);
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if (va == vector_page) {
		pa = systempage.pv_pa;
		m = NULL;
	} else {
		if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
			VM_OBJECT_ASSERT_LOCKED(m->object);
		pa = VM_PAGE_TO_PHYS(m);
	}

	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
	if ((va < VM_MAXUSER_ADDRESS) &&
	    (*pl1pd & L1_TYPE_MASK) == L1_S_PROTO) {
		(void)pmap_demote_section(pmap, va);
	}

	user = 0;
	/*
	 * Make sure userland mappings get the right permissions
	 */
	if (pmap != pmap_kernel() && va != vector_page)
		user = 1;

	nflags = 0;

	if (prot & VM_PROT_WRITE)
		nflags |= PVF_WRITE;
	if ((flags & PMAP_ENTER_WIRED) != 0)
		nflags |= PVF_WIRED;

	PDEBUG(1, printf("pmap_enter: pmap = %08x, va = %08x, m = %08x, "
	    "prot = %x, flags = %x\n", (uint32_t) pmap, va, (uint32_t) m,
	    prot, flags));

	if (pmap == pmap_kernel()) {
		l2b = pmap_get_l2_bucket(pmap, va);
		if (l2b == NULL)
			l2b = pmap_grow_l2_bucket(pmap, va);
	} else {
do_l2b_alloc:
		l2b = pmap_alloc_l2_bucket(pmap, va);
		if (l2b == NULL) {
			if ((flags & PMAP_ENTER_NOSLEEP) == 0) {
				PMAP_UNLOCK(pmap);
				rw_wunlock(&pvh_global_lock);
				VM_WAIT;
				rw_wlock(&pvh_global_lock);
				PMAP_LOCK(pmap);
				goto do_l2b_alloc;
			}
			return (KERN_RESOURCE_SHORTAGE);
		}
	}

	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
	if ((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO)
		panic("pmap_enter: attempt to enter on 1MB page, va: %#x", va);

	ptep = &l2b->l2b_kva[l2pte_index(va)];

	opte = *ptep;
	npte = pa;
	is_exec = is_refd = 0;

	if (opte) {
		if (l2pte_pa(opte) == pa) {
			/*
			 * We're changing the attrs of an existing mapping.
			 */
			if (m != NULL)
				pmap_modify_pv(m, pmap, va,
				    PVF_WRITE | PVF_WIRED, nflags);
			is_exec |= PTE_BEEN_EXECD(opte);
			is_refd |= PTE_BEEN_REFD(opte);
			goto validate;
		}
		if ((om = PHYS_TO_VM_PAGE(l2pte_pa(opte)))) {
			/*
			 * Replacing an existing mapping with a new one.
			 * It is part of our managed memory so we
			 * must remove it from the PV list
			 */
			if ((pve = pmap_remove_pv(om, pmap, va))) {
				is_exec |= PTE_BEEN_EXECD(opte);
				is_refd |= PTE_BEEN_REFD(opte);
		
				if (m && ((m->oflags & VPO_UNMANAGED)))
					pmap_free_pv_entry(pmap, pve);
			}
		}

	} else {
		/*
		 * Keep the stats up to date
		 */
		l2b->l2b_occupancy++;
		pmap->pm_stats.resident_count++;
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m && !(m->oflags & VPO_UNMANAGED))) {
		if ((!pve) && (pve = pmap_get_pv_entry(pmap, FALSE)) == NULL)
			panic("pmap_enter: no pv entries");

		KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva,
		("pmap_enter: managed mapping within the clean submap"));
		KASSERT(pve != NULL, ("No pv"));
		pmap_enter_pv(m, pve, pmap, va, nflags);
	}

validate:
	/* Make the new PTE valid */
	npte |= L2_S_PROTO;
#ifdef SMP
	npte |= L2_SHARED;
#endif
	/* Set defaults first - kernel read access */
	npte |= L2_APX;
	npte |= L2_S_PROT_R;
	/* Set "referenced" flag */
	npte |= L2_S_REF;

	/* Now tune APs as desired */
	if (user)
		npte |= L2_S_PROT_U;
	/*
	 * If this is not a vector_page
	 * then continue setting mapping parameters
	 */
	if (m != NULL) {
		if ((m->oflags & VPO_UNMANAGED) == 0) {
			if (prot & (VM_PROT_ALL)) {
				vm_page_aflag_set(m, PGA_REFERENCED);
			} else {
				/*
				 * Need to do page referenced emulation.
				 */
				npte &= ~L2_S_REF;
			}
		}

		if (prot & VM_PROT_WRITE) {
			if ((m->oflags & VPO_UNMANAGED) == 0) {
				vm_page_aflag_set(m, PGA_WRITEABLE);
				/*
				 * XXX: Skip modified bit emulation for now.
				 *	The emulation reveals problems
				 *	that result in random failures
				 *	during memory allocation on some
				 *	platforms.
				 *	Therefore, the page is marked RW
				 *	immediately.
				 */
				npte &= ~(L2_APX);
				vm_page_dirty(m);
			} else
				npte &= ~(L2_APX);
		}
		if (!(prot & VM_PROT_EXECUTE))
			npte |= L2_XN;

		if (m->md.pv_memattr != VM_MEMATTR_UNCACHEABLE)
			npte |= pte_l2_s_cache_mode;
	}

	CTR5(KTR_PMAP,"enter: pmap:%p va:%x prot:%x pte:%x->%x",
	    pmap, va, prot, opte, npte);
	/*
	 * If this is just a wiring change, the two PTEs will be
	 * identical, so there's no need to update the page table.
	 */
	if (npte != opte) {
		boolean_t is_cached = pmap_is_current(pmap);

		*ptep = npte;
		PTE_SYNC(ptep);
		if (is_cached) {
			/*
			 * We only need to frob the cache/tlb if this pmap
			 * is current
			 */
			if (L1_IDX(va) != L1_IDX(vector_page) &&
			    l2pte_valid(npte)) {
				/*
				 * This mapping is likely to be accessed as
				 * soon as we return to userland. Fix up the
				 * L1 entry to avoid taking another
				 * page/domain fault.
				 */
				l1pd = l2b->l2b_phys |
				    L1_C_DOM(pmap->pm_domain) | L1_C_PROTO;
				if (*pl1pd != l1pd) {
					*pl1pd = l1pd;
					PTE_SYNC(pl1pd);
				}
			}
		}

		if (is_exec)
			cpu_tlb_flushID_SE(va);
		else if (is_refd)
			cpu_tlb_flushD_SE(va);
		cpu_cpwait();
	}

	if ((pmap != pmap_kernel()) && (pmap == &curproc->p_vmspace->vm_pmap))
		cpu_icache_sync_range(va, PAGE_SIZE);
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
	vm_offset_t va;
	vm_page_t m;
	vm_pindex_t diff, psize;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	m = m_start;
	prot &= VM_PROT_READ | VM_PROT_EXECUTE;
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		if ((va & L1_S_OFFSET) == 0 && L2_NEXT_BUCKET(va) <= end &&
		    m->psind == 1 && sp_enabled &&
		    pmap_enter_section(pmap, va, m, prot))
			m = &m[L1_S_SIZE / PAGE_SIZE - 1];
		else
			pmap_enter_locked(pmap, va, m, prot,
			    PMAP_ENTER_NOSLEEP);
		m = TAILQ_NEXT(m, listq);
	}
	PMAP_UNLOCK(pmap);
	rw_wunlock(&pvh_global_lock);
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

	prot &= VM_PROT_READ | VM_PROT_EXECUTE;
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	pmap_enter_locked(pmap, va, m, prot, PMAP_ENTER_NOSLEEP);
	PMAP_UNLOCK(pmap);
	rw_wunlock(&pvh_global_lock);
}

/*
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	XXX Wired mappings of unmanaged pages cannot be counted by this pmap
 *	implementation.
 */
void
pmap_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct l2_bucket *l2b;
	struct md_page *pvh;
	pd_entry_t l1pd;
	pt_entry_t *ptep, pte;
	pv_entry_t pv;
	vm_offset_t next_bucket;
	vm_paddr_t pa;
	vm_page_t m;
 
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	while (sva < eva) {
		next_bucket = L2_NEXT_BUCKET(sva);
		l1pd = pmap->pm_l1->l1_kva[L1_IDX(sva)];
		if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
			pa = l1pd & L1_S_FRAME;
			m = PHYS_TO_VM_PAGE(pa);
			KASSERT(m != NULL && (m->oflags & VPO_UNMANAGED) == 0,
			    ("pmap_unwire: unmanaged 1mpage %p", m));
			pvh = pa_to_pvh(pa);
			pv = pmap_find_pv(pvh, pmap, trunc_1mpage(sva));
			if ((pv->pv_flags & PVF_WIRED) == 0)
				panic("pmap_unwire: pv %p isn't wired", pv);

			/*
			 * Are we unwiring the entire large page? If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L1_S_SIZE == next_bucket &&
			    eva >= next_bucket) {
				pv->pv_flags &= ~PVF_WIRED;
				pmap->pm_stats.wired_count -= L2_PTE_NUM_TOTAL;
				sva = next_bucket;
				continue;
			} else if (!pmap_demote_section(pmap, sva))
				panic("pmap_unwire: demotion failed");
		}
		if (next_bucket > eva)
			next_bucket = eva;
		l2b = pmap_get_l2_bucket(pmap, sva);
		if (l2b == NULL) {
			sva = next_bucket;
			continue;
		}
		for (ptep = &l2b->l2b_kva[l2pte_index(sva)]; sva < next_bucket;
		    sva += PAGE_SIZE, ptep++) {
			if ((pte = *ptep) == 0 ||
			    (m = PHYS_TO_VM_PAGE(l2pte_pa(pte))) == NULL ||
			    (m->oflags & VPO_UNMANAGED) != 0)
				continue;
			pv = pmap_find_pv(&m->md, pmap, sva);
			if ((pv->pv_flags & PVF_WIRED) == 0)
				panic("pmap_unwire: pv %p isn't wired", pv);
			pv->pv_flags &= ~PVF_WIRED;
			pmap->pm_stats.wired_count--;
		}
	}
	rw_wunlock(&pvh_global_lock);
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
	vm_paddr_t pa;

	PMAP_LOCK(pmap);
	pa = pmap_extract_locked(pmap, va);
	PMAP_UNLOCK(pmap);
	return (pa);
}

static vm_paddr_t
pmap_extract_locked(pmap_t pmap, vm_offset_t va)
{
	struct l2_dtable *l2;
	pd_entry_t l1pd;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	u_int l1idx;

	if (kernel_vm_end != 0 && pmap != kernel_pmap)
		PMAP_ASSERT_LOCKED(pmap);
	l1idx = L1_IDX(va);
	l1pd = pmap->pm_l1->l1_kva[l1idx];
	if (l1pte_section_p(l1pd)) {
		/* XXX: what to do about the bits > 32 ? */
		if (l1pd & L1_S_SUPERSEC)
			pa = (l1pd & L1_SUP_FRAME) | (va & L1_SUP_OFFSET);
		else
			pa = (l1pd & L1_S_FRAME) | (va & L1_S_OFFSET);
	} else {
		/*
		 * Note that we can't rely on the validity of the L1
		 * descriptor as an indication that a mapping exists.
		 * We have to look it up in the L2 dtable.
		 */
		l2 = pmap->pm_l2[L2_IDX(l1idx)];
		if (l2 == NULL ||
		    (ptep = l2->l2_bucket[L2_BUCKET(l1idx)].l2b_kva) == NULL)
			return (0);
		pte = ptep[l2pte_index(va)];
		if (pte == 0)
			return (0);
		switch (pte & L2_TYPE_MASK) {
		case L2_TYPE_L:
			pa = (pte & L2_L_FRAME) | (va & L2_L_OFFSET);
			break;
		default:
			pa = (pte & L2_S_FRAME) | (va & L2_S_OFFSET);
			break;
		}
	}
	return (pa);
}

/*
 * Atomically extract and hold the physical page with the given
 * pmap and virtual address pair if that mapping permits the given
 * protection.
 *
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	struct l2_dtable *l2;
	pd_entry_t l1pd;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa, paddr;
	vm_page_t m = NULL;
	u_int l1idx;
	l1idx = L1_IDX(va);
	paddr = 0;

	PMAP_LOCK(pmap);
retry:
	l1pd = pmap->pm_l1->l1_kva[l1idx];
	if (l1pte_section_p(l1pd)) {
		/* XXX: what to do about the bits > 32 ? */
		if (l1pd & L1_S_SUPERSEC)
			pa = (l1pd & L1_SUP_FRAME) | (va & L1_SUP_OFFSET);
		else
			pa = (l1pd & L1_S_FRAME) | (va & L1_S_OFFSET);
		if (vm_page_pa_tryrelock(pmap, pa & PG_FRAME, &paddr))
			goto retry;
		if (L1_S_WRITABLE(l1pd) || (prot & VM_PROT_WRITE) == 0) {
			m = PHYS_TO_VM_PAGE(pa);
			vm_page_hold(m);
		}
	} else {
		/*
		 * Note that we can't rely on the validity of the L1
		 * descriptor as an indication that a mapping exists.
		 * We have to look it up in the L2 dtable.
		 */
		l2 = pmap->pm_l2[L2_IDX(l1idx)];

		if (l2 == NULL ||
		    (ptep = l2->l2_bucket[L2_BUCKET(l1idx)].l2b_kva) == NULL) {
			PMAP_UNLOCK(pmap);
			return (NULL);
		}

		ptep = &ptep[l2pte_index(va)];
		pte = *ptep;

		if (pte == 0) {
			PMAP_UNLOCK(pmap);
			return (NULL);
		} else if ((prot & VM_PROT_WRITE) && (pte & L2_APX)) {
			PMAP_UNLOCK(pmap);
			return (NULL);
		} else {
			switch (pte & L2_TYPE_MASK) {
			case L2_TYPE_L:
				panic("extract and hold section mapping");
				break;
			default:
				pa = (pte & L2_S_FRAME) | (va & L2_S_OFFSET);
				break;
			}
			if (vm_page_pa_tryrelock(pmap, pa & PG_FRAME, &paddr))
				goto retry;
			m = PHYS_TO_VM_PAGE(pa);
			vm_page_hold(m);
		}

	}

	PMAP_UNLOCK(pmap);
	PA_UNLOCK_COND(paddr);
	return (m);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */

int
pmap_pinit(pmap_t pmap)
{
	PDEBUG(1, printf("pmap_pinit: pmap = %08x\n", (uint32_t) pmap));

	pmap_alloc_l1(pmap);
	bzero(pmap->pm_l2, sizeof(pmap->pm_l2));

	CPU_ZERO(&pmap->pm_active);

	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_stats.resident_count = 1;
	if (vector_page < KERNBASE) {
		pmap_enter(pmap, vector_page,
		    PHYS_TO_VM_PAGE(systempage.pv_pa), VM_PROT_READ,
		    PMAP_ENTER_WIRED, 0);
	}
	return (1);
}


/***************************************************
 * Superpage management routines.
 ***************************************************/

static PMAP_INLINE struct pv_entry *
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);

	pv = pmap_find_pv(pvh, pmap, va);
	if (pv != NULL)
		TAILQ_REMOVE(&pvh->pv_list, pv, pv_list);

	return (pv);
}

static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found"));
	pmap_free_pv_entry(pmap, pv);
}

static boolean_t
pmap_pv_insert_section(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = pmap_get_pv_entry(pmap, TRUE)) != NULL) {
		pv->pv_va = va;
		pvh = pa_to_pvh(pa);
		TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_list);
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * Create the pv entries for each of the pages within a superpage.
 */
static void
pmap_pv_demote_section(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pve, pv;
	vm_offset_t va_last;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT((pa & L1_S_OFFSET) == 0,
	    ("pmap_pv_demote_section: pa is not 1mpage aligned"));

	/*
	 * Transfer the 1mpage's pv entry for this mapping to the first
	 * page's pv list.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_1mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_section: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_HEAD(&m->md.pv_list, pv, pv_list);
	/* Instantiate the remaining pv entries. */
	va_last = L2_NEXT_BUCKET(va) - PAGE_SIZE;
	do {
		m++;
		KASSERT((m->oflags & VPO_UNMANAGED) == 0,
		    ("pmap_pv_demote_section: page %p is not managed", m));
		va += PAGE_SIZE;
		pve = pmap_get_pv_entry(pmap, FALSE);
		pmap_enter_pv(m, pve, pmap, va, pv->pv_flags);
	} while (va < va_last);
}

static void
pmap_pv_promote_section(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT((pa & L1_S_OFFSET) == 0,
	    ("pmap_pv_promote_section: pa is not 1mpage aligned"));

	/*
	 * Transfer the first page's pv entry for this mapping to the
	 * 1mpage's pv list.  Aside from avoiding the cost of a call
	 * to get_pv_entry(), a transfer avoids the possibility that
	 * get_pv_entry() calls pmap_pv_reclaim() and that pmap_pv_reclaim()
	 * removes one of the mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = trunc_1mpage(va);
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_promote_section: pv not found"));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_list);
	/* Free the remaining pv entries in the newly mapped section pages */
	va_last = L2_NEXT_BUCKET(va) - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		/*
		 * Don't care the flags, first pv contains sufficient
		 * information for all of the pages so nothing is really lost.
		 */
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
}

/*
 * Tries to create a 1MB page mapping.  Returns TRUE if successful and
 * FALSE otherwise.  Fails if (1) page is unmanageg, kernel pmap or vectors
 * page, (2) a mapping already exists at the specified virtual address, or
 * (3) a pv entry cannot be allocated without reclaiming another pv entry. 
 */
static boolean_t
pmap_enter_section(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	pd_entry_t *pl1pd;
	vm_offset_t pa;
	struct l2_bucket *l2b;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_ASSERT_LOCKED(pmap);

	/* Skip kernel, vectors page and unmanaged mappings */
	if ((pmap == pmap_kernel()) || (L1_IDX(va) == L1_IDX(vector_page)) ||
	    ((m->oflags & VPO_UNMANAGED) != 0)) {
		CTR2(KTR_PMAP, "pmap_enter_section: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	/*
	 * Check whether this is a valid section superpage entry or
	 * there is a l2_bucket associated with that L1 page directory.
	 */
	va = trunc_1mpage(va);
	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
	l2b = pmap_get_l2_bucket(pmap, va);
	if ((*pl1pd & L1_S_PROTO) || (l2b != NULL)) {
		CTR2(KTR_PMAP, "pmap_enter_section: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	pa = VM_PAGE_TO_PHYS(m); 
	/*
	 * Abort this mapping if its PV entry could not be created.
	 */
	if (!pmap_pv_insert_section(pmap, va, VM_PAGE_TO_PHYS(m))) {
		CTR2(KTR_PMAP, "pmap_enter_section: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	/*
	 * Increment counters.
	 */
	pmap->pm_stats.resident_count += L2_PTE_NUM_TOTAL;
	/*
	 * Despite permissions, mark the superpage read-only.
	 */
	prot &= ~VM_PROT_WRITE;
	/*
	 * Map the superpage.
	 */
	pmap_map_section(pmap, va, pa, prot, FALSE);

	pmap_section_mappings++;
	CTR2(KTR_PMAP, "pmap_enter_section: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

/*
 * pmap_remove_section: do the things to unmap a superpage in a process
 */
static void
pmap_remove_section(pmap_t pmap, vm_offset_t sva)
{
	struct md_page *pvh;
	struct l2_bucket *l2b;
	pd_entry_t *pl1pd, l1pd;
	vm_offset_t eva, va;
	vm_page_t m;

	PMAP_ASSERT_LOCKED(pmap);
	if ((pmap == pmap_kernel()) || (L1_IDX(sva) == L1_IDX(vector_page)))
		return;

	KASSERT((sva & L1_S_OFFSET) == 0,
	    ("pmap_remove_section: sva is not 1mpage aligned"));

	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(sva)];
	l1pd = *pl1pd;

	m = PHYS_TO_VM_PAGE(l1pd & L1_S_FRAME);
	KASSERT((m != NULL && ((m->oflags & VPO_UNMANAGED) == 0)),
	    ("pmap_remove_section: no corresponding vm_page or "
	    "page unmanaged"));

	pmap->pm_stats.resident_count -= L2_PTE_NUM_TOTAL;
	pvh = pa_to_pvh(l1pd & L1_S_FRAME);
	pmap_pvh_free(pvh, pmap, sva);
	eva = L2_NEXT_BUCKET(sva);
	for (va = sva, m = PHYS_TO_VM_PAGE(l1pd & L1_S_FRAME);
	    va < eva; va += PAGE_SIZE, m++) {
		/*
		 * Mark base pages referenced but skip marking them dirty.
		 * If the superpage is writeable, hence all base pages were
		 * already marked as dirty in pmap_fault_fixup() before
		 * promotion. Reference bit however, might not have been set
		 * for each base page when the superpage was created at once,
		 * not as a result of promotion.
		 */
		if (L1_S_REFERENCED(l1pd))
			vm_page_aflag_set(m, PGA_REFERENCED);
		if (TAILQ_EMPTY(&m->md.pv_list) &&
		    TAILQ_EMPTY(&pvh->pv_list))
			vm_page_aflag_clear(m, PGA_WRITEABLE);
	}
	
	l2b = pmap_get_l2_bucket(pmap, sva);
	if (l2b != NULL) {
		KASSERT(l2b->l2b_occupancy == L2_PTE_NUM_TOTAL,
		    ("pmap_remove_section: l2_bucket occupancy error"));
		pmap_free_l2_bucket(pmap, l2b, L2_PTE_NUM_TOTAL);
	}
	/* Now invalidate L1 slot */
	*pl1pd = 0;
	PTE_SYNC(pl1pd);
	if (L1_S_EXECUTABLE(l1pd))
		cpu_tlb_flushID_SE(sva);
	else
		cpu_tlb_flushD_SE(sva);
	cpu_cpwait();
}

/*
 * Tries to promote the 256, contiguous 4KB page mappings that are
 * within a single l2_bucket to a single 1MB section mapping.
 * For promotion to occur, two conditions must be met: (1) the 4KB page
 * mappings must map aligned, contiguous physical memory and (2) the 4KB page
 * mappings must have identical characteristics.
 */
static void
pmap_promote_section(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *firstptep, firstpte, oldpte, pa, *pte;
	vm_page_t m, oldm;
	vm_offset_t first_va, old_va;
	struct l2_bucket *l2b = NULL;
	vm_prot_t prot;
	struct pv_entry *pve, *first_pve;

	PMAP_ASSERT_LOCKED(pmap);

	prot = VM_PROT_ALL;
	/*
	 * Skip promoting kernel pages. This is justified by following:
	 * 1. Kernel is already mapped using section mappings in each pmap
	 * 2. Managed mappings within the kernel are not to be promoted anyway
	 */
	if (pmap == pmap_kernel()) {
		pmap_section_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_section: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	/* Do not attemp to promote vectors pages */
	if (L1_IDX(va) == L1_IDX(vector_page)) {
		pmap_section_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_section: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	/*
	 * Examine the first PTE in the specified l2_bucket. Abort if this PTE
	 * is either invalid, unused, or does not map the first 4KB physical
	 * page within 1MB page.
	 */
	first_va = trunc_1mpage(va);
	l2b = pmap_get_l2_bucket(pmap, first_va);
	KASSERT(l2b != NULL, ("pmap_promote_section: trying to promote "
	    "not existing l2 bucket"));
	firstptep = &l2b->l2b_kva[0];

	firstpte = *firstptep;
	if ((l2pte_pa(firstpte) & L1_S_OFFSET) != 0) {
		pmap_section_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_section: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}

	if ((firstpte & (L2_S_PROTO | L2_S_REF)) != (L2_S_PROTO | L2_S_REF)) {
		pmap_section_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_section: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	/*
	 * ARM uses pv_entry to mark particular mapping WIRED so don't promote
	 * unmanaged pages since it is impossible to determine, whether the
	 * page is wired or not if there is no corresponding pv_entry.
	 */
	m = PHYS_TO_VM_PAGE(l2pte_pa(firstpte));
	if (m && ((m->oflags & VPO_UNMANAGED) != 0)) {
		pmap_section_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_section: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	first_pve = pmap_find_pv(&m->md, pmap, first_va);
	/*
	 * PTE is modified only on write due to modified bit
	 * emulation. If the entry is referenced and writable
	 * then it is modified and we don't clear write enable.
	 * Otherwise, writing is disabled in PTE anyway and
	 * we just configure protections for the section mapping
	 * that is going to be created.
	 */
	if ((first_pve->pv_flags & PVF_WRITE) != 0) {
		if (!L2_S_WRITABLE(firstpte)) {
			first_pve->pv_flags &= ~PVF_WRITE;
			prot &= ~VM_PROT_WRITE;
		}
	} else
		prot &= ~VM_PROT_WRITE;

	if (!L2_S_EXECUTABLE(firstpte))
		prot &= ~VM_PROT_EXECUTE;

	/* 
	 * Examine each of the other PTEs in the specified l2_bucket. 
	 * Abort if this PTE maps an unexpected 4KB physical page or
	 * does not have identical characteristics to the first PTE.
	 */
	pa = l2pte_pa(firstpte) + ((L2_PTE_NUM_TOTAL - 1) * PAGE_SIZE);
	old_va = L2_NEXT_BUCKET(first_va) - PAGE_SIZE;

	for (pte = (firstptep + L2_PTE_NUM_TOTAL - 1); pte > firstptep; pte--) {
		oldpte = *pte;
		if (l2pte_pa(oldpte) != pa) {
			pmap_section_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_section: failure for "
			    "va %#x in pmap %p", va, pmap);
			return;
		}
		if ((oldpte & L2_S_PROMOTE) != (firstpte & L2_S_PROMOTE)) {
			pmap_section_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_section: failure for "
			    "va %#x in pmap %p", va, pmap);
			return;
		}
		oldm = PHYS_TO_VM_PAGE(l2pte_pa(oldpte));
		if (oldm && ((oldm->oflags & VPO_UNMANAGED) != 0)) {
			pmap_section_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_section: failure for "
			    "va %#x in pmap %p", va, pmap);
			return;
		}

		pve = pmap_find_pv(&oldm->md, pmap, old_va);
		if (pve == NULL) {
			pmap_section_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_section: failure for "
			    "va %#x old_va  %x - no pve", va, old_va);
			return;
		}

		if (!L2_S_WRITABLE(oldpte) && (pve->pv_flags & PVF_WRITE))
			pve->pv_flags &= ~PVF_WRITE;
		if (pve->pv_flags != first_pve->pv_flags) {
			pmap_section_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_section: failure for "
			    "va %#x in pmap %p", va, pmap);
			return;
		}

		old_va -= PAGE_SIZE;
		pa -= PAGE_SIZE;
	}
	/*
	 * Promote the pv entries.
	 */
	pmap_pv_promote_section(pmap, first_va, l2pte_pa(firstpte));
	/*
	 * Map the superpage.
	 */
	pmap_map_section(pmap, first_va, l2pte_pa(firstpte), prot, TRUE);
	/*
	 * Invalidate all possible TLB mappings for small
	 * pages within the newly created superpage.
	 * Rely on the first PTE's attributes since they
	 * have to be consistent across all of the base pages
	 * within the superpage. If page is not executable it
	 * is at least referenced.
	 * The fastest way to do that is to invalidate whole
	 * TLB at once instead of executing 256 CP15 TLB
	 * invalidations by single entry. TLBs usually maintain
	 * several dozen entries so loss of unrelated entries is
	 * still a less agresive approach.
	 */
	if (L2_S_EXECUTABLE(firstpte))
		cpu_tlb_flushID();
	else
		cpu_tlb_flushD();
	cpu_cpwait();

	pmap_section_promotions++;
	CTR2(KTR_PMAP, "pmap_promote_section: success for va %#x"
	    " in pmap %p", first_va, pmap);
}

/*
 * Fills a l2_bucket with mappings to consecutive physical pages.
 */
static void
pmap_fill_l2b(struct l2_bucket *l2b, pt_entry_t newpte)
{
	pt_entry_t *ptep;
	int i;

	for (i = 0; i < L2_PTE_NUM_TOTAL; i++) {
		ptep = &l2b->l2b_kva[i];
		*ptep = newpte;
		PTE_SYNC(ptep);

		newpte += PAGE_SIZE;
	}

	l2b->l2b_occupancy = L2_PTE_NUM_TOTAL;
}

/*
 * Tries to demote a 1MB section mapping. If demotion fails, the
 * 1MB section mapping is invalidated.
 */
static boolean_t
pmap_demote_section(pmap_t pmap, vm_offset_t va)
{
	struct l2_bucket *l2b;
	struct pv_entry *l1pdpve;
	struct md_page *pvh;
	pd_entry_t *pl1pd, l1pd, newl1pd;
	pt_entry_t *firstptep, newpte;
	vm_offset_t pa;
	vm_page_t m;

	PMAP_ASSERT_LOCKED(pmap);
	/*
	 * According to assumptions described in pmap_promote_section,
	 * kernel is and always should be mapped using 1MB section mappings.
	 * What more, managed kernel pages were not to be promoted. 
	 */
	KASSERT(pmap != pmap_kernel() && L1_IDX(va) != L1_IDX(vector_page),
	    ("pmap_demote_section: forbidden section mapping"));

	va = trunc_1mpage(va);
	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
	l1pd = *pl1pd;
	KASSERT((l1pd & L1_TYPE_MASK) == L1_S_PROTO,
	    ("pmap_demote_section: not section or invalid section"));
	
	pa = l1pd & L1_S_FRAME;
	m = PHYS_TO_VM_PAGE(pa);
	KASSERT((m != NULL && (m->oflags & VPO_UNMANAGED) == 0),
	    ("pmap_demote_section: no vm_page for selected superpage or"
	     "unmanaged"));

	pvh = pa_to_pvh(pa);
	l1pdpve = pmap_find_pv(pvh, pmap, va);
	KASSERT(l1pdpve != NULL, ("pmap_demote_section: no pv entry for "
	    "managed page"));

	l2b = pmap_get_l2_bucket(pmap, va);
	if (l2b == NULL) {
		KASSERT((l1pdpve->pv_flags & PVF_WIRED) == 0,
		    ("pmap_demote_section: No l2_bucket for wired mapping"));
		/*
		 * Invalidate the 1MB section mapping and return
		 * "failure" if the mapping was never accessed or the
		 * allocation of the new l2_bucket fails.
		 */
		if (!L1_S_REFERENCED(l1pd) ||
		    (l2b = pmap_alloc_l2_bucket(pmap, va)) == NULL) {
			/* Unmap and invalidate superpage. */
			pmap_remove_section(pmap, trunc_1mpage(va));
			CTR2(KTR_PMAP, "pmap_demote_section: failure for "
			    "va %#x in pmap %p", va, pmap);
			return (FALSE);
		}
	}

	/*
	 * Now we should have corresponding l2_bucket available.
	 * Let's process it to recreate 256 PTEs for each base page
	 * within superpage.
	 */
	newpte = pa | L1_S_DEMOTE(l1pd);
	if (m->md.pv_memattr != VM_MEMATTR_UNCACHEABLE)
		newpte |= pte_l2_s_cache_mode;

	/*
	 * If the l2_bucket is new, initialize it.
	 */
	if (l2b->l2b_occupancy == 0)
		pmap_fill_l2b(l2b, newpte);
	else {
		firstptep = &l2b->l2b_kva[0];
		KASSERT(l2pte_pa(*firstptep) == (pa),
		    ("pmap_demote_section: firstpte and newpte map different "
		     "physical addresses"));
		/*
		 * If the mapping has changed attributes, update the page table
		 * entries.
		 */ 
		if ((*firstptep & L2_S_PROMOTE) != (L1_S_DEMOTE(l1pd)))
			pmap_fill_l2b(l2b, newpte);
	}
	/* Demote PV entry */
	pmap_pv_demote_section(pmap, va, pa);

	/* Now fix-up L1 */
	newl1pd = l2b->l2b_phys | L1_C_DOM(pmap->pm_domain) | L1_C_PROTO;
	*pl1pd = newl1pd;
	PTE_SYNC(pl1pd);
	/* Invalidate old TLB mapping */
	if (L1_S_EXECUTABLE(l1pd))
		cpu_tlb_flushID_SE(va);
	else if (L1_S_REFERENCED(l1pd))
		cpu_tlb_flushD_SE(va);
	cpu_cpwait();

	pmap_section_demotions++;
	CTR2(KTR_PMAP, "pmap_demote_section: success for va %#x"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

/***************************************************
 * page management routines.
 ***************************************************/

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 */
static vm_page_t
pmap_pv_reclaim(pmap_t locked_pmap)
{
	struct pch newtail;
	struct pv_chunk *pc;
	struct l2_bucket *l2b = NULL;
	pmap_t pmap;
	pd_entry_t *pl1pd;
	pt_entry_t *ptep;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t free, m, m_pc;
	uint32_t inuse;
	int bit, field, freed, idx;

	PMAP_ASSERT_LOCKED(locked_pmap);
	pmap = NULL;
	free = m_pc = NULL;
	TAILQ_INIT(&newtail);
	while ((pc = TAILQ_FIRST(&pv_chunks)) != NULL && (pv_vafree == 0 ||
	    free == NULL)) {
		TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
		if (pmap != pc->pc_pmap) {
			if (pmap != NULL) {
				cpu_tlb_flushID();
				cpu_cpwait();
				if (pmap != locked_pmap)
					PMAP_UNLOCK(pmap);
			}
			pmap = pc->pc_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap)
				PMAP_LOCK(pmap);
			else if (pmap != locked_pmap && !PMAP_TRYLOCK(pmap)) {
				pmap = NULL;
				TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);
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
				bit = ffs(inuse) - 1;
				idx = field * sizeof(inuse) * NBBY + bit;
				pv = &pc->pc_pventry[idx];
				va = pv->pv_va;

				pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
				if ((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO)
					continue;
				if (pv->pv_flags & PVF_WIRED)
					continue;

				l2b = pmap_get_l2_bucket(pmap, va);
				KASSERT(l2b != NULL, ("No l2 bucket"));
				ptep = &l2b->l2b_kva[l2pte_index(va)];
				m = PHYS_TO_VM_PAGE(l2pte_pa(*ptep));
				KASSERT((vm_offset_t)m >= KERNBASE,
				    ("Trying to access non-existent page "
				     "va %x pte %x", va, *ptep));
				*ptep = 0;
				PTE_SYNC(ptep);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
				if (TAILQ_EMPTY(&m->md.pv_list))
					vm_page_aflag_clear(m, PGA_WRITEABLE);
				pc->pc_map[field] |= 1UL << bit;
				freed++;
			}
		}

		if (freed == 0) {
			TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);
			continue;
		}
		/* Every freed mapping is for a 4 KB page. */
		pmap->pm_stats.resident_count -= freed;
		PV_STAT(pv_entry_frees += freed);
		PV_STAT(pv_entry_spare += freed);
		pv_entry_count -= freed;
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		for (field = 0; field < _NPCM; field++)
			if (pc->pc_map[field] != pc_freemask[field]) {
				TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc,
				    pc_list);
				TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);

				/*
				 * One freed pv entry in locked_pmap is
				 * sufficient.
				 */
				if (pmap == locked_pmap)
					goto out;
				break;
			}
		if (field == _NPCM) {
			PV_STAT(pv_entry_spare -= _NPCPV);
			PV_STAT(pc_chunk_count--);
			PV_STAT(pc_chunk_frees++);
			/* Entire chunk is free; return it. */
			m_pc = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)pc));
			pmap_qremove((vm_offset_t)pc, 1);
			pmap_ptelist_free(&pv_vafree, (vm_offset_t)pc);
			break;
		}
	}
out:
	TAILQ_CONCAT(&pv_chunks, &newtail, pc_lru);
	if (pmap != NULL) {
		cpu_tlb_flushID();
		cpu_cpwait();
		if (pmap != locked_pmap)
			PMAP_UNLOCK(pmap);
	}
	return (m_pc);
}

/*
 * free the pv_entry back to the free list
 */
static void
pmap_free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int bit, field, idx;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_ASSERT_LOCKED(pmap);
	PV_STAT(pv_entry_frees++);
	PV_STAT(pv_entry_spare++);
	pv_entry_count--;
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / (sizeof(u_long) * NBBY);
	bit = idx % (sizeof(u_long) * NBBY);
	pc->pc_map[field] |= 1ul << bit;
	for (idx = 0; idx < _NPCM; idx++)
		if (pc->pc_map[idx] != pc_freemask[idx]) {
			/*
			 * 98% of the time, pc is already at the head of the
			 * list.  If it isn't already, move it to the head.
			 */
			if (__predict_false(TAILQ_FIRST(&pmap->pm_pvchunk) !=
			    pc)) {
				TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
				TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc,
				    pc_list);
			}
			return;
		}
	TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
	pmap_free_pv_chunk(pc);
}

static void
pmap_free_pv_chunk(struct pv_chunk *pc)
{
	vm_page_t m;

	TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
	PV_STAT(pv_entry_spare -= _NPCPV);
	PV_STAT(pc_chunk_count--);
	PV_STAT(pc_chunk_frees++);
	/* entire chunk is free, return it */
	m = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)pc));
	pmap_qremove((vm_offset_t)pc, 1);
	vm_page_unwire(m, PQ_INACTIVE);
	vm_page_free(m);
	pmap_ptelist_free(&pv_vafree, (vm_offset_t)pc);

}

static pv_entry_t
pmap_get_pv_entry(pmap_t pmap, boolean_t try)
{
	static const struct timeval printinterval = { 60, 0 };
	static struct timeval lastprint;
	struct pv_chunk *pc;
	pv_entry_t pv;
	vm_page_t m;
	int bit, field, idx;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_ASSERT_LOCKED(pmap);
	PV_STAT(pv_entry_allocs++);
	pv_entry_count++;

	if (pv_entry_count > pv_entry_high_water)
		if (ratecheck(&lastprint, &printinterval))
			printf("%s: Approaching the limit on PV entries.\n",
			    __func__);
retry:
	pc = TAILQ_FIRST(&pmap->pm_pvchunk);
	if (pc != NULL) {
		for (field = 0; field < _NPCM; field++) {
			if (pc->pc_map[field]) {
				bit = ffs(pc->pc_map[field]) - 1;
				break;
			}
		}
		if (field < _NPCM) {
			idx = field * sizeof(pc->pc_map[field]) * NBBY + bit;
			pv = &pc->pc_pventry[idx];
			pc->pc_map[field] &= ~(1ul << bit);
			/* If this was the last item, move it to tail */
			for (field = 0; field < _NPCM; field++)
				if (pc->pc_map[field] != 0) {
					PV_STAT(pv_entry_spare--);
					return (pv);	/* not full, return */
				}
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
			PV_STAT(pv_entry_spare--);
			return (pv);
		}
	}
	/*
	 * Access to the ptelist "pv_vafree" is synchronized by the pvh
	 * global lock.  If "pv_vafree" is currently non-empty, it will
	 * remain non-empty until pmap_ptelist_alloc() completes.
	 */
	if (pv_vafree == 0 || (m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
		if (try) {
			pv_entry_count--;
			PV_STAT(pc_chunk_tryfail++);
			return (NULL);
		}
		m = pmap_pv_reclaim(pmap);
		if (m == NULL)
			goto retry;
	}
	PV_STAT(pc_chunk_count++);
	PV_STAT(pc_chunk_allocs++);
	pc = (struct pv_chunk *)pmap_ptelist_alloc(&pv_vafree);
	pmap_qenter((vm_offset_t)pc, &m, 1);
	pc->pc_pmap = pmap;
	pc->pc_map[0] = pc_freemask[0] & ~1ul;	/* preallocated bit 0 */
	for (field = 1; field < _NPCM; field++)
		pc->pc_map[field] = pc_freemask[field];
	TAILQ_INSERT_TAIL(&pv_chunks, pc, pc_lru);
	pv = &pc->pc_pventry[0];
	TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
	PV_STAT(pv_entry_spare += _NPCPV - 1);
	return (pv);
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
#define	PMAP_REMOVE_CLEAN_LIST_SIZE	3
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct l2_bucket *l2b;
	vm_offset_t next_bucket;
	pd_entry_t l1pd;
	pt_entry_t *ptep;
	u_int total;
	u_int mappings, is_exec, is_refd;
	int flushall = 0;


	/*
	 * we lock in the pmap => pv_head direction
	 */

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	total = 0;
	while (sva < eva) {
		next_bucket = L2_NEXT_BUCKET(sva);

		/*
		 * Check for large page.
		 */
		l1pd = pmap->pm_l1->l1_kva[L1_IDX(sva)];
		if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
			KASSERT((l1pd & L1_S_DOM_MASK) !=
			    L1_S_DOM(PMAP_DOMAIN_KERNEL), ("pmap_remove: "
			    "Trying to remove kernel section mapping"));
			/*
			 * Are we removing the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L1_S_SIZE == next_bucket &&
			    eva >= next_bucket) {
				pmap_remove_section(pmap, sva);
				sva = next_bucket;
				continue;
			} else if (!pmap_demote_section(pmap, sva)) {
				/* The large page mapping was destroyed. */
				sva = next_bucket;
				continue;
			}
		}
		/*
		 * Do one L2 bucket's worth at a time.
		 */
		if (next_bucket > eva)
			next_bucket = eva;

		l2b = pmap_get_l2_bucket(pmap, sva);
		if (l2b == NULL) {
			sva = next_bucket;
			continue;
		}

		ptep = &l2b->l2b_kva[l2pte_index(sva)];
		mappings = 0;

		while (sva < next_bucket) {
			struct vm_page *m;
			pt_entry_t pte;
			vm_paddr_t pa;

			pte = *ptep;

			if (pte == 0) {
				/*
				 * Nothing here, move along
				 */
				sva += PAGE_SIZE;
				ptep++;
				continue;
			}

			pmap->pm_stats.resident_count--;
			pa = l2pte_pa(pte);
			is_exec = 0;
			is_refd = 1;

			/*
			 * Update flags. In a number of circumstances,
			 * we could cluster a lot of these and do a
			 * number of sequential pages in one go.
			 */
			if ((m = PHYS_TO_VM_PAGE(pa)) != NULL) {
				struct pv_entry *pve;

				pve = pmap_remove_pv(m, pmap, sva);
				if (pve) {
					is_exec = PTE_BEEN_EXECD(pte);
					is_refd = PTE_BEEN_REFD(pte);
					pmap_free_pv_entry(pmap, pve);
				}
			}

			*ptep = 0;
			PTE_SYNC(ptep);
			if (pmap_is_current(pmap)) {
				total++;
				if (total < PMAP_REMOVE_CLEAN_LIST_SIZE) {
					if (is_exec)
						cpu_tlb_flushID_SE(sva);
					else if (is_refd)
						cpu_tlb_flushD_SE(sva);
				} else if (total == PMAP_REMOVE_CLEAN_LIST_SIZE)
					flushall = 1;
			}

			sva += PAGE_SIZE;
			ptep++;
			mappings++;
		}

		pmap_free_l2_bucket(pmap, l2b, mappings);
	}

	rw_wunlock(&pvh_global_lock);
	if (flushall)
		cpu_tlb_flushID();
	cpu_cpwait();

	PMAP_UNLOCK(pmap);
}

/*
 * pmap_zero_page()
 *
 * Zero a given physical page by mapping it at a page hook point.
 * In doing the zero page op, the page we zero is mapped cachable, as with
 * StrongARM accesses to non-cached pages are non-burst making writing
 * _any_ bulk data very slow.
 */
static void
pmap_zero_page_gen(vm_page_t m, int off, int size)
{
	struct czpages *czp;

	KASSERT(TAILQ_EMPTY(&m->md.pv_list), 
	    ("pmap_zero_page_gen: page has mappings"));

	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	sched_pin();
	czp = &cpu_czpages[PCPU_GET(cpuid)];
	mtx_lock(&czp->lock);
	
	/*
	 * Hook in the page, zero it.
	 */
	*czp->dstptep = L2_S_PROTO | phys | pte_l2_s_cache_mode | L2_S_REF;
	pmap_set_prot(czp->dstptep, VM_PROT_WRITE, 0);
	PTE_SYNC(czp->dstptep);
	cpu_tlb_flushD_SE(czp->dstva);
	cpu_cpwait();

	if (off || size != PAGE_SIZE)
		bzero((void *)(czp->dstva + off), size);
	else
		bzero_page(czp->dstva);

	/*
	 * Although aliasing is not possible, if we use temporary mappings with
	 * memory that will be mapped later as non-cached or with write-through
	 * caches, we might end up overwriting it when calling wbinv_all.  So
	 * make sure caches are clean after the operation.
	 */
	cpu_idcache_wbinv_range(czp->dstva, size);
	pmap_l2cache_wbinv_range(czp->dstva, phys, size);

	mtx_unlock(&czp->lock);
	sched_unpin();
}

/*
 *	pmap_zero_page zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.
 */
void
pmap_zero_page(vm_page_t m)
{
	pmap_zero_page_gen(m, 0, PAGE_SIZE);
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

	pmap_zero_page_gen(m, off, size);
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

	pmap_zero_page(m);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */

/*
 * pmap_copy_page()
 *
 * Copy one physical page into another, by mapping the pages into
 * hook points. The same comment regarding cachability as in
 * pmap_zero_page also applies here.
 */
void
pmap_copy_page_generic(vm_paddr_t src, vm_paddr_t dst)
{
	struct czpages *czp;

	sched_pin();
	czp = &cpu_czpages[PCPU_GET(cpuid)];
	mtx_lock(&czp->lock);
	
	/*
	 * Map the pages into the page hook points, copy them, and purge the
	 * cache for the appropriate page.
	 */
	*czp->srcptep = L2_S_PROTO | src | pte_l2_s_cache_mode | L2_S_REF;
	pmap_set_prot(czp->srcptep, VM_PROT_READ, 0);
	PTE_SYNC(czp->srcptep);
	cpu_tlb_flushD_SE(czp->srcva);
	*czp->dstptep = L2_S_PROTO | dst | pte_l2_s_cache_mode | L2_S_REF;
	pmap_set_prot(czp->dstptep, VM_PROT_READ | VM_PROT_WRITE, 0);
	PTE_SYNC(czp->dstptep);
	cpu_tlb_flushD_SE(czp->dstva);
	cpu_cpwait();

	bcopy_page(czp->srcva, czp->dstva);

	/*
	 * Although aliasing is not possible, if we use temporary mappings with
	 * memory that will be mapped later as non-cached or with write-through
	 * caches, we might end up overwriting it when calling wbinv_all.  So
	 * make sure caches are clean after the operation.
	 */
	cpu_idcache_wbinv_range(czp->dstva, PAGE_SIZE);
	pmap_l2cache_wbinv_range(czp->dstva, dst, PAGE_SIZE);

	mtx_unlock(&czp->lock);
	sched_unpin();
}

int unmapped_buf_allowed = 1;

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
	vm_page_t a_pg, b_pg;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;
	struct czpages *czp;

	sched_pin();
	czp = &cpu_czpages[PCPU_GET(cpuid)];
	mtx_lock(&czp->lock);

	while (xfersize > 0) {
		a_pg = ma[a_offset >> PAGE_SHIFT];
		a_pg_offset = a_offset & PAGE_MASK;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		b_pg = mb[b_offset >> PAGE_SHIFT];
		b_pg_offset = b_offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		*czp->srcptep = L2_S_PROTO | VM_PAGE_TO_PHYS(a_pg) |
		    pte_l2_s_cache_mode | L2_S_REF;
		pmap_set_prot(czp->srcptep, VM_PROT_READ, 0);
		PTE_SYNC(czp->srcptep);
		cpu_tlb_flushD_SE(czp->srcva);
		*czp->dstptep = L2_S_PROTO | VM_PAGE_TO_PHYS(b_pg) |
		    pte_l2_s_cache_mode | L2_S_REF;
		pmap_set_prot(czp->dstptep, VM_PROT_READ | VM_PROT_WRITE, 0);
		PTE_SYNC(czp->dstptep);
		cpu_tlb_flushD_SE(czp->dstva);
		cpu_cpwait();
		bcopy((char *)czp->srcva + a_pg_offset, (char *)czp->dstva + b_pg_offset,
		    cnt);
		cpu_idcache_wbinv_range(czp->dstva + b_pg_offset, cnt);
		pmap_l2cache_wbinv_range(czp->dstva + b_pg_offset,
		    VM_PAGE_TO_PHYS(b_pg) + b_pg_offset, cnt);
		xfersize -= cnt;
		a_offset += cnt;
		b_offset += cnt;
	}

	mtx_unlock(&czp->lock);
	sched_unpin();
}

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{

	if (_arm_memcpy && PAGE_SIZE >= _min_memcpy_size &&
	    _arm_memcpy((void *)VM_PAGE_TO_PHYS(dst),
	    (void *)VM_PAGE_TO_PHYS(src), PAGE_SIZE, IS_PHYSICAL) == 0)
		return;

	pmap_copy_page_generic(VM_PAGE_TO_PHYS(src), VM_PAGE_TO_PHYS(dst));
}

/*
 * this routine returns true if a physical page resides
 * in the given pmap.
 */
boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv;
	int loops = 0;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	rv = FALSE;
	rw_wlock(&pvh_global_lock);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
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
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_list) {
			if (PV_PMAP(pv) == pmap) {
				rv = TRUE;
				break;
			}
			loops++;
			if (loops >= 16)
				break;
		}
	}
	rw_wunlock(&pvh_global_lock);
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
	int count;

	count = 0;
	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (count);
	rw_wlock(&pvh_global_lock);
	count = pmap_pvh_wired_mappings(&m->md, count);
	if ((m->flags & PG_FICTITIOUS) == 0) {
	    count = pmap_pvh_wired_mappings(pa_to_pvh(VM_PAGE_TO_PHYS(m)),
	        count);
	}
	rw_wunlock(&pvh_global_lock);
	return (count);
}

/*
 *	pmap_pvh_wired_mappings:
 *
 *	Return the updated number "count" of managed mappings that are wired.
 */
static int
pmap_pvh_wired_mappings(struct md_page *pvh, int count)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_list) {
		if ((pv->pv_flags & PVF_WIRED) != 0)
			count++;
	}
	return (count);
}

/*
 * Returns TRUE if any of the given mappings were referenced and FALSE
 * otherwise.  Both page and section mappings are supported.
 */
static boolean_t
pmap_is_referenced_pvh(struct md_page *pvh)
{
	struct l2_bucket *l2b;
	pv_entry_t pv;
	pd_entry_t *pl1pd;
	pt_entry_t *ptep;
	pmap_t pmap;
	boolean_t rv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	rv = FALSE;
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_list) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(pv->pv_va)];
		if ((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO)
			rv = L1_S_REFERENCED(*pl1pd);
		else {
			l2b = pmap_get_l2_bucket(pmap, pv->pv_va);
			ptep = &l2b->l2b_kva[l2pte_index(pv->pv_va)];
			rv = L2_S_REFERENCED(*ptep);
		}
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}
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
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	rw_wlock(&pvh_global_lock);
	rv = pmap_is_referenced_pvh(&m->md) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_is_referenced_pvh(pa_to_pvh(VM_PAGE_TO_PHYS(m))));
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 *	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 */
int
pmap_ts_referenced(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	return (pmap_clearbit(m, PVF_REF));
}

/*
 * Returns TRUE if any of the given mappings were used to modify
 * physical memory. Otherwise, returns FALSE. Both page and 1MB section
 * mappings are supported.
 */
static boolean_t
pmap_is_modified_pvh(struct md_page *pvh)
{
	pd_entry_t *pl1pd;
	struct l2_bucket *l2b;
	pv_entry_t pv;
	pt_entry_t *ptep;
	pmap_t pmap;
	boolean_t rv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	rv = FALSE;

	TAILQ_FOREACH(pv, &pvh->pv_list, pv_list) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(pv->pv_va)];
		if ((*pl1pd & L1_TYPE_MASK) == L1_S_PROTO)
			rv = L1_S_WRITABLE(*pl1pd);
		else {
			l2b = pmap_get_l2_bucket(pmap, pv->pv_va);
			ptep = &l2b->l2b_kva[l2pte_index(pv->pv_va)];
			rv = L2_S_WRITABLE(*ptep);
		}
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}

	return (rv);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));
	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no PTEs can have APX cleared.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return (FALSE);
	rw_wlock(&pvh_global_lock);
	rv = pmap_is_modified_pvh(&m->md) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_is_modified_pvh(pa_to_pvh(VM_PAGE_TO_PHYS(m))));
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	struct l2_bucket *l2b;
	struct pv_entry *pve;
	pd_entry_t l1pd;
	pt_entry_t *ptep, opte, pte;
	vm_offset_t next_bucket;
	vm_page_t m;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = next_bucket) {
		next_bucket = L2_NEXT_BUCKET(sva);
		if (next_bucket < sva)
			next_bucket = eva;
		l1pd = pmap->pm_l1->l1_kva[L1_IDX(sva)];
		if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
			if (pmap == pmap_kernel())
				continue;
			if (!pmap_demote_section(pmap, sva)) {
				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}
			/*
			 * Unless the page mappings are wired, remove the
			 * mapping to a single page so that a subsequent
			 * access may repromote. Since the underlying
			 * l2_bucket is fully populated, this removal
			 * never frees an entire l2_bucket.
			 */
			l2b = pmap_get_l2_bucket(pmap, sva);
			KASSERT(l2b != NULL,
			    ("pmap_advise: no l2 bucket for "
			     "va 0x%#x, pmap 0x%p", sva, pmap));
			ptep = &l2b->l2b_kva[l2pte_index(sva)];
			opte = *ptep;
			m = PHYS_TO_VM_PAGE(l2pte_pa(*ptep));
			KASSERT(m != NULL,
			    ("pmap_advise: no vm_page for demoted superpage"));
			pve = pmap_find_pv(&m->md, pmap, sva);
			KASSERT(pve != NULL,
			    ("pmap_advise: no PV entry for managed mapping"));
			if ((pve->pv_flags & PVF_WIRED) == 0) {
				pmap_free_l2_bucket(pmap, l2b, 1);
				pve = pmap_remove_pv(m, pmap, sva);
				pmap_free_pv_entry(pmap, pve);
				*ptep = 0;
				PTE_SYNC(ptep);
				if (pmap_is_current(pmap)) {
					if (PTE_BEEN_EXECD(opte))
						cpu_tlb_flushID_SE(sva);
					else if (PTE_BEEN_REFD(opte))
						cpu_tlb_flushD_SE(sva);
				}
			}
		}
		if (next_bucket > eva)
			next_bucket = eva;
		l2b = pmap_get_l2_bucket(pmap, sva);
		if (l2b == NULL)
			continue;
		for (ptep = &l2b->l2b_kva[l2pte_index(sva)];
		    sva != next_bucket; ptep++, sva += PAGE_SIZE) {
			opte = pte = *ptep;
			if ((opte & L2_S_PROTO) == 0)
				continue;
			m = PHYS_TO_VM_PAGE(l2pte_pa(opte));
			if (m == NULL || (m->oflags & VPO_UNMANAGED) != 0)
				continue;
			else if (L2_S_WRITABLE(opte)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Don't need to mark the page
					 * dirty as it was already marked as
					 * such in pmap_fault_fixup() or
					 * pmap_enter_locked().
					 * Just clear the state.
					 */
				} else
					pte |= L2_APX;

				pte &= ~L2_S_REF;
				*ptep = pte;
				PTE_SYNC(ptep);
			} else if (L2_S_REFERENCED(opte)) {
				pte &= ~L2_S_REF;
				*ptep = pte;
				PTE_SYNC(ptep);
			} else
				continue;
			if (pmap_is_current(pmap)) {
				if (PTE_BEEN_EXECD(opte))
					cpu_tlb_flushID_SE(sva);
				else if (PTE_BEEN_REFD(opte))
					cpu_tlb_flushD_SE(sva);
			}
		}
	}
	cpu_cpwait();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
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
	 * If the page is not PGA_WRITEABLE, then no mappings can be modified.
	 * If the object containing the page is locked and the page is not
	 * exclusive busied, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;
	if (pmap_is_modified(m))
		pmap_clearbit(m, PVF_MOD);
}


/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * set by another thread while the object is locked.  Thus,
	 * if PGA_WRITEABLE is clear, no page table entries need updating.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (vm_page_xbusied(m) || (m->aflags & PGA_WRITEABLE) != 0)
		pmap_clearbit(m, PVF_WRITE);
}


/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	struct l2_bucket *l2b;
	pd_entry_t *pl1pd, l1pd;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	vm_page_t m;
	int val;
	boolean_t managed;

	PMAP_LOCK(pmap);
retry:
	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(addr)];
	l1pd = *pl1pd;
	if ((l1pd & L1_TYPE_MASK) == L1_S_PROTO) {
		pa = (l1pd & L1_S_FRAME);
		val = MINCORE_SUPER | MINCORE_INCORE;
		if (L1_S_WRITABLE(l1pd))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		managed = FALSE;
		m = PHYS_TO_VM_PAGE(pa);
		if (m != NULL && (m->oflags & VPO_UNMANAGED) == 0)
			managed = TRUE;
		if (managed) {
			if (L1_S_REFERENCED(l1pd))
				val |= MINCORE_REFERENCED |
				    MINCORE_REFERENCED_OTHER;
		}
	} else {
		l2b = pmap_get_l2_bucket(pmap, addr);
		if (l2b == NULL) {
			val = 0;
			goto out;
		}
		ptep = &l2b->l2b_kva[l2pte_index(addr)];
		pte = *ptep;
		if (!l2pte_valid(pte)) {
			val = 0;
			goto out;
		}
		val = MINCORE_INCORE;
		if (L2_S_WRITABLE(pte))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		managed = FALSE;
		pa = l2pte_pa(pte);
		m = PHYS_TO_VM_PAGE(pa);
		if (m != NULL && (m->oflags & VPO_UNMANAGED) == 0)
			managed = TRUE;
		if (managed) {
			if (L2_S_REFERENCED(pte))
				val |= MINCORE_REFERENCED |
				    MINCORE_REFERENCED_OTHER;
		}
	}
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) && managed) {
		/* Ensure that "PHYS_TO_VM_PAGE(pa)->object" doesn't change. */
		if (vm_page_pa_tryrelock(pmap, pa, locked_pa))
			goto retry;
	} else
out:
		PA_UNLOCK_COND(*locked_pa);
	PMAP_UNLOCK(pmap);
	return (val);
}

void
pmap_sync_icache(pmap_t pmap, vm_offset_t va, vm_size_t sz)
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
}

/*
 * pmap_map_section:
 *
 *	Create a single section mapping.
 */
void
pmap_map_section(pmap_t pmap, vm_offset_t va, vm_offset_t pa, vm_prot_t prot,
    boolean_t ref)
{
	pd_entry_t *pl1pd, l1pd;
	pd_entry_t fl;

	KASSERT(((va | pa) & L1_S_OFFSET) == 0,
	    ("Not a valid section mapping"));

	fl = pte_l1_s_cache_mode;

	pl1pd = &pmap->pm_l1->l1_kva[L1_IDX(va)];
	l1pd = L1_S_PROTO | pa | L1_S_PROT(PTE_USER, prot) | fl |
	    L1_S_DOM(pmap->pm_domain);

	/* Mark page referenced if this section is a result of a promotion. */
	if (ref == TRUE)
		l1pd |= L1_S_REF;
#ifdef SMP
	l1pd |= L1_SHARED;
#endif
	*pl1pd = l1pd;
	PTE_SYNC(pl1pd);
}

/*
 * pmap_link_l2pt:
 *
 *	Link the L2 page table specified by l2pv.pv_pa into the L1
 *	page table at the slot for "va".
 */
void
pmap_link_l2pt(vm_offset_t l1pt, vm_offset_t va, struct pv_addr *l2pv)
{
	pd_entry_t *pde = (pd_entry_t *) l1pt, proto;
	u_int slot = va >> L1_S_SHIFT;

	proto = L1_S_DOM(PMAP_DOMAIN_KERNEL) | L1_C_PROTO;

#ifdef VERBOSE_INIT_ARM
	printf("pmap_link_l2pt: pa=0x%x va=0x%x\n", l2pv->pv_pa, l2pv->pv_va);
#endif

	pde[slot + 0] = proto | (l2pv->pv_pa + 0x000);
	PTE_SYNC(&pde[slot]);

	SLIST_INSERT_HEAD(&kernel_pt_list, l2pv, pv_list);

}

/*
 * pmap_map_entry
 *
 *	Create a single page mapping.
 */
void
pmap_map_entry(vm_offset_t l1pt, vm_offset_t va, vm_offset_t pa, int prot,
    int cache)
{
	pd_entry_t *pde = (pd_entry_t *) l1pt;
	pt_entry_t fl;
	pt_entry_t *ptep;

	KASSERT(((va | pa) & PAGE_MASK) == 0, ("ouin"));

	fl = l2s_mem_types[cache];

	if ((pde[va >> L1_S_SHIFT] & L1_TYPE_MASK) != L1_TYPE_C)
		panic("pmap_map_entry: no L2 table for VA 0x%08x", va);

	ptep = (pt_entry_t *)kernel_pt_lookup(pde[L1_IDX(va)] & L1_C_ADDR_MASK);

	if (ptep == NULL)
		panic("pmap_map_entry: can't find L2 table for VA 0x%08x", va);

	ptep[l2pte_index(va)] = L2_S_PROTO | pa | fl | L2_S_REF;
	pmap_set_prot(&ptep[l2pte_index(va)], prot, 0);
	PTE_SYNC(&ptep[l2pte_index(va)]);
}

/*
 * pmap_map_chunk:
 *
 *	Map a chunk of memory using the most efficient mappings
 *	possible (section. large page, small page) into the
 *	provided L1 and L2 tables at the specified virtual address.
 */
vm_size_t
pmap_map_chunk(vm_offset_t l1pt, vm_offset_t va, vm_offset_t pa,
    vm_size_t size, int prot, int type)
{
	pd_entry_t *pde = (pd_entry_t *) l1pt;
	pt_entry_t *ptep, f1, f2s, f2l;
	vm_size_t resid;
	int i;

	resid = (size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

	if (l1pt == 0)
		panic("pmap_map_chunk: no L1 table provided");

#ifdef VERBOSE_INIT_ARM
	printf("pmap_map_chunk: pa=0x%x va=0x%x size=0x%x resid=0x%x "
	    "prot=0x%x type=%d\n", pa, va, size, resid, prot, type);
#endif

	f1 = l1_mem_types[type];
	f2l = l2l_mem_types[type];
	f2s = l2s_mem_types[type];

	size = resid;

	while (resid > 0) {
		/* See if we can use a section mapping. */
		if (L1_S_MAPPABLE_P(va, pa, resid)) {
#ifdef VERBOSE_INIT_ARM
			printf("S");
#endif
			pde[va >> L1_S_SHIFT] = L1_S_PROTO | pa |
			    L1_S_PROT(PTE_KERNEL, prot | VM_PROT_EXECUTE) |
			    f1 | L1_S_DOM(PMAP_DOMAIN_KERNEL) | L1_S_REF;
			PTE_SYNC(&pde[va >> L1_S_SHIFT]);
			va += L1_S_SIZE;
			pa += L1_S_SIZE;
			resid -= L1_S_SIZE;
			continue;
		}

		/*
		 * Ok, we're going to use an L2 table.  Make sure
		 * one is actually in the corresponding L1 slot
		 * for the current VA.
		 */
		if ((pde[va >> L1_S_SHIFT] & L1_TYPE_MASK) != L1_TYPE_C)
			panic("pmap_map_chunk: no L2 table for VA 0x%08x", va);

		ptep = (pt_entry_t *) kernel_pt_lookup(
		    pde[L1_IDX(va)] & L1_C_ADDR_MASK);
		if (ptep == NULL)
			panic("pmap_map_chunk: can't find L2 table for VA"
			    "0x%08x", va);
		/* See if we can use a L2 large page mapping. */
		if (L2_L_MAPPABLE_P(va, pa, resid)) {
#ifdef VERBOSE_INIT_ARM
			printf("L");
#endif
			for (i = 0; i < 16; i++) {
				ptep[l2pte_index(va) + i] =
				    L2_L_PROTO | pa |
				    L2_L_PROT(PTE_KERNEL, prot) | f2l;
				PTE_SYNC(&ptep[l2pte_index(va) + i]);
			}
			va += L2_L_SIZE;
			pa += L2_L_SIZE;
			resid -= L2_L_SIZE;
			continue;
		}

		/* Use a small page mapping. */
#ifdef VERBOSE_INIT_ARM
		printf("P");
#endif
		ptep[l2pte_index(va)] = L2_S_PROTO | pa | f2s | L2_S_REF;
		pmap_set_prot(&ptep[l2pte_index(va)], prot, 0);
		PTE_SYNC(&ptep[l2pte_index(va)]);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
		resid -= PAGE_SIZE;
	}
#ifdef VERBOSE_INIT_ARM
	printf("\n");
#endif
	return (size);

}

int
pmap_dmap_iscurrent(pmap_t pmap)
{
	return(pmap_is_current(pmap));
}

void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	/* 
	 * Remember the memattr in a field that gets used to set the appropriate
	 * bits in the PTEs as mappings are established.
	 */
	m->md.pv_memattr = ma;

	/*
	 * It appears that this function can only be called before any mappings
	 * for the page are established on ARM.  If this ever changes, this code
	 * will need to walk the pv_list and make each of the existing mappings
	 * uncacheable, being careful to sync caches and PTEs (and maybe
	 * invalidate TLB?) for any current mapping it modifies.
	 */
	if (TAILQ_FIRST(&m->md.pv_list) != NULL)
		panic("Can't change memattr on page with existing mappings");
}
