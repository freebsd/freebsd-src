/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 1998,2000 Doug Rabson
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
 *	from:	i386 Id: pmap.c,v 1.193 1998/04/19 15:22:48 bde Exp
 *		with some ideas from NetBSD's alpha pmap
 * $FreeBSD$
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
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

/*
 * Following the Linux model, region IDs are allocated in groups of
 * eight so that a single region ID can be used for as many RRs as we
 * want by encoding the RR number into the low bits of the ID.
 *
 * We reserve region ID 0 for the kernel and allocate the remaining
 * IDs for user pmaps.
 *
 * Region 0..4
 *	User virtually mapped
 *
 * Region 5
 *	Kernel virtually mapped
 *
 * Region 6
 *	Kernel physically mapped uncacheable
 *
 * Region 7
 *	Kernel physically mapped cacheable
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <sys/user.h>

#include <machine/md_var.h>

MALLOC_DEFINE(M_PMAP, "PMAP", "PMAP Structures");

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#if defined(DIAGNOSTIC)
#define PMAP_DIAGNOSTIC
#endif

#define MINPV 2048

#if 0
#define PMAP_DIAGNOSTIC
#define PMAP_DEBUG
#endif

#if !defined(PMAP_DIAGNOSTIC)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

#if 0

static void
pmap_break(void)
{
}

/* #define PMAP_DEBUG_VA(va) if ((va) == 0x120058000) pmap_break(); else */

#endif

#ifndef PMAP_DEBUG_VA
#define PMAP_DEBUG_VA(va) do {} while(0)
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define pmap_pte_w(pte)		((pte)->pte_ig & PTE_IG_WIRED)
#define pmap_pte_managed(pte)	((pte)->pte_ig & PTE_IG_MANAGED)
#define pmap_pte_v(pte)		((pte)->pte_p)
#define pmap_pte_pa(pte)	(((pte)->pte_ppn) << 12)
#define pmap_pte_prot(pte)	(((pte)->pte_ar << 2) | (pte)->pte_pl)

#define pmap_pte_set_w(pte, v) ((v)?((pte)->pte_ig |= PTE_IG_WIRED) \
				:((pte)->pte_ig &= ~PTE_IG_WIRED))
#define pmap_pte_set_prot(pte, v) do {		\
    (pte)->pte_ar = v >> 2;			\
    (pte)->pte_pl = v & 3;			\
} while (0)

/*
 * Given a map and a machine independent protection code,
 * convert to an ia64 protection code.
 */
#define pte_prot(m, p)		(protection_codes[m == pmap_kernel() ? 0 : 1][p])
int	protection_codes[2][8];

/*
 * Return non-zero if this pmap is currently active
 */
#define pmap_isactive(pmap)	(pmap->pm_active)

/*
 * Statically allocated kernel pmap
 */
static struct pmap kernel_pmap_store;
pmap_t kernel_pmap;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */


vm_offset_t kernel_vm_end;

/*
 * Values for ptc.e. XXX values for SKI.
 */
static u_int64_t pmap_pte_e_base = 0x100000000;
static u_int64_t pmap_pte_e_count1 = 3;
static u_int64_t pmap_pte_e_count2 = 2;
static u_int64_t pmap_pte_e_stride1 = 0x2000;
static u_int64_t pmap_pte_e_stride2 = 0x100000000;

/*
 * Data for the RID allocator
 */
static int pmap_nextrid;
static int pmap_ridbits = 18;

/*
 * Data for the pv entry allocation mechanism
 */
static vm_zone_t pvzone;
static struct vm_zone pvzone_store;
static struct vm_object pvzone_obj;
static vm_zone_t pvbootzone;
static struct vm_zone pvbootzone_store;
static int pv_entry_count=0, pv_entry_max=0, pv_entry_high_water=0;
static int pmap_pagedaemon_waken = 0;
static struct pv_entry *pvinit;
static struct pv_entry *pvbootinit;

static PMAP_INLINE void	free_pv_entry __P((pv_entry_t pv));
static pv_entry_t get_pv_entry __P((void));
static void	ia64_protection_init __P((void));

static void	pmap_remove_all __P((vm_page_t m));
static void	pmap_enter_quick __P((pmap_t pmap, vm_offset_t va, vm_page_t m));

vm_offset_t
pmap_steal_memory(vm_size_t size)
{
	vm_size_t bank_size;
	vm_offset_t pa, va;

	size = round_page(size);

	bank_size = phys_avail[1] - phys_avail[0];
	while (size > bank_size) {
		int i;
		for (i = 0; phys_avail[i+2]; i+= 2) {
			phys_avail[i] = phys_avail[i+2];
			phys_avail[i+1] = phys_avail[i+3];
		}
		phys_avail[i] = 0;
		phys_avail[i+1] = 0;
		if (!phys_avail[0])
			panic("pmap_steal_memory: out of memory");
		bank_size = phys_avail[1] - phys_avail[0];
	}

	pa = phys_avail[0];
	phys_avail[0] += size;

	va = IA64_PHYS_TO_RR7(pa);
	bzero((caddr_t) va, size);
	return va;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap()
{
	int i;
	int boot_pvs;

	/*
	 * Setup RIDs. We use the bits above pmap_ridbits for a
	 * generation counter, saving generation zero for
	 * 'invalid'. RIDs 0..7 are reserved for the kernel.
	 */
	pmap_nextrid = (1 << pmap_ridbits) + 8;

	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i+2]; i+= 2) ;
	avail_end = phys_avail[i+1];

	virtual_avail = IA64_RR_BASE(5);
	virtual_end = IA64_RR_BASE(6)-1;

	/*
	 * Initialize protection array.
	 */
	ia64_protection_init();

	/*
	 * The kernel's pmap is statically allocated so we don't have to use
	 * pmap_create, which is unlikely to work correctly at this part of
	 * the boot sequence (XXX and which no longer exists).
	 */
	kernel_pmap = &kernel_pmap_store;
	kernel_pmap->pm_rid = 0;
	kernel_pmap->pm_count = 1;
	kernel_pmap->pm_active = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);

	/*
	 * Region 5 is mapped via the vhpt.
	 */
	ia64_set_rr(IA64_RR_BASE(5),
		    (5 << 8) | (PAGE_SHIFT << 2) | 1);

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest 
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (28 << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (28 << 2));
			 
	/*
	 * We need some PVs to cope with pmap_kenter() calls prior to
	 * pmap_init(). This is all a bit flaky and needs to be
	 * rethought, probably by avoiding the zone allocator
	 * entirely.
	 */
  	boot_pvs = 32768;
	pvbootzone = &pvbootzone_store;
	pvbootinit = (struct pv_entry *)
		pmap_steal_memory(boot_pvs * sizeof (struct pv_entry));
	zbootinit(pvbootzone, "PV ENTRY", sizeof (struct pv_entry),
		  pvbootinit, boot_pvs);

	/*
	 * Set up proc0's PCB.
	 */
#if 0
	proc0.p_addr->u_pcb.pcb_hw.apcb_asn = 0;
#endif
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 *	pmap_init has been enhanced to support in a fairly consistant
 *	way, discontiguous physical memory.
 */
void
pmap_init(phys_start, phys_end)
	vm_offset_t phys_start, phys_end;
{
	int i;
	int initial_pvs;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */

	for(i = 0; i < vm_page_array_size; i++) {
		vm_page_t m;

		m = &vm_page_array[i];
		TAILQ_INIT(&m->md.pv_list);
		m->md.pv_list_count = 0;
 	}

	/*
	 * init the pv free list
	 */
	initial_pvs = vm_page_array_size;
	if (initial_pvs < MINPV)
		initial_pvs = MINPV;
	pvzone = &pvzone_store;
	pvinit = (struct pv_entry *) kmem_alloc(kernel_map,
		initial_pvs * sizeof (struct pv_entry));
	zbootinit(pvzone, "PV ENTRY", sizeof (struct pv_entry), pvinit,
		  vm_page_array_size);

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

/*
 * Initialize the address space (zone) for the pv_entries.  Set a
 * high water mark so that the system can recover from excessive
 * numbers of pv entries.
 */
void
pmap_init2()
{
	pv_entry_max = PMAP_SHPGPERPROC * maxproc + vm_page_array_size;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	zinitna(pvzone, &pvzone_obj, NULL, 0, pv_entry_max, ZONE_INTERRUPT, 1);
}


/***************************************************
 * Manipulate TLBs for a pmap
 ***************************************************/

static void
pmap_invalidate_rid(pmap_t pmap)
{
	KASSERT(pmap != kernel_pmap,
		("changing kernel_pmap's RID"));
	KASSERT(pmap == PCPU_GET(current_pmap),
		("invalidating RID of non-current pmap"));
	pmap_remove_pages(pmap, IA64_RR_BASE(0), IA64_RR_BASE(5));
	pmap->pm_rid = 0;
}

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	KASSERT(pmap == PCPU_GET(current_pmap),
		("invalidating TLB for non-current pmap"));
	ia64_ptc_l(va, PAGE_SHIFT << 2);
}

static void
pmap_invalidate_all(pmap_t pmap)
{
	u_int64_t addr;
	int i, j;
	u_int32_t psr;

	KASSERT(pmap == PCPU_GET(current_pmap),
		("invalidating TLB for non-current pmap"));

	psr = save_intr();
	disable_intr();
	addr = pmap_pte_e_base;
	for (i = 0; i < pmap_pte_e_count1; i++) {
		for (j = 0; j < pmap_pte_e_count2; j++) {
			ia64_ptc_e(addr);
			addr += pmap_pte_e_stride2;
		}
		addr += pmap_pte_e_stride1;
	}
	restore_intr(psr);
}

static void
pmap_get_rid(pmap_t pmap)
{
	if ((pmap_nextrid & ((1 << pmap_ridbits) - 1)) == 0) {
		/*
		 * Start a new ASN generation.
		 *
		 * Invalidate all per-process mappings and I-cache
		 */
		pmap_nextrid += 8;

		/*
		 * Since we are about to start re-using ASNs, we must
		 * clear out the TLB.
		 * with the ASN.
		 */
#if 0
		IA64_TBIAP();
		ia64_pal_imb();	/* XXX overkill? */
#endif
	}
	pmap->pm_rid = pmap_nextrid;
	pmap_nextrid += 8;
}

/***************************************************
 * Low level helper routines.....
 ***************************************************/

/*
 * Install a pte into the VHPT
 */
static PMAP_INLINE void
pmap_install_pte(struct ia64_lpte *vhpte, struct ia64_lpte *pte)
{
	u_int64_t *vhp, *p;

	/* invalidate the pte */
	atomic_set_64(&vhpte->pte_tag, 1L << 63);
	ia64_mf();			/* make sure everyone sees */

	vhp = (u_int64_t *) vhpte;
	p = (u_int64_t *) pte;

	vhp[0] = p[0];
	vhp[1] = p[1];
	vhp[2] = p[2];			/* sets ti to one */

	ia64_mf();
}

/*
 * Compare essential parts of pte.
 */
static PMAP_INLINE int
pmap_equal_pte(struct ia64_lpte *pte1, struct ia64_lpte *pte2)
{
	return *(u_int64_t *) pte1 == *(u_int64_t *) pte2;
}

/*
 * this routine defines the region(s) of memory that should
 * not be tested for the modified bit.
 */
static PMAP_INLINE int
pmap_track_modified(vm_offset_t va)
{
	if ((va < clean_sva) || (va >= clean_eva)) 
		return 1;
	else
		return 0;
}

/*
 * Create the UPAGES for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
pmap_new_proc(struct proc *p)
{
	struct user *up;

	/*
	 * Use contigmalloc for user area so that we can use a region
	 * 7 address for it which makes it impossible to accidentally
	 * lose when recording a trapframe.
	 */
	up = contigmalloc(UPAGES * PAGE_SIZE, M_PMAP,
			  M_WAITOK,
			  0ul,
			  256*1024*1024 - 1,
			  PAGE_SIZE,
			  256*1024*1024);

	p->p_md.md_uservirt = up;
	p->p_addr = (struct user *)
		IA64_PHYS_TO_RR7(ia64_tpa((u_int64_t) up));
}

/*
 * Dispose the UPAGES for a process that has exited.
 * This routine directly impacts the exit perf of a process.
 */
void
pmap_dispose_proc(p)
	struct proc *p;
{
	contigfree(p->p_md.md_uservirt, UPAGES * PAGE_SIZE, M_PMAP);
	p->p_md.md_uservirt = 0;
	p->p_addr = 0;
}

/*
 * Allow the UPAGES for a process to be prejudicially paged out.
 */
void
pmap_swapout_proc(p)
	struct proc *p;
{
#if 0
	int i;
	vm_object_t upobj;
	vm_page_t m;

	/*
	 * Make sure we aren't fpcurproc.
	 */
	ia64_fpstate_save(p, 1);

	upobj = p->p_upages_obj;
	/*
	 * let the upages be paged
	 */
	for(i=0;i<UPAGES;i++) {
		if ((m = vm_page_lookup(upobj, i)) == NULL)
			panic("pmap_swapout_proc: upage already missing???");
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		pmap_kremove((vm_offset_t)p->p_addr + PAGE_SIZE * i);
	}
#endif
}

/*
 * Bring the UPAGES for a specified process back in.
 */
void
pmap_swapin_proc(p)
	struct proc *p;
{
#if 0
	int i,rv;
	vm_object_t upobj;
	vm_page_t m;

	upobj = p->p_upages_obj;
	for(i=0;i<UPAGES;i++) {

		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		pmap_kenter(((vm_offset_t) p->p_addr) + i * PAGE_SIZE,
			VM_PAGE_TO_PHYS(m));

		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(upobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_proc: cannot get upages for proc: %d\n", p->p_pid);
			m = vm_page_lookup(upobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}

		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
	}
#endif
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

void
pmap_pinit0(pmap)
	struct pmap *pmap;
{
	/*
	 * kernel_pmap is the same as any other pmap.
	 */
	pmap_pinit(pmap);
	pmap->pm_flags = 0;
	pmap->pm_rid = 0;
	pmap->pm_count = 1;
	pmap->pm_ptphint = NULL;
	pmap->pm_active = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	pmap->pm_flags = 0;
	pmap->pm_rid = 0;
	pmap->pm_count = 1;
	pmap->pm_ptphint = NULL;
	pmap->pm_active = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Wire in kernel global address entries.  To avoid a race condition
 * between pmap initialization and pmap_growkernel, this procedure
 * should be called after the vmspace is attached to the process
 * but before this pmap is activated.
 */
void
pmap_pinit2(pmap)
	struct pmap *pmap;
{
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
#if defined(DIAGNOSTIC)
	if (object->ref_count != 1)
		panic("pmap_release: pteobj reference count != 1");
#endif
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap_t pmap)
{
	int count;

	if (pmap == NULL)
		return;

	count = --pmap->pm_count;
	if (count == 0) {
		pmap_release(pmap);
		panic("destroying a pmap is not yet implemented");
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pmap)
{
	if (pmap != NULL) {
		pmap->pm_count++;
	}
}

/***************************************************
* page management routines.
 ***************************************************/

/*
 * free the pv_entry back to the free list
 */
static PMAP_INLINE void
free_pv_entry(pv_entry_t pv)
{
	pv_entry_count--;
	zfreei(pvzone, pv);
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static pv_entry_t
get_pv_entry(void)
{
	if (!pvinit)
		return zalloci(pvbootzone);

	pv_entry_count++;
	if (pv_entry_high_water &&
		(pv_entry_count > pv_entry_high_water) &&
		(pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup (&vm_pages_needed);
	}
	return (pv_entry_t) IA64_PHYS_TO_RR7(vtophys(zalloci(pvzone)));
}

/*
 * Add a pv_entry to the VHPT.
 */
static void
pmap_enter_vhpt(pv_entry_t pv)
{
	struct ia64_lpte *vhpte;

	vhpte = (struct ia64_lpte *) ia64_thash(pv->pv_va);

	pv->pv_pte.pte_chain = vhpte->pte_chain;
	vhpte->pte_chain = ia64_tpa((vm_offset_t) pv);

	if (!vhpte->pte_p && pv->pv_pte.pte_p)
		pmap_install_pte(vhpte, &pv->pv_pte);
	else
		ia64_mf();
}

/*
 * Update VHPT after pv->pv_pte has changed.
 */
static void
pmap_update_vhpt(pv_entry_t pv)
{
	struct ia64_lpte *vhpte;

	vhpte = (struct ia64_lpte *) ia64_thash(pv->pv_va);

	if ((!vhpte->pte_p || vhpte->pte_tag == pv->pv_pte.pte_tag)
	    && pv->pv_pte.pte_p)
		pmap_install_pte(vhpte, &pv->pv_pte);
}

/*
 * Remove a pv_entry from the VHPT. Return true if it worked.
 */
static int
pmap_remove_vhpt(pv_entry_t pv)
{
	struct ia64_lpte *pte;
	struct ia64_lpte *lpte;
	struct ia64_lpte *vhpte;
	u_int64_t tag;

	vhpte = (struct ia64_lpte *) ia64_thash(pv->pv_va);

	/*
	 * If the VHPTE is invalid, there can't be a collision chain.
	 */
	if (!vhpte->pte_p) {
		KASSERT(!vhpte->pte_chain, ("bad vhpte"));
		return 0;
	}

	lpte = vhpte;
	pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(vhpte->pte_chain);
	tag = ia64_ttag(pv->pv_va);

	while (pte->pte_tag != tag) {
		lpte = pte;
		if (pte->pte_chain)
			pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);
		else
			return 0; /* error here? */
	}

	/*
	 * Snip this pv_entry out of the collision chain.
	 */
	lpte->pte_chain = pte->pte_chain;

	/*
	 * If the VHPTE matches as well, change it to map the first
	 * element from the chain if there is one.
	 */
	if (vhpte->pte_tag == tag) {
		if (vhpte->pte_chain) {
			pte = (struct ia64_lpte *)
				IA64_PHYS_TO_RR7(vhpte->pte_chain);
			pmap_install_pte(vhpte, pte);
		} else {
			vhpte->pte_p = 0;
			ia64_mf();
		}
	}

	return 1;
}

/*
 * Make a pv_entry_t which maps the given virtual address. The pte
 * will be initialised with pte_p = 0. The function pmap_set_pv()
 * should be called to change the value of the pte.
 * Must be called at splvm().
 */
static pv_entry_t
pmap_make_pv(pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = get_pv_entry();
	bzero(pv, sizeof(*pv));
	pv->pv_va = va;
	pv->pv_pmap = pmap;

	pv->pv_pte.pte_p = 0;		/* invalid for now */
	pv->pv_pte.pte_ma = PTE_MA_WB;	/* cacheable, write-back */
	pv->pv_pte.pte_a = 0;
	pv->pv_pte.pte_d = 0;
	pv->pv_pte.pte_pl = 0;		/* privilege level 0 */
	pv->pv_pte.pte_ar = 3;		/* read/write/execute */
	pv->pv_pte.pte_ppn = 0;		/* physical address */
	pv->pv_pte.pte_ed = 0;
	pv->pv_pte.pte_ig = 0;

	pv->pv_pte.pte_ps = PAGE_SHIFT;	/* page size */
	pv->pv_pte.pte_key = 0;		/* protection key */

	pv->pv_pte.pte_tag = ia64_ttag(va);

	pmap_enter_vhpt(pv);

	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	pmap->pm_stats.resident_count++;

	return pv;
}

/*
 * Initialise a pv_entry_t with a given physical address and
 * protection code. If the passed vm_page_t is non-zero, the entry is
 * added to its list of mappings.
 * Must be called at splvm().
 */
static void
pmap_set_pv(pmap_t pmap, pv_entry_t pv, vm_offset_t pa,
	    int prot, vm_page_t m)
{
	if (pv->pv_pte.pte_p && pv->pv_pte.pte_ig & PTE_IG_MANAGED) {
		vm_offset_t opa = pv->pv_pte.pte_ppn << 12;
		vm_page_t om = PHYS_TO_VM_PAGE(opa);

		TAILQ_REMOVE(&om->md.pv_list, pv, pv_list);
		om->md.pv_list_count--;

		if (TAILQ_FIRST(&om->md.pv_list) == NULL)
			vm_page_flag_clear(om, PG_MAPPED | PG_WRITEABLE);
	}

	pv->pv_pte.pte_p = 1;		/* set to valid */

	/*
	 * Only track access/modify for managed pages.
	 */
	if (m) {
		pv->pv_pte.pte_a = 0;
		pv->pv_pte.pte_d = 0;
	} else {
		pv->pv_pte.pte_a = 1;
		pv->pv_pte.pte_d = 1;
	}

	pv->pv_pte.pte_pl = prot & 3;	/* privilege level */
	pv->pv_pte.pte_ar = prot >> 2;	/* access rights */
	pv->pv_pte.pte_ppn = pa >> 12;	/* physical address */

	if (m) {
		pv->pv_pte.pte_ig |= PTE_IG_MANAGED;
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count++;
	}

	/*
	 * Update the VHPT entry if it needs to change.
	 */
	pmap_update_vhpt(pv);
}
	
/*
 * Remove a mapping represented by a particular pv_entry_t. If the
 * passed vm_page_t is non-zero, then the entry is removed from it.
 * Must be called at splvm().
 */
static int
pmap_remove_pv(pmap_t pmap, pv_entry_t pv, vm_page_t m)
{
	int rtval;

	/*
	 * First remove from the VHPT.
	 */
	rtval = pmap_remove_vhpt(pv);
	if (!rtval)
		return rtval;

	if (m) {
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;

		if (TAILQ_FIRST(&m->md.pv_list) == NULL)
			vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
	}

	TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
	pmap->pm_stats.resident_count--;

	free_pv_entry(pv);

	return (rtval);
}

/*
 * Find a pv given a pmap and virtual address.
 */
static pv_entry_t
pmap_find_pv(pmap_t pmap, vm_offset_t va)
{
	struct ia64_lpte *pte;
	u_int64_t tag;

	pte = (struct ia64_lpte *) ia64_thash(va);
	if (!pte->pte_chain)
		return 0;

	tag = ia64_ttag(va);
	pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);

	while (pte->pte_tag != tag) {
		if (pte->pte_chain)
			pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);
		else
			return 0;
	}

	return (pv_entry_t) pte;	/* XXX wrong va */
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_offset_t 
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	pv_entry_t pv = pmap_find_pv(pmap, va);
	if (pv)
		return pmap_pte_pa(&pv->pv_pte);
	else
		return 0;
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

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
	int i, inval;
	pv_entry_t pv;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		pv = pmap_find_pv(kernel_pmap, tva);
		inval = 0;
		if (!pv)
			pv = pmap_make_pv(kernel_pmap, tva);
		else
			inval = 1;

		PMAP_DEBUG_VA(va);
		pmap_set_pv(kernel_pmap, pv,
			    VM_PAGE_TO_PHYS(m[i]),
			    (PTE_AR_RWX<<2) | PTE_PL_KERN, 0);
		if (inval)
			pmap_invalidate_page(kernel_pmap, tva);
	}
}

/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(va, count)
	vm_offset_t va;
	int count;
{
	int i;
	pv_entry_t pv;

	for (i = 0; i < count; i++) {
		pv = pmap_find_pv(kernel_pmap, va);
		PMAP_DEBUG_VA(va);
		if (pv) {
			pmap_remove_pv(kernel_pmap, pv, 0);
			pmap_invalidate_page(kernel_pmap, va);
		}
		va += PAGE_SIZE;
	}
}

/*
 * Add a wired page to the kva.
 */
void 
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	pv_entry_t pv;

	pv = pmap_find_pv(kernel_pmap, va);
	if (!pv)
		pv = pmap_make_pv(kernel_pmap, va);
	pmap_set_pv(kernel_pmap, pv,
		    pa, (PTE_AR_RWX<<2) | PTE_PL_KERN, 0);
	pmap_invalidate_page(kernel_pmap, va);
}

/*
 * Remove a page from the kva
 */
void
pmap_kremove(vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_find_pv(kernel_pmap, va);
	if (pv) {
		pmap_remove_pv(kernel_pmap, pv, 0);
		pmap_invalidate_page(kernel_pmap, va);
	}
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(vm_offset_t virt, vm_offset_t start, vm_offset_t end, int prot)
{
	/*
	 * XXX We should really try to use larger pagesizes here to
	 * cut down the number of PVs used.
	 */
	while (start < end) {
		pmap_kenter(virt, start);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return (virt);
}

/*
 * This routine is very drastic, but can save the system
 * in a pinch.
 */
void
pmap_collect()
{
	int i;
	vm_page_t m;
	static int warningdone=0;

	if (pmap_pagedaemon_waken == 0)
		return;

	if (warningdone < 5) {
		printf("pmap_collect: collecting pv entries -- suggest increasing PMAP_SHPGPERPROC\n");
		warningdone++;
	}

	for(i = 0; i < vm_page_array_size; i++) {
		m = &vm_page_array[i];
		if (m->wire_count || m->hold_count || m->busy ||
		    (m->flags & PG_BUSY))
			continue;
		pmap_remove_all(m);
	}
	pmap_pagedaemon_waken = 0;
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;
	vm_page_t m;
	int rtval;
	int s;

	s = splvm();

	pv = pmap_find_pv(pmap, va);

	rtval = 0;
	if (pv) {
		m = PHYS_TO_VM_PAGE(pmap_pte_pa(&pv->pv_pte));
		rtval = pmap_remove_pv(pmap, pv, m);
		pmap_invalidate_page(pmap, va);
	}
			
	splx(s);
	return;
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
	vm_offset_t va, nva;

	if (pmap == NULL)
		return;

	if (pmap->pm_stats.resident_count == 0)
		return;

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pmap_remove_page(pmap, sva);
		return;
	}

	if (atop(eva - sva) < pmap->pm_stats.resident_count) {
		for (va = sva; va < eva; va = nva) {
			pmap_remove_page(pmap, va);
			nva = va + PAGE_SIZE;
		}
	} else {
		pv_entry_t pv, pvnext;
		int s;

		s = splvm();
		for (pv = TAILQ_FIRST(&pmap->pm_pvlist);
			pv;
			pv = pvnext) {
			pvnext = TAILQ_NEXT(pv, pv_plist);
			if (pv->pv_va >= sva && pv->pv_va < eva) {
				vm_page_t m = PHYS_TO_VM_PAGE(pmap_pte_pa(&pv->pv_pte));
				va = pv->pv_va;
				pmap_remove_pv(pmap, pv, m);
				pmap_invalidate_page(pmap, va);
			}
		}
		splx(s);
	}
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

static void
pmap_remove_all(vm_page_t m)
{
	register pv_entry_t pv;
	int nmodify;
	int s;

	nmodify = 0;
#if defined(PMAP_DIAGNOSTIC)
	/*
	 * XXX this makes pmap_page_protect(NONE) illegal for non-managed
	 * pages!
	 */
	if (!pmap_initialized || (m->flags & PG_FICTITIOUS)) {
		panic("pmap_page_protect: illegal for unmanaged page, va: 0x%lx", VM_PAGE_TO_PHYS(m));
	}
#endif

	s = splvm();

	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		vm_page_t m = PHYS_TO_VM_PAGE(pmap_pte_pa(&pv->pv_pte));
		vm_offset_t va = pv->pv_va;
		pmap_remove_pv(pv->pv_pmap, pv, m);
		pmap_invalidate_page(pv->pv_pmap, va);
	}

	vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);

	splx(s);
	return;
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pmap_t oldpmap;
	pv_entry_t pv;
	int newprot;

	if (pmap == NULL)
		return;

	oldpmap = pmap_install(pmap);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		pmap_install(oldpmap);
		return;
	}

	if (prot & VM_PROT_WRITE) {
		pmap_install(oldpmap);
		return;
	}

	newprot = pte_prot(pmap, prot);

	if ((sva & PAGE_MASK) || (eva & PAGE_MASK))
		panic("pmap_protect: unaligned addresses");

	while (sva < eva) {
		/* 
		 * If page is invalid, skip this page
		 */
		pv = pmap_find_pv(pmap, sva);
		if (!pv) {
			sva += PAGE_SIZE;
			continue;
		}

		if (pmap_pte_prot(&pv->pv_pte) != newprot) {
			pmap_pte_set_prot(&pv->pv_pte, newprot);
			pmap_update_vhpt(pv);
			pmap_invalidate_page(pmap, sva);
		}

		sva += PAGE_SIZE;
	}
	pmap_install(oldpmap);
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
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	pmap_t oldpmap;
	vm_offset_t pa;
	pv_entry_t pv;
	vm_offset_t opa;
	struct ia64_lpte origpte;
	int managed;

	if (pmap == NULL)
		return;

	oldpmap = pmap_install(pmap);

	va &= ~PAGE_MASK;
#ifdef PMAP_DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
#endif

	pv = pmap_find_pv(pmap, va);
	if (!pv)
		pv = pmap_make_pv(pmap, va);

	origpte = pv->pv_pte;
	if (origpte.pte_p)
		opa = pmap_pte_pa(&origpte);
	else
		opa = 0;

	pa = VM_PAGE_TO_PHYS(m) & ~PAGE_MASK;
	managed = 0;

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (origpte.pte_p && (opa == pa)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && ((origpte.pte_ig & PTE_IG_WIRED) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (origpte.pte_ig & PTE_IG_WIRED))
			pmap->pm_stats.wired_count--;

		managed = origpte.pte_ig & PTE_IG_MANAGED;
		goto validate;
	}  else {
		/*
		 * Mapping has changed, invalidate old range and fall
		 * through to handle validating new mapping.
		 */
	}

	/*
	 * Increment counters
	 */
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 * This enters the pv_entry_t on the page's list if necessary.
	 */
	pmap_set_pv(pmap, pv, pa, pte_prot(pmap, prot), m);

	if (wired)
		pv->pv_pte.pte_ig |= PTE_IG_WIRED;

	/*
	 * if the mapping or permission bits are different, we need
	 * to invalidate the page.
	 */
	if (!pmap_equal_pte(&origpte, &pv->pv_pte)) {
		PMAP_DEBUG_VA(va);
		pmap_invalidate_page(pmap, va);
	}

	pmap_install(oldpmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * 5. Tlbflush is deferred to calling procedure.
 * 6. Page IS managed.
 * but is *MUCH* faster than pmap_enter...
 */

static void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;
	int s;

	s = splvm();

	pv = pmap_find_pv(pmap, va);
	if (!pv)
		pv = pmap_make_pv(pmap, va);

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	PMAP_DEBUG_VA(va);
	pmap_set_pv(pmap, pv, VM_PAGE_TO_PHYS(m),
		    (PTE_AR_R << 2) | PTE_PL_USER, m);

	splx(s);
}

/*
 * Make temporary mapping for a physical address. This is called
 * during dump.
 */
void *
pmap_kenter_temporary(vm_offset_t pa)
{
	return (void *) IA64_PHYS_TO_RR7(pa);
}

#define MAX_INIT_PT (96)
/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size, int limit)
{
	pmap_t oldpmap;
	vm_offset_t tmpidx;
	int psize;
	vm_page_t p;
	int objpgs;

	if (pmap == NULL || object == NULL)
		return;

	oldpmap = pmap_install(pmap);

	psize = ia64_btop(size);

	if ((object->type != OBJT_VNODE) ||
		(limit && (psize > MAX_INIT_PT) &&
			(object->resident_page_count > MAX_INIT_PT))) {
		pmap_install(oldpmap);
		return;
	}

	if (psize + pindex > object->size)
		psize = object->size - pindex;

	/*
	 * if we are processing a major portion of the object, then scan the
	 * entire thing.
	 */
	if (psize > (object->resident_page_count >> 2)) {
		objpgs = psize;

		for (p = TAILQ_FIRST(&object->memq);
		    ((objpgs > 0) && (p != NULL));
		    p = TAILQ_NEXT(p, listq)) {

			tmpidx = p->pindex;
			if (tmpidx < pindex) {
				continue;
			}
			tmpidx -= pindex;
			if (tmpidx >= psize) {
				continue;
			}
			if (((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				pmap_enter_quick(pmap,
						 addr + ia64_ptob(tmpidx), p);
				vm_page_flag_set(p, PG_MAPPED);
				vm_page_wakeup(p);
			}
			objpgs -= 1;
		}
	} else {
		/*
		 * else lookup the pages one-by-one.
		 */
		for (tmpidx = 0; tmpidx < psize; tmpidx += 1) {
			p = vm_page_lookup(object, tmpidx + pindex);
			if (p &&
			    ((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				pmap_enter_quick(pmap,
						 addr + ia64_ptob(tmpidx), p);
				vm_page_flag_set(p, PG_MAPPED);
				vm_page_wakeup(p);
			}
		}
	}
	pmap_install(oldpmap);
	return;
}

/*
 * pmap_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of pmap_object_init_pt, except it runs at page fault time instead
 * of mmap time.
 */
#define PFBAK 4
#define PFFOR 4
#define PAGEORDER_SIZE (PFBAK+PFFOR)

static int pmap_prefault_pageorder[] = {
	-PAGE_SIZE, PAGE_SIZE,
	-2 * PAGE_SIZE, 2 * PAGE_SIZE,
	-3 * PAGE_SIZE, 3 * PAGE_SIZE
	-4 * PAGE_SIZE, 4 * PAGE_SIZE
};

void
pmap_prefault(pmap, addra, entry)
	pmap_t pmap;
	vm_offset_t addra;
	vm_map_entry_t entry;
{
	int i;
	vm_offset_t starta;
	vm_offset_t addr;
	vm_pindex_t pindex;
	vm_page_t m, mpte;
	vm_object_t object;

	if (!curproc || (pmap != vmspace_pmap(curproc->p_vmspace)))
		return;

	object = entry->object.vm_object;

	starta = addra - PFBAK * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	mpte = NULL;
	for (i = 0; i < PAGEORDER_SIZE; i++) {
		vm_object_t lobject;
		pv_entry_t pv;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		pv = pmap_find_pv(pmap, addr);
		if (pv)
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = object;
		for (m = vm_page_lookup(lobject, pindex);
		    (!m && (lobject->type == OBJT_DEFAULT) && (lobject->backing_object));
		    lobject = lobject->backing_object) {
			if (lobject->backing_object_offset & PAGE_MASK)
				break;
			pindex += (lobject->backing_object_offset >> PAGE_SHIFT);
			m = vm_page_lookup(lobject->backing_object, pindex);
		}

		/*
		 * give-up when a page is not in memory
		 */
		if (m == NULL)
			break;

		if (((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if ((m->queue - m->pc) == PQ_CACHE) {
				vm_page_deactivate(m);
			}
			vm_page_busy(m);
			pmap_enter_quick(pmap, addr, m);
			vm_page_flag_set(m, PG_MAPPED);
			vm_page_wakeup(m);
		}
	}
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	pmap_t oldpmap;
	pv_entry_t pv;

	if (pmap == NULL)
		return;

	oldpmap = pmap_install(pmap);

	pv = pmap_find_pv(pmap, va);

	if (wired && !pmap_pte_w(&pv->pv_pte))
		pmap->pm_stats.wired_count++;
	else if (!wired && pmap_pte_w(&pv->pv_pte))
		pmap->pm_stats.wired_count--;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pmap_pte_set_w(&pv->pv_pte, wired);

	pmap_install(oldpmap);
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
 *	Routine:	pmap_kernel
 *	Function:
 *		Returns the physical map handle for the kernel.
 */
pmap_t
pmap_kernel()
{
	return (kernel_pmap);
}

/*
 *	pmap_zero_page zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.
 */

void
pmap_zero_page(vm_offset_t pa)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(pa);
	bzero((caddr_t) va, PAGE_SIZE);
}


/*
 *	pmap_zero_page_area zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.
 *
 *	off and size must reside within a single page.
 */

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(pa);
	bzero((char *)(caddr_t)va + off, size);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	src = IA64_PHYS_TO_RR7(src);
	dst = IA64_PHYS_TO_RR7(dst);
	bcopy((caddr_t) src, (caddr_t) dst, PAGE_SIZE);
}


/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t pmap;
	vm_offset_t sva, eva;
	boolean_t pageable;
{
}

/*
 * this routine returns true if a physical page resides
 * in the given pmap.
 */
boolean_t
pmap_page_exists(pmap, m)
	pmap_t pmap;
	vm_page_t m;
{
	register pv_entry_t pv;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	s = splvm();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pmap == pmap) {
			splx(s);
			return TRUE;
		}
	}
	splx(s);
	return (FALSE);
}

#define PMAP_REMOVE_PAGES_CURPROC_ONLY
/*
 * Remove all pages from specified address space
 * this aids process exit speeds.  Also, this code
 * is special cased for current process only, but
 * can have the more generic (and slightly slower)
 * mode enabled.  This is much faster than pmap_remove
 * in the case of running down an entire address space.
 */
void
pmap_remove_pages(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	pv_entry_t pv, npv;
	int s;

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (!curproc || (pmap != vmspace_pmap(curproc->p_vmspace))) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
#endif

	s = splvm();
	for (pv = TAILQ_FIRST(&pmap->pm_pvlist);
		pv;
		pv = npv) {
		vm_page_t m;

		npv = TAILQ_NEXT(pv, pv_plist);

		if (pv->pv_va >= eva || pv->pv_va < sva) {
			continue;
		}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (pv->pv_pte.pte_ig & PTE_IG_WIRED) {
			continue;
		}

		PMAP_DEBUG_VA(pv->pv_va);

		m = PHYS_TO_VM_PAGE(pmap_pte_pa(&pv->pv_pte));
		pmap_remove_pv(pmap, pv, m);
	}
	splx(s);

	pmap_invalidate_all(pmap);
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	pv_entry_t pv;

	if ((prot & VM_PROT_WRITE) != 0)
		return;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		for (pv = TAILQ_FIRST(&m->md.pv_list);
		     pv;
		     pv = TAILQ_NEXT(pv, pv_list)) {
			int newprot = pte_prot(pv->pv_pmap, prot);
			pmap_t oldpmap = pmap_install(pv->pv_pmap);
			pmap_pte_set_prot(&pv->pv_pte, newprot);
			pmap_update_vhpt(pv);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			pmap_install(oldpmap);
		}
	} else {
		pmap_remove_all(m);
	}
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return (ia64_ptob(ppn));
}

/*
 *	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 *	
 */
int
pmap_ts_referenced(vm_page_t m)
{
	pv_entry_t pv;
	int count = 0;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return 0;

	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pte.pte_a) {
			pmap_t oldpmap = pmap_install(pv->pv_pmap);
			count++;
			pv->pv_pte.pte_a = 0;
			pmap_update_vhpt(pv);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			pmap_install(oldpmap);
		}
	}

	return count;
}

#if 0
/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
static boolean_t
pmap_is_referenced(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pte.pte_a) {
			return 1;
		}
	}

	return 0;
}
#endif

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pte.pte_d) {
			return 1;
		}
	}

	return 0;
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pte.pte_d) {
			pmap_t oldpmap = pmap_install(pv->pv_pmap);
			pv->pv_pte.pte_d = 0;
			pmap_update_vhpt(pv);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			pmap_install(oldpmap);
		}
	}
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		if (pv->pv_pte.pte_a) {
			pmap_t oldpmap = pmap_install(pv->pv_pmap);
			pv->pv_pte.pte_a = 0;
			pmap_update_vhpt(pv);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			pmap_install(oldpmap);
		}
	}
}

/*
 * Miscellaneous support routines follow
 */

static void
ia64_protection_init()
{
	int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			break;

		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_X_RX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_X_RX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = (PTE_AR_RW << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RW << 2) | PTE_PL_USER;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RWX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RWX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_R << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = (PTE_AR_RW << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RW << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RWX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RWX << 2) | PTE_PL_USER;
			break;
		}
	}
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev(pa, size)
	vm_offset_t pa;
	vm_size_t size;
{
	return (void*) IA64_PHYS_TO_RR6(pa);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap, addr)
	pmap_t pmap;
	vm_offset_t addr;
{
	pv_entry_t pv;
	struct ia64_lpte *pte;
	int val = 0;
	
	pv = pmap_find_pv(pmap, addr);
	if (pv == 0) {
		return 0;
	}
	pte = &pv->pv_pte;

	if (pmap_pte_v(pte)) {
		vm_page_t m;
		vm_offset_t pa;

		val = MINCORE_INCORE;
		if ((pte->pte_ig & PTE_IG_MANAGED) == 0)
			return val;

		pa = pmap_pte_pa(pte);

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if (pte->pte_d)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else if (pmap_is_modified(m))
			val |= MINCORE_MODIFIED_OTHER;
		/*
		 * Referenced by us
		 */
		if (pte->pte_a)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;

		/*
		 * Referenced by someone
		 */
		else if (pmap_ts_referenced(m)) {
			val |= MINCORE_REFERENCED_OTHER;
			vm_page_flag_set(m, PG_REFERENCED);
		}
	} 
	return val;
}

void
pmap_activate(struct proc *p)
{
	pmap_install(vmspace_pmap(p->p_vmspace));
}

pmap_t
pmap_install(pmap_t pmap)
{
	pmap_t oldpmap;
	int rid;

	oldpmap = PCPU_GET(current_pmap);

	if (pmap == oldpmap || pmap == kernel_pmap)
		return pmap;

	PCPU_SET(current_pmap, pmap);
	if (!pmap) {
		/*
		 * RIDs 0..4 have no mappings to make sure we generate 
		 * page faults on accesses.
		 */
		ia64_set_rr(IA64_RR_BASE(0), (0 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(1), (1 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(2), (2 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(3), (3 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(4), (4 << 8)|(PAGE_SHIFT << 2)|1);
		return oldpmap;
	}

	pmap->pm_active = 1;	/* XXX use bitmap for SMP */

 reinstall:
	rid = pmap->pm_rid & ((1 << pmap_ridbits) - 1);
	ia64_set_rr(IA64_RR_BASE(0), ((rid + 0) << 8)|(PAGE_SHIFT << 2)|1);
	ia64_set_rr(IA64_RR_BASE(1), ((rid + 1) << 8)|(PAGE_SHIFT << 2)|1);
	ia64_set_rr(IA64_RR_BASE(2), ((rid + 2) << 8)|(PAGE_SHIFT << 2)|1);
	ia64_set_rr(IA64_RR_BASE(3), ((rid + 3) << 8)|(PAGE_SHIFT << 2)|1);
	ia64_set_rr(IA64_RR_BASE(4), ((rid + 4) << 8)|(PAGE_SHIFT << 2)|1);

	/*
	 * If we need a new RID, get it now. Note that we need to
	 * remove our old mappings (if any) from the VHTP, so we will
	 * run on the old RID for a moment while we invalidate the old 
	 * one. XXX maybe we should just clear out the VHTP when the
	 * RID generation rolls over.
	 */
	if ((pmap->pm_rid>>pmap_ridbits) != (pmap_nextrid>>pmap_ridbits)) {
		if (pmap->pm_rid)
			pmap_invalidate_rid(pmap);
		pmap_get_rid(pmap);
		goto reinstall;
	}

	return oldpmap;
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{

	return addr;
}

#if 0
#if defined(PMAP_DEBUG)
pmap_pid_dump(int pid)
{
	pmap_t pmap;
	struct proc *p;
	int npte = 0;
	int index;
	LIST_FOREACH(p, &allproc, p_list) {
		if (p->p_pid != pid)
			continue;

		if (p->p_vmspace) {
			int i,j;
			index = 0;
			pmap = vmspace_pmap(p->p_vmspace);
			for(i=0;i<1024;i++) {
				pd_entry_t *pde;
				pt_entry_t *pte;
				unsigned base = i << PDRSHIFT;
				
				pde = &pmap->pm_pdir[i];
				if (pde && pmap_pde_v(pde)) {
					for(j=0;j<1024;j++) {
						unsigned va = base + (j << PAGE_SHIFT);
						if (va >= (vm_offset_t) VM_MIN_KERNEL_ADDRESS) {
							if (index) {
								index = 0;
								printf("\n");
							}
							return npte;
						}
						pte = pmap_pte_quick( pmap, va);
						if (pte && pmap_pte_v(pte)) {
							vm_offset_t pa;
							vm_page_t m;
							pa = *(int *)pte;
							m = PHYS_TO_VM_PAGE(pa);
							printf("va: 0x%x, pt: 0x%x, h: %d, w: %d, f: 0x%x",
								va, pa, m->hold_count, m->wire_count, m->flags);
							npte++;
							index++;
							if (index >= 2) {
								index = 0;
								printf("\n");
							} else {
								printf(" ");
							}
						}
					}
				}
			}
		}
	}
	return npte;
}
#endif

#if defined(DEBUG)

static void	pads __P((pmap_t pm));
static void	pmap_pvdump __P((vm_page_t m));

/* print address space of pmap*/
static void
pads(pm)
	pmap_t pm;
{
        int i, j;
	vm_offset_t va;
	pt_entry_t *ptep;

	if (pm == kernel_pmap)
		return;
	for (i = 0; i < 1024; i++)
		if (pm->pm_pdir[i])
			for (j = 0; j < 1024; j++) {
				va = (i << PDRSHIFT) + (j << PAGE_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
					continue;
				if (pm != kernel_pmap && va > UPT_MAX_ADDRESS)
					continue;
				ptep = pmap_pte_quick(pm, va);
				if (pmap_pte_v(ptep))
					printf("%x:%x ", va, *(int *) ptep);
			};

}

static void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	pv_entry_t pv;

	printf("pa %x", pa);
	m = PHYS_TO_VM_PAGE(pa);
	for (pv = TAILQ_FIRST(&m->md.pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
		printf(" -> pmap %x, va %x",
		    pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}
#endif
#endif
