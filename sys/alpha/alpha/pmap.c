/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 1998 Doug Rabson
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
 * Notes for alpha pmap.
 * 
 * On alpha, pm_pdeobj will hold lev1, lev2 and lev3 page tables.
 * Indices from 0 to NUSERLEV3MAPS-1 will map user lev3 page tables,
 * indices from NUSERLEV3MAPS to NUSERLEV3MAPS+NUSERLEV2MAPS-1 will
 * map user lev2 page tables and index NUSERLEV3MAPS+NUSERLEV2MAPS
 * will map the lev1 page table.  The lev1 table will self map at
 * address VADDR(PTLEV1I,0,0).
 * 
 * The vm_object kptobj holds the kernel page tables on i386 (62 or 63
 * of them, depending on whether the system is SMP).  On alpha, kptobj
 * will hold the lev3 and lev2 page tables for K1SEG.  Indices 0 to
 * NKLEV3MAPS-1 will map kernel lev3 page tables and indices
 * NKLEV3MAPS to NKLEV3MAPS+NKLEV2MAPS will map lev2 page tables. (XXX
 * should the kernel Lev1map be inserted into this object?).
 * 
 * pvtmmap is not needed for alpha since K0SEG maps all of physical
 * memory.
 * 
 * 
 * alpha virtual memory map:
 * 
 * 
 *  Address							Lev1 index
 * 
 * 	         	---------------------------------
 *  0000000000000000    | 				|	0
 * 		        |				|
 * 		        |				|
 * 		        |				|
 * 		        |				|
 * 		       ---      		       ---
 * 		                User space (USEG)
 * 		       ---      		       ---
 * 		        |				|
 * 		        |				|
 * 		        |				|
 * 		        |				|
 *  000003ffffffffff    |				|	511=UMAXLEV1I
 * 	                ---------------------------------
 *  fffffc0000000000    |				|	512=K0SEGLEV1I
 * 	                |	Kernel code/data/bss	|
 * 	                |				|
 * 	                |				|
 * 	                |				|
 * 	               ---			       ---
 * 	                	K0SEG
 * 	               ---			       ---
 * 	                |				|
 * 	                |	1-1 physical/virtual	|
 * 	                |				|
 * 	                |				|
 *  fffffdffffffffff    |				|
 * 	                ---------------------------------
 *  fffffe0000000000    |				|	768=K1SEGLEV1I
 * 	                |	Kernel dynamic data	|
 * 	                |				|
 * 	                |				|
 * 	                |				|
 * 	               ---			       ---
 * 	                	K1SEG
 * 	               ---	        	       ---
 * 	                |				|
 * 	                |	mapped by ptes		|
 * 	                |				|
 * 	                |				|
 *  fffffff7ffffffff    |				|
 * 	                ---------------------------------
 *  fffffffe00000000    | 				|	1023=PTLEV1I
 * 		        |	PTmap (pte self map)	|
 *  ffffffffffffffff	|				|
 * 			---------------------------------
 * 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/smp.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

#include <sys/user.h>

#include <machine/md_var.h>
#include <machine/rpb.h>

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

/*
 * Some macros for manipulating virtual addresses
 */
#define ALPHA_L1SIZE		(1L << ALPHA_L1SHIFT)
#define ALPHA_L2SIZE		(1L << ALPHA_L2SHIFT)

#define alpha_l1trunc(va)	((va) & ~(ALPHA_L1SIZE-1))
#define alpha_l2trunc(va)	((va) & ~(ALPHA_L2SIZE-1))

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define pmap_pte_w(pte)		((*(pte) & PG_W) != 0)
#define pmap_pte_managed(pte)	((*(pte) & PG_MANAGED) != 0)
#define pmap_pte_v(pte)		((*(pte) & PG_V) != 0)
#define pmap_pte_pa(pte)	alpha_ptob(ALPHA_PTE_TO_PFN(*(pte)))
#define pmap_pte_prot(pte)	(*(pte) & PG_PROT)

#define pmap_pte_set_w(pte, v) ((v)?(*pte |= PG_W):(*pte &= ~PG_W))
#define pmap_pte_set_prot(pte, v) ((*pte &= ~PG_PROT), (*pte |= (v)))

/*
 * Given a map and a machine independent protection code,
 * convert to an alpha protection code.
 */
#define pte_prot(m, p)		(protection_codes[m == kernel_pmap ? 0 : 1][p])
int	protection_codes[2][8];

/*
 * Return non-zero if this pmap is currently active
 */
#define pmap_isactive(pmap)	(pmap->pm_active)

/* 
 * Extract level 1, 2 and 3 page table indices from a va
 */
#define PTMASK	((1 << ALPHA_PTSHIFT) - 1)

#define pmap_lev1_index(va)	(((va) >> ALPHA_L1SHIFT) & PTMASK)
#define pmap_lev2_index(va)	(((va) >> ALPHA_L2SHIFT) & PTMASK)
#define pmap_lev3_index(va)	(((va) >> ALPHA_L3SHIFT) & PTMASK)

/*
 * Given a physical address, construct a pte
 */
#define pmap_phys_to_pte(pa)	ALPHA_PTE_FROM_PFN(alpha_btop(pa))

/*
 * Given a page frame number, construct a k0seg va
 */
#define pmap_k0seg_to_pfn(va)	alpha_btop(ALPHA_K0SEG_TO_PHYS(va))

/*
 * Given a pte, construct a k0seg va
 */
#define pmap_k0seg_to_pte(va)	ALPHA_PTE_FROM_PFN(pmap_k0seg_to_pfn(va))

/*
 * Lev1map:
 *
 *	Kernel level 1 page table.  This maps all kernel level 2
 *	page table pages, and is used as a template for all user
 *	pmap level 1 page tables.  When a new user level 1 page
 *	table is allocated, all Lev1map PTEs for kernel addresses
 *	are copied to the new map.
 *
 * Lev2map:
 *
 *	Initial set of kernel level 2 page table pages.  These
 *	map the kernel level 3 page table pages.  As kernel
 *	level 3 page table pages are added, more level 2 page
 *	table pages may be added to map them.  These pages are
 *	never freed.
 *
 * Lev3map:
 *
 *	Initial set of kernel level 3 page table pages.  These
 *	map pages in K1SEG.  More level 3 page table pages may
 *	be added at run-time if additional K1SEG address space
 *	is required.  These pages are never freed.
 *
 * Lev2mapsize:
 *
 *	Number of entries in the initial Lev2map.
 *
 * Lev3mapsize:
 *
 *	Number of entries in the initial Lev3map.
 *
 * NOTE: When mappings are inserted into the kernel pmap, all
 * level 2 and level 3 page table pages must already be allocated
 * and mapped into the parent page table.
 */
pt_entry_t	*Lev1map, *Lev2map, *Lev3map;
vm_size_t	Lev2mapsize, Lev3mapsize;

/*
 * Statically allocated kernel pmap
 */
struct pmap kernel_pmap_store;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */

static vm_object_t kptobj;

static int nklev3, nklev2;
vm_offset_t kernel_vm_end;

/*
 * Data for the ASN allocator
 */
static int pmap_maxasn;
static pmap_t pmap_active[MAXCPU];
static LIST_HEAD(,pmap) allpmaps;
static struct mtx allpmaps_lock;

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static struct vm_object pvzone_obj;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
static int pmap_pagedaemon_waken = 0;

static PMAP_INLINE void	free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(void);
static void	alpha_protection_init(void);
static void	pmap_changebit(vm_page_t m, int bit, boolean_t setem);

static void	pmap_remove_all(vm_page_t m);
static vm_page_t pmap_enter_quick(pmap_t pmap, vm_offset_t va,
				      vm_page_t m, vm_page_t mpte);
static int pmap_remove_pte(pmap_t pmap, pt_entry_t* ptq, vm_offset_t sva);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va);
static int pmap_remove_entry(struct pmap *pmap, vm_page_t m, vm_offset_t va);
static void pmap_insert_entry(pmap_t pmap, vm_offset_t va,
		vm_page_t mpte, vm_page_t m);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va);

static int pmap_release_free_page(pmap_t pmap, vm_page_t p);
static vm_page_t _pmap_allocpte(pmap_t pmap, unsigned ptepindex);
static vm_page_t pmap_page_lookup(vm_object_t object, vm_pindex_t pindex);
static int pmap_unuse_pt(pmap_t, vm_offset_t, vm_page_t);
static void *pmap_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait);
#ifdef SMP
static void pmap_invalidate_page_action(void *arg);
static void pmap_invalidate_all_action(void *arg);
#endif


/*
 *	Routine:	pmap_lev1pte
 *	Function:
 *		Extract the level 1 page table entry associated
 *		with the given map/virtual_address pair.
 */
static PMAP_INLINE pt_entry_t*
pmap_lev1pte(pmap_t pmap, vm_offset_t va)
{
	if (!pmap)
		return 0;
	return &pmap->pm_lev1[pmap_lev1_index(va)];
}

/*
 *	Routine:	pmap_lev2pte
 *	Function:
 *		Extract the level 2 page table entry associated
 *		with the given map/virtual_address pair.
 */
static PMAP_INLINE pt_entry_t*
pmap_lev2pte(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t* l1pte;
	pt_entry_t* l2map;

	l1pte = pmap_lev1pte(pmap, va);
	if (!pmap_pte_v(l1pte))
		return 0;

	l2map = (pt_entry_t*) ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l1pte));
	return &l2map[pmap_lev2_index(va)];
}

/*
 *	Routine:	pmap_lev3pte
 *	Function:
 *		Extract the level 3 page table entry associated
 *		with the given map/virtual_address pair.
 */
static PMAP_INLINE pt_entry_t*
pmap_lev3pte(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t* l2pte;
	pt_entry_t* l3map;

	l2pte = pmap_lev2pte(pmap, va);
	if (!l2pte || !pmap_pte_v(l2pte))
		return 0;

	l3map = (pt_entry_t*) ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l2pte));
	return &l3map[pmap_lev3_index(va)];
}

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

	va = ALPHA_PHYS_TO_K0SEG(pa);
	bzero((caddr_t) va, size);
	return va;
}

extern pt_entry_t rom_pte;			/* XXX */
extern int prom_mapped;				/* XXX */

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t ptaddr, u_int maxasn)
{
	pt_entry_t newpte;
	int i;

	/*
	 * Setup ASNs. PCPU_GET(next_asn) and PCPU_GET(current_asngen) are set
	 * up already.
	 */
	pmap_maxasn = maxasn;

	/*
	 * Allocate a level 1 map for the kernel.
	 */
	Lev1map = (pt_entry_t*) pmap_steal_memory(PAGE_SIZE);

	/*
	 * Allocate a level 2 map for the kernel
	 */
	Lev2map = (pt_entry_t*) pmap_steal_memory(PAGE_SIZE);
	Lev2mapsize = PAGE_SIZE;

	/*
	 * Allocate some level 3 maps for the kernel
	 */
	Lev3map = (pt_entry_t*) pmap_steal_memory(PAGE_SIZE*NKPT);
	Lev3mapsize = NKPT * PAGE_SIZE;

	/* Map all of the level 2 maps */
	for (i = 0; i < howmany(Lev2mapsize, PAGE_SIZE); i++) {
		unsigned long pfn =
			pmap_k0seg_to_pfn((vm_offset_t) Lev2map) + i;
		newpte = ALPHA_PTE_FROM_PFN(pfn);
		newpte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_W;
		Lev1map[K1SEGLEV1I + i] = newpte;
	}


	/* Setup the mapping for the prom console */
	{

		if (pmap_uses_prom_console()) {
			/* XXX save old pte so that we can remap prom if necessary */
			rom_pte = *(pt_entry_t *)ptaddr & ~PG_ASM;	/* XXX */
		}
		prom_mapped = 0;

		/*
		 * Actually, this code lies.  The prom is still mapped, and will
		 * remain so until the context switch after alpha_init() returns.
		 * Printfs using the firmware before then will end up frobbing
		 * Lev1map unnecessarily, but that's OK.
		 */
	}

	/*
	 * Level 1 self mapping.
	 *
	 * Don't set PG_ASM since the self-mapping is different for each
	 * address space.
	 */
	newpte = pmap_k0seg_to_pte((vm_offset_t) Lev1map);
	newpte |= PG_V | PG_KRE | PG_KWE;
	Lev1map[PTLEV1I] = newpte;

	/* Map all of the level 3 maps */
	for (i = 0; i < howmany(Lev3mapsize, PAGE_SIZE); i++) {
		unsigned long pfn =
			pmap_k0seg_to_pfn((vm_offset_t) Lev3map) + i;
		newpte = ALPHA_PTE_FROM_PFN(pfn);
		newpte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_W;
		Lev2map[i] = newpte;
	}

	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i+2]; i+= 2) ;
	avail_end = phys_avail[i+1];

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VPTBASE;

	/*
	 * Initialize protection array.
	 */
	alpha_protection_init();

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	kernel_pmap->pm_lev1 = Lev1map;
	kernel_pmap->pm_active = ~0;
	kernel_pmap->pm_asn[alpha_pal_whami()].asn = 0;
	kernel_pmap->pm_asn[alpha_pal_whami()].gen = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	nklev3 = NKPT;
	nklev2 = 1;

	/*
	 * Initialize list of pmaps.
	 */
	LIST_INIT(&allpmaps);
	LIST_INSERT_HEAD(&allpmaps, kernel_pmap, pm_list);
	
	/*
	 * Set up proc0's PCB such that the ptbr points to the right place
	 * and has the kernel pmap's.
	 */
	thread0.td_pcb->pcb_hw.apcb_ptbr =
	    ALPHA_K0SEG_TO_PHYS((vm_offset_t)Lev1map) >> PAGE_SHIFT;
	thread0.td_pcb->pcb_hw.apcb_asn = 0;
}

int
pmap_uses_prom_console()
{
	int cputype;

	cputype = hwrpb->rpb_type;
	return (cputype == ST_DEC_21000 || ST_DEC_4100);

	return 0;
}

static void *
pmap_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	*flags = UMA_SLAB_PRIV;
	return (void *)kmem_alloc(kernel_map, bytes);
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
		m->md.pv_flags = 0;
 	}

	/*
	 * init the pv free list
	 */
	initial_pvs = vm_page_array_size;
	if (initial_pvs < MINPV)
		initial_pvs = MINPV;
	pvzone = uma_zcreate("PV ENTRY", sizeof (struct pv_entry), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	uma_zone_set_allocf(pvzone, pmap_allocf);
	uma_prealloc(pvzone, initial_pvs);
	/*
	 * object for kernel page table pages
	 */
	kptobj = vm_object_allocate(OBJT_DEFAULT, NKLEV3MAPS + NKLEV2MAPS);

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
	int shpgperproc = PMAP_SHPGPERPROC;

	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + vm_page_array_size;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	uma_zone_set_obj(pvzone, &pvzone_obj, pv_entry_max);
}


/***************************************************
 * Manipulate TLBs for a pmap
 ***************************************************/

static void
pmap_invalidate_asn(pmap_t pmap)
{
	pmap->pm_asn[PCPU_GET(cpuid)].gen = 0;
}

struct pmap_invalidate_page_arg {
	pmap_t pmap;
	vm_offset_t va;
};

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
#ifdef SMP
	struct pmap_invalidate_page_arg arg;
	arg.pmap = pmap;
	arg.va = va;

	smp_rendezvous(0, pmap_invalidate_page_action, 0, (void *) &arg);
}

static void
pmap_invalidate_page_action(void *arg)
{
	pmap_t pmap = ((struct pmap_invalidate_page_arg *) arg)->pmap;
	vm_offset_t va = ((struct pmap_invalidate_page_arg *) arg)->va;
#endif

	if (pmap->pm_active & PCPU_GET(cpumask)) {
		ALPHA_TBIS(va);
		alpha_pal_imb();		/* XXX overkill? */
	} else {
		pmap_invalidate_asn(pmap);
	}
}

static void
pmap_invalidate_all(pmap_t pmap)
{
#ifdef SMP
	smp_rendezvous(0, pmap_invalidate_all_action, 0, (void *) pmap);
}

static void
pmap_invalidate_all_action(void *arg)
{
	pmap_t pmap = (pmap_t) arg;
#endif

	if (pmap->pm_active & PCPU_GET(cpumask)) {
		ALPHA_TBIA();
		alpha_pal_imb();		/* XXX overkill? */
	} else
		pmap_invalidate_asn(pmap);
}

static void
pmap_get_asn(pmap_t pmap)
{

	if (PCPU_GET(next_asn) > pmap_maxasn) {
		/*
		 * Start a new ASN generation.
		 *
		 * Invalidate all per-process mappings and I-cache
		 */
		PCPU_SET(next_asn, 0);
		PCPU_SET(current_asngen, (PCPU_GET(current_asngen) + 1) &
		    ASNGEN_MASK);

		if (PCPU_GET(current_asngen) == 0) {
			/*
			 * Clear the pm_asn[].gen of all pmaps.
			 * This is safe since it is only called from
			 * pmap_activate after it has deactivated
			 * the old pmap and it only affects this cpu.
			 */
			pmap_t tpmap;
			       
#ifdef PMAP_DIAGNOSTIC
			printf("pmap_get_asn: generation rollover\n");
#endif
			PCPU_SET(current_asngen, 1);
			mtx_lock_spin(&allpmaps_lock);
			LIST_FOREACH(tpmap, &allpmaps, pm_list) {
				tpmap->pm_asn[PCPU_GET(cpuid)].gen = 0;
			}
			mtx_unlock_spin(&allpmaps_lock);
		}

		/*
		 * Since we are about to start re-using ASNs, we must
		 * clear out the TLB and the I-cache since they are tagged
		 * with the ASN.
		 */
		ALPHA_TBIAP();
		alpha_pal_imb();	/* XXX overkill? */
	}
	pmap->pm_asn[PCPU_GET(cpuid)].asn = PCPU_GET(next_asn);
	PCPU_SET(next_asn, PCPU_GET(next_asn) + 1);
	pmap->pm_asn[PCPU_GET(cpuid)].gen = PCPU_GET(current_asngen);
}

/***************************************************
 * Low level helper routines.....
 ***************************************************/



/*
 * this routine defines the region(s) of memory that should
 * not be tested for the modified bit.
 */
static PMAP_INLINE int
pmap_track_modified(vm_offset_t va)
{
	if ((va < kmi.clean_sva) || (va >= kmi.clean_eva)) 
		return 1;
	else
		return 0;
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
	pt_entry_t* pte = pmap_lev3pte(pmap, va);
	if (pte)
		return alpha_ptob(ALPHA_PTE_TO_PFN(*pte));
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
	int i;
	pt_entry_t *pte;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		pt_entry_t npte = pmap_phys_to_pte(VM_PAGE_TO_PHYS(m[i]))
			| PG_ASM | PG_KRE | PG_KWE | PG_V;
		pt_entry_t opte;
		pte = vtopte(tva);
		opte = *pte;
		*pte = npte;
		if (opte)
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
	register pt_entry_t *pte;

	for (i = 0; i < count; i++) {
		pte = vtopte(va);
		*pte = 0;
		pmap_invalidate_page(kernel_pmap, va);
		va += PAGE_SIZE;
	}
}

/*
 * add a wired page to the kva
 * note that in order for the mapping to take effect -- you
 * should do a invltlb after doing the pmap_kenter...
 */
PMAP_INLINE void 
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	pt_entry_t *pte;
	pt_entry_t npte, opte;

	npte = pmap_phys_to_pte(pa) | PG_ASM | PG_KRE | PG_KWE | PG_V;
	pte = vtopte(va);
	opte = *pte;
	*pte = npte;
	if (opte)
		pmap_invalidate_page(kernel_pmap, va);
}

/*
 * remove a page from the kernel pagetables
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	register pt_entry_t *pte;

	pte = vtopte(va);
	*pte = 0;
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
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t start, vm_offset_t end, int prot)
{
	return ALPHA_PHYS_TO_K0SEG(start);
}


static vm_page_t
pmap_page_lookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;
retry:
	m = vm_page_lookup(object, pindex);
	if (m && vm_page_sleep_busy(m, FALSE, "pplookp"))
		goto retry;
	return m;
}

/*
 * Create the kernel stack for a new thread.
 * This routine directly affects the fork perf for a process and thread.
 */
void
pmap_new_thread(struct thread *td)
{
	int i;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	pt_entry_t *ptek, oldpte;

	/*
	 * allocate object for the kstack
	 */
	ksobj = vm_object_allocate(OBJT_DEFAULT, KSTACK_PAGES);
	td->td_kstack_obj = ksobj;

#ifdef KSTACK_GUARD
	/* get a kernel virtual address for the kstack for this thread */
	ks = kmem_alloc_nofault(kernel_map, (KSTACK_PAGES + 1) * PAGE_SIZE);
	if (ks == NULL)
		panic("pmap_new_thread: kstack allocation failed");

	/* Set the first page to be the unmapped guard page. */
	ptek = vtopte(ks);
	oldpte = *ptek;
	*ptek = 0;
	if (oldpte)
		pmap_invalidate_page(kernel_pmap, ks);
	/* move to the next page, which is where the real stack starts. */
	ks += PAGE_SIZE;
	td->td_kstack = ks;
	ptek++;
#else
	/* get a kernel virtual address for the kstack for this thread */
	ks = kmem_alloc_nofault(kernel_map, KSTACK_PAGES * PAGE_SIZE);
	if (ks == NULL)
		panic("pmap_new_thread: kstack allocation failed");
	td->td_kstack = ks;
	ptek = vtopte(ks);
#endif
	/*
	 * For the length of the stack, link in a real page of ram for each
	 * page of stack.
	 */
	for (i = 0; i < KSTACK_PAGES; i++) {
		/*
		 * Get a kernel stack page
		 */
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page
		 */
		m->wire_count++;
		cnt.v_wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		oldpte = ptek[i];
		ptek[i] = pmap_phys_to_pte(VM_PAGE_TO_PHYS(m))
			| PG_ASM | PG_KRE | PG_KWE | PG_V;
		if (oldpte) 
			pmap_invalidate_page(kernel_pmap, ks + i * PAGE_SIZE);

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

/*
 * Dispose the kernel stack for a thread that has exited.
 * This routine directly impacts the exit perf of a thread.
 */
void
pmap_dispose_thread(td)
	struct thread *td;
{
	int i;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	pt_entry_t *ptek;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	ptek = vtopte(ks);
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("pmap_dispose_thread: kstack already missing?");
		vm_page_busy(m);
		ptek[i] = 0;
		pmap_invalidate_page(kernel_pmap, ks + i * PAGE_SIZE);
		vm_page_lock_queues();
		vm_page_unwire(m, 0);
		vm_page_free(m);
		vm_page_unlock_queues();
	}

	/*
	 * Free the space that this stack was mapped to in the kernel
	 * address map.
	 */
#ifdef KSTACK_GUARD
	kmem_free(kernel_map, ks - PAGE_SIZE, (KSTACK_PAGES + 1) * PAGE_SIZE);
#else
	kmem_free(kernel_map, ks, KSTACK_PAGES * PAGE_SIZE);
#endif
	vm_object_deallocate(ksobj);
}

/*
 * Allow the kernel stack for a thread to be prejudicially paged out.
 */
void
pmap_swapout_thread(td)
	struct thread *td;
{
	int i;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;

	/*
	 * Make sure we aren't fpcurthread.
	 */
	alpha_fpstate_save(td, 1);

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("pmap_swapout_thread: kstack already missing?");
		vm_page_lock_queues();
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		vm_page_unlock_queues();
		pmap_kremove(ks + i * PAGE_SIZE);
	}
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
void
pmap_swapin_thread(td)
	struct thread *td;
{
	int i, rv;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(ksobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_thread: cannot get kstack for proc: %d\n", td->td_proc->p_pid);
			m = vm_page_lookup(ksobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		vm_page_lock_queues();
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		vm_page_unlock_queues();
	}

	/*
	 * The pcb may be at a different physical address now so cache the
	 * new address.
	 */
	td->td_md.md_pcbpaddr = (void *)vtophys((vm_offset_t)td->td_pcb);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

/*
 * This routine unholds page table pages, and if the hold count
 * drops to zero, then it decrements the wire count.
 */
static int 
_pmap_unwire_pte_hold(pmap_t pmap, vm_offset_t va, vm_page_t m)
{

	while (vm_page_sleep_busy(m, FALSE, "pmuwpt"))
		;

	if (m->hold_count == 0) {
		vm_offset_t pteva;
		pt_entry_t* pte;

		/*
		 * unmap the page table page
		 */
		if (m->pindex >= NUSERLEV3MAPS) {
			/* Level 2 page table */
			pte = pmap_lev1pte(pmap, va);
			pteva = (vm_offset_t) PTlev2 + alpha_ptob(m->pindex - NUSERLEV3MAPS);
		} else {
			/* Level 3 page table */
			pte = pmap_lev2pte(pmap, va);
			pteva = (vm_offset_t) PTmap + alpha_ptob(m->pindex);
		}

		*pte = 0;

		if (m->pindex < NUSERLEV3MAPS) {
			/* unhold the level 2 page table */
			vm_page_t lev2pg;
			lev2pg = pmap_page_lookup(pmap->pm_pteobj,
						  NUSERLEV3MAPS + pmap_lev1_index(va));
			vm_page_unhold(lev2pg);
			if (lev2pg->hold_count == 0)
				_pmap_unwire_pte_hold(pmap, va, lev2pg);
		}

		--pmap->pm_stats.resident_count;
		/*
		 * Do a invltlb to make the invalidated mapping
		 * take effect immediately.
		 */
		pmap_invalidate_page(pmap, pteva);

		if (pmap->pm_ptphint == m)
			pmap->pm_ptphint = NULL;

		/*
		 * If the page is finally unwired, simply free it.
		 */
		--m->wire_count;
		if (m->wire_count == 0) {

			vm_page_flash(m);
			vm_page_busy(m);
			vm_page_free_zero(m);
			--cnt.v_wire_count;
		}
		return 1;
	}
	return 0;
}

static PMAP_INLINE int
pmap_unwire_pte_hold(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	vm_page_unhold(m);
	if (m->hold_count == 0)
		return _pmap_unwire_pte_hold(pmap, va, m);
	else
		return 0;
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, vm_page_t mpte)
{
	unsigned ptepindex;
	if (va >= VM_MAXUSER_ADDRESS)
		return 0;

	if (mpte == NULL) {
		ptepindex = (va >> ALPHA_L2SHIFT);
		if (pmap->pm_ptphint &&
			(pmap->pm_ptphint->pindex == ptepindex)) {
			mpte = pmap->pm_ptphint;
		} else {
			mpte = pmap_page_lookup(pmap->pm_pteobj, ptepindex);
			pmap->pm_ptphint = mpte;
		}
	}

	return pmap_unwire_pte_hold(pmap, va, mpte);
}

void
pmap_pinit0(pmap)
	struct pmap *pmap;
{
	int i;

	pmap->pm_lev1 = Lev1map;
	pmap->pm_ptphint = NULL;
	pmap->pm_active = 0;
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asn[i].asn = 0;
		pmap->pm_asn[i].gen = 0;
	}
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	mtx_init(&allpmaps_lock, "allpmaps", NULL, MTX_SPIN | MTX_QUIET);
	LIST_INSERT_HEAD(&allpmaps, pmap, pm_list);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	vm_page_t lev1pg;
	int i;

	/*
	 * allocate object for the ptes
	 */
	if (pmap->pm_pteobj == NULL)
		pmap->pm_pteobj = vm_object_allocate(OBJT_DEFAULT, NUSERLEV3MAPS + NUSERLEV2MAPS + 1);

	/*
	 * allocate the page directory page
	 */
	lev1pg = vm_page_grab(pmap->pm_pteobj, NUSERLEV3MAPS + NUSERLEV2MAPS,
			      VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

	lev1pg->wire_count = 1;
	++cnt.v_wire_count;

	vm_page_flag_clear(lev1pg, PG_MAPPED | PG_BUSY);	/* not mapped normally */
	lev1pg->valid = VM_PAGE_BITS_ALL;

	pmap->pm_lev1 = (pt_entry_t*) ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(lev1pg));
	if ((lev1pg->flags & PG_ZERO) == 0)
		bzero(pmap->pm_lev1, PAGE_SIZE);


	/* install self-referential address mapping entry (not PG_ASM) */
	pmap->pm_lev1[PTLEV1I] = pmap_phys_to_pte(VM_PAGE_TO_PHYS(lev1pg))
		| PG_V | PG_KRE | PG_KWE;

	pmap->pm_ptphint = NULL;
	pmap->pm_active = 0;
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asn[i].asn = 0;
		pmap->pm_asn[i].gen = 0;
	}
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);
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
	bcopy(PTlev1 + K1SEGLEV1I, pmap->pm_lev1 + K1SEGLEV1I, nklev2 * PTESIZE);
}

static int
pmap_release_free_page(pmap_t pmap, vm_page_t p)
{
	pt_entry_t* pte;
	pt_entry_t* l2map;

	if (p->pindex >= NUSERLEV3MAPS + NUSERLEV2MAPS)
		/* level 1 page table */
		pte = &pmap->pm_lev1[PTLEV1I];
	else if (p->pindex >= NUSERLEV3MAPS)
		/* level 2 page table */
		pte = &pmap->pm_lev1[p->pindex - NUSERLEV3MAPS];
	else {
		/* level 3 page table */
		pte = &pmap->pm_lev1[p->pindex >> ALPHA_PTSHIFT];
		l2map = (pt_entry_t*) ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(pte));
		pte = &l2map[p->pindex & ((1 << ALPHA_PTSHIFT) - 1)];
	}

	/*
	 * This code optimizes the case of freeing non-busy
	 * page-table pages.  Those pages are zero now, and
	 * might as well be placed directly into the zero queue.
	 */
	if (vm_page_sleep_busy(p, FALSE, "pmaprl"))
		return 0;

	vm_page_busy(p);

	/*
	 * Remove the page table page from the processes address space.
	 */
	*pte = 0;
	pmap->pm_stats.resident_count--;

#ifdef PMAP_DEBUG
	if (p->hold_count)  {
		panic("pmap_release: freeing held page table page");
	}
#endif
	/*
	 * Level1  pages need to have the kernel
	 * stuff cleared, so they can go into the zero queue also.
	 */
	if (p->pindex == NUSERLEV3MAPS + NUSERLEV2MAPS)
		bzero(pmap->pm_lev1 + K1SEGLEV1I, nklev2 * PTESIZE);

	if (pmap->pm_ptphint && (pmap->pm_ptphint->pindex == p->pindex))
		pmap->pm_ptphint = NULL;

#ifdef PMAP_DEBUG
	{
	    u_long *lp = (u_long*) ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(p));
	    u_long *ep = (u_long*) ((char*) lp + PAGE_SIZE);
	    for (; lp < ep; lp++)
		if (*lp != 0)
		    panic("pmap_release_free_page: page not zero");
	}
#endif

	p->wire_count--;
	cnt.v_wire_count--;
	vm_page_free_zero(p);
	return 1;
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap, ptepindex)
	pmap_t	pmap;
	unsigned ptepindex;
{
	pt_entry_t* pte;
	vm_offset_t ptepa;
	vm_page_t m;

	/*
	 * Find or fabricate a new pagetable page
	 */
	m = vm_page_grab(pmap->pm_pteobj, ptepindex,
			VM_ALLOC_ZERO | VM_ALLOC_RETRY);

	KASSERT(m->queue == PQ_NONE,
		("_pmap_allocpte: %p->queue != PQ_NONE", m));

	if (m->wire_count == 0)
		cnt.v_wire_count++;
	m->wire_count++;

	/*
	 * Increment the hold count for the page table page
	 * (denoting a new mapping.)
	 */
	m->hold_count++;

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	pmap->pm_stats.resident_count++;

	ptepa = VM_PAGE_TO_PHYS(m);

	if (ptepindex >= NUSERLEV3MAPS) {
		pte = &pmap->pm_lev1[ptepindex - NUSERLEV3MAPS];
	} else {
		int l1index = ptepindex >> ALPHA_PTSHIFT;
		pt_entry_t* l1pte = &pmap->pm_lev1[l1index];
		pt_entry_t* l2map;
		if (!pmap_pte_v(l1pte))
			_pmap_allocpte(pmap, NUSERLEV3MAPS + l1index);
		else {
			vm_page_t l2page =
				pmap_page_lookup(pmap->pm_pteobj,
						 NUSERLEV3MAPS + l1index);
			l2page->hold_count++;
		}
		l2map = (pt_entry_t*) ALPHA_PHYS_TO_K0SEG(pmap_pte_pa(l1pte));
		pte = &l2map[ptepindex & ((1 << ALPHA_PTSHIFT) - 1)];
	}

	*pte = pmap_phys_to_pte(ptepa) | PG_KRE | PG_KWE | PG_V;

	/*
	 * Set the page table hint
	 */
	pmap->pm_ptphint = m;

	if ((m->flags & PG_ZERO) == 0)
		bzero((caddr_t) ALPHA_PHYS_TO_K0SEG(ptepa), PAGE_SIZE);

	m->valid = VM_PAGE_BITS_ALL;
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_flag_set(m, PG_MAPPED);
	vm_page_wakeup(m);

	return m;
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va)
{
	unsigned ptepindex;
	pt_entry_t* lev2pte;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = va >> (PAGE_SHIFT + ALPHA_PTSHIFT);

	/*
	 * Get the level2 entry
	 */
	lev2pte = pmap_lev2pte(pmap, va);

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (lev2pte && pmap_pte_v(lev2pte)) {
		/*
		 * In order to get the page table page, try the
		 * hint first.
		 */
		if (pmap->pm_ptphint &&
			(pmap->pm_ptphint->pindex == ptepindex)) {
			m = pmap->pm_ptphint;
		} else {
			m = pmap_page_lookup(pmap->pm_pteobj, ptepindex);
			pmap->pm_ptphint = m;
		}
		m->hold_count++;
		return m;
	}
	/*
	 * Here if the pte page isn't mapped, or if it has been deallocated.
	 */
	return _pmap_allocpte(pmap, ptepindex);
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
	vm_page_t p,n,lev1pg;
	vm_object_t object = pmap->pm_pteobj;
	int curgeneration;

#if defined(DIAGNOSTIC)
	if (object->ref_count != 1)
		panic("pmap_release: pteobj reference count != 1");
#endif
	
	lev1pg = NULL;
retry:
	curgeneration = object->generation;
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = n) {
		n = TAILQ_NEXT(p, listq);
		if (p->pindex >= NUSERLEV3MAPS) {
			continue;
		}
		while (1) {
			if (!pmap_release_free_page(pmap, p) &&
				(object->generation != curgeneration))
				goto retry;
		}
	}
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = n) {
		n = TAILQ_NEXT(p, listq);
		if (p->pindex < NUSERLEV3MAPS) {
			/* can this happen?  maybe panic */
			goto retry;
		}
		if (p->pindex >= NUSERLEV3MAPS + NUSERLEV2MAPS) {
			lev1pg = p;
			continue;
		}
		while (1) {
			if (!pmap_release_free_page(pmap, p) &&
				(object->generation != curgeneration))
				goto retry;
		}
	}

	if (lev1pg && !pmap_release_free_page(pmap, lev1pg))
		goto retry;
	mtx_lock_spin(&allpmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	/* XXX come back to this */
	struct pmap *pmap;
	pt_entry_t* pte;
	pt_entry_t newlev1, newlev2;
	vm_offset_t pa;
	vm_page_t nkpg;

	critical_enter();
	if (kernel_vm_end == 0) {
		kernel_vm_end = VM_MIN_KERNEL_ADDRESS;;

		/* Count the level 2 page tables */
		nklev2 = 0;
		nklev3 = 0;
		while (pmap_pte_v(pmap_lev1pte(kernel_pmap, kernel_vm_end))) {
			nklev2++;
			nklev3 += (1L << ALPHA_PTSHIFT);
			kernel_vm_end += ALPHA_L1SIZE;
		}
			
		/* Count the level 3 page tables in the last level 2 page table */
		kernel_vm_end -= ALPHA_L1SIZE;
		nklev3 -= (1 << ALPHA_PTSHIFT);
		while (pmap_pte_v(pmap_lev2pte(kernel_pmap, kernel_vm_end))) {
			nklev3++;
			kernel_vm_end += ALPHA_L2SIZE;
		}
	}

	addr = (addr + ALPHA_L2SIZE) & ~(ALPHA_L2SIZE - 1);
	while (kernel_vm_end < addr) {
		/*
		 * If the level 1 pte is invalid, allocate a new level 2 page table
		 */
		pte = pmap_lev1pte(kernel_pmap, kernel_vm_end);
		if (!pmap_pte_v(pte)) {
			int pindex = NKLEV3MAPS + pmap_lev1_index(kernel_vm_end) - K1SEGLEV1I;

			nkpg = vm_page_alloc(kptobj, pindex,
			    VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED);
			if (!nkpg)
				panic("pmap_growkernel: no memory to grow kernel");
			printf("pmap_growkernel: growing to %lx\n", addr);
			printf("pmap_growkernel: adding new level2 page table\n");

			nklev2++;
			pmap_zero_page(nkpg);

			pa = VM_PAGE_TO_PHYS(nkpg);
			newlev1 = pmap_phys_to_pte(pa)
				| PG_V | PG_ASM | PG_KRE | PG_KWE;

			mtx_lock_spin(&allpmaps_lock);
			LIST_FOREACH(pmap, &allpmaps, pm_list) {
				*pmap_lev1pte(pmap, kernel_vm_end) = newlev1;
			}
			mtx_unlock_spin(&allpmaps_lock);
			*pte = newlev1;
			pmap_invalidate_all(kernel_pmap);
		}

		/*
		 * If the level 2 pte is invalid, allocate a new level 3 page table
		 */
		pte = pmap_lev2pte(kernel_pmap, kernel_vm_end);
		if (pmap_pte_v(pte)) {
			kernel_vm_end = (kernel_vm_end + ALPHA_L2SIZE) & ~(ALPHA_L2SIZE - 1);
			continue;
		}

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = vm_page_alloc(kptobj, nklev3,
		    VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");

		nklev3++;
		pmap_zero_page(nkpg);
		pa = VM_PAGE_TO_PHYS(nkpg);
		newlev2 = pmap_phys_to_pte(pa) | PG_V | PG_ASM | PG_KRE | PG_KWE;
		*pte = newlev2;

		kernel_vm_end = (kernel_vm_end + ALPHA_L2SIZE) & ~(ALPHA_L2SIZE - 1);
	}
	critical_exit();
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
	uma_zfree(pvzone, pv);
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
	pv_entry_count++;
	if (pv_entry_high_water &&
		(pv_entry_count > pv_entry_high_water) &&
		(pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup (&vm_pages_needed);
	}
	return uma_zalloc(pvzone, M_NOWAIT);
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
	static int warningdone = 0;

	if (pmap_pagedaemon_waken == 0)
		return;

	if (warningdone < 5) {
		printf("pmap_collect: collecting pv entries -- suggest increasing PMAP_SHPGPERPROC\n");
		warningdone++;
	}

	for(i = 0; i < vm_page_array_size; i++) {
		m = &vm_page_array[i];
		if (m->wire_count || m->hold_count || m->busy ||
		    (m->flags & (PG_BUSY | PG_UNMANAGED)))
			continue;
		pmap_remove_all(m);
	}
	pmap_pagedaemon_waken = 0;
}
	

/*
 * If it is the first entry on the list, it is actually
 * in the header and we must copy the following entry up
 * to the header.  Otherwise we must search the list for
 * the entry.  In either case we free the now unused entry.
 */

static int
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;
	int rtval;
	int s;

	s = splvm();
	if (m->md.pv_list_count < pmap->pm_stats.resident_count) {
		TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
			if (pmap == pv->pv_pmap && va == pv->pv_va) 
				break;
		}
	} else {
		TAILQ_FOREACH(pv, &pmap->pm_pvlist, pv_plist) {
			if (va == pv->pv_va) 
				break;
		}
	}

	rtval = 0;
	if (pv) {
		rtval = pmap_unuse_pt(pmap, va, pv->pv_ptem);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		if (TAILQ_FIRST(&m->md.pv_list) == NULL)
			vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);

		TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
		free_pv_entry(pv);
	}
			
	splx(s);
	return rtval;
}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t mpte, vm_page_t m)
{

	int s;
	pv_entry_t pv;

	s = splvm();
	pv = get_pv_entry();
	pv->pv_va = va;
	pv->pv_pmap = pmap;
	pv->pv_ptem = mpte;

	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;

	splx(s);
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t va)
{
	pt_entry_t oldpte;
	vm_page_t m;

	oldpte = *ptq;
	*ptq = 0;
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	if (oldpte & PG_MANAGED) {
		m = PHYS_TO_VM_PAGE(pmap_pte_pa(&oldpte));
		if ((oldpte & PG_FOW) == 0) {
			if (pmap_track_modified(va))
				vm_page_dirty(m);
		}
		if ((oldpte & PG_FOR) == 0)
			vm_page_flag_set(m, PG_REFERENCED);
		return pmap_remove_entry(pmap, m, va);
	} else {
		return pmap_unuse_pt(pmap, va, NULL);
	}

	return 0;
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va)
{
	register pt_entry_t *ptq;

	ptq = pmap_lev3pte(pmap, va);
	
	/*
	 * if there is no pte for this address, just skip it!!!
	 */
	if (!ptq || !pmap_pte_v(ptq))
		return;

	/*
	 * get a local va for mappings for this pmap.
	 */
	(void) pmap_remove_pte(pmap, ptq, va);
	pmap_invalidate_page(pmap, va);

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

	for (va = sva; va < eva; va = nva) {
		if (!pmap_pte_v(pmap_lev1pte(pmap, va))) {
			nva = alpha_l1trunc(va + ALPHA_L1SIZE);
			continue;
		}

		if (!pmap_pte_v(pmap_lev2pte(pmap, va))) {
			nva = alpha_l2trunc(va + ALPHA_L2SIZE);
			continue;
		}

		pmap_remove_page(pmap, va);
		nva = va + PAGE_SIZE;
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
	pt_entry_t *pte, tpte;
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
		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);

		pv->pv_pmap->pm_stats.resident_count--;

		if (pmap_pte_pa(pte) != VM_PAGE_TO_PHYS(m))
			panic("pmap_remove_all: pv_table for %lx is inconsistent", VM_PAGE_TO_PHYS(m));

		tpte = *pte;

		*pte = 0;
		if (tpte & PG_W)
			pv->pv_pmap->pm_stats.wired_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if ((tpte & PG_FOW) == 0) {
			if (pmap_track_modified(pv->pv_va))
				vm_page_dirty(m);
		}
		if ((tpte & PG_FOR) == 0)
			vm_page_flag_set(m, PG_REFERENCED);

		pmap_invalidate_page(pv->pv_pmap, pv->pv_va);

		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}

	vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);

	splx(s);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pt_entry_t* pte;
	int newprot;

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	newprot = pte_prot(pmap, prot);

	if ((sva & PAGE_MASK) || (eva & PAGE_MASK))
		panic("pmap_protect: unaligned addresses");

	while (sva < eva) {

		/*
		 * If level 1 pte is invalid, skip this segment
		 */
		pte = pmap_lev1pte(pmap, sva);
		if (!pmap_pte_v(pte)) {
			sva = alpha_l1trunc(sva) + ALPHA_L1SIZE;
			continue;
		}

		/*
		 * If level 2 pte is invalid, skip this segment
		 */
		pte = pmap_lev2pte(pmap, sva);
		if (!pmap_pte_v(pte)) {
			sva = alpha_l2trunc(sva) + ALPHA_L2SIZE;
			continue;
		}

		/* 
		 * If level 3 pte is invalid, skip this page
		 */
		pte = pmap_lev3pte(pmap, sva);
		if (!pmap_pte_v(pte)) {
			sva += PAGE_SIZE;
			continue;
		}

		if (pmap_pte_prot(pte) != newprot) {
			pt_entry_t oldpte = *pte;
			vm_page_t m = NULL;
			if ((oldpte & PG_FOR) == 0) {
				m = PHYS_TO_VM_PAGE(pmap_pte_pa(pte));
				vm_page_flag_set(m, PG_REFERENCED);
				oldpte |= (PG_FOR | PG_FOE);
			}
			if ((oldpte & PG_FOW) == 0) {
				m = PHYS_TO_VM_PAGE(pmap_pte_pa(pte));
				if (pmap_track_modified(sva))
					vm_page_dirty(m);
				oldpte |= PG_FOW;
			}
			oldpte = (oldpte & ~PG_PROT) | newprot;
			*pte = oldpte;
			pmap_invalidate_page(pmap, sva);
		}

		sva += PAGE_SIZE;
	}
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
	vm_offset_t pa;
	pt_entry_t *pte;
	vm_offset_t opa;
	pt_entry_t origpte, newpte;
	vm_page_t mpte;
	int managed;

	if (pmap == NULL)
		return;

	va &= ~PAGE_MASK;
#ifdef PMAP_DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
#endif

	mpte = NULL;
	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_allocpte(pmap, va);
	}

	pte = pmap_lev3pte(pmap, va);

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL) {
		panic("pmap_enter: invalid kernel page tables pmap=%p, va=0x%lx\n", pmap, va);
	}

	origpte = *pte;
	pa = VM_PAGE_TO_PHYS(m) & ~PAGE_MASK;
	managed = 0;
	opa = pmap_pte_pa(pte);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (origpte && (opa == pa)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && ((origpte & PG_W) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (origpte & PG_W))
			pmap->pm_stats.wired_count--;

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->hold_count--;

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (origpte & PG_MANAGED) {
			if ((origpte & PG_FOW) != PG_FOW
			    && pmap_track_modified(va)) {
				vm_page_t om;
				om = PHYS_TO_VM_PAGE(opa);
				vm_page_dirty(om);
			}
		}

		managed = origpte & PG_MANAGED;
		goto validate;
	} 
	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		int err;
		err = pmap_remove_pte(pmap, pte, va);
		if (err)
			panic("pmap_enter: pte vanished, va: 0x%lx", va);
	}

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if (pmap_initialized && (m->flags & PG_FICTITIOUS) == 0) {
		pmap_insert_entry(pmap, va, mpte, m);
		managed |= PG_MANAGED;
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	newpte = pmap_phys_to_pte(pa) | pte_prot(pmap, prot) | PG_V | managed;

	if (managed) {
		/*
		 * Set up referenced/modified emulation for the new
		 * mapping. Any old referenced/modified emulation
		 * results for the old mapping will have been recorded
		 * either in pmap_remove_pte() or above in the code
		 * which handles protection and/or wiring changes.
		 */
		newpte |= (PG_FOR | PG_FOW | PG_FOE);
	}

	if (wired)
		newpte |= PG_W;

	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if (origpte != newpte) {
		*pte = newpte;
		if (origpte)
			pmap_invalidate_page(pmap, va);
		if (prot & VM_PROT_EXECUTE)
			alpha_pal_imb();
	}
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

static vm_page_t
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_page_t mpte)
{
	register pt_entry_t *pte;


	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		unsigned ptepindex;
		pt_entry_t* l2pte;

		/*
		 * Calculate lev2 page index
		 */
		ptepindex = va >> ALPHA_L2SHIFT;
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->hold_count++;
		} else {
retry:
			/*
			 * Get the level 2 entry
			 */
			l2pte = pmap_lev2pte(pmap, va);

			/*
			 * If the level 2 page table is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (l2pte && pmap_pte_v(l2pte)) {
				if (pmap->pm_ptphint &&
				    (pmap->pm_ptphint->pindex == ptepindex)) {
					mpte = pmap->pm_ptphint;
				} else {
					mpte = pmap_page_lookup(pmap->pm_pteobj,
					    ptepindex);
					pmap->pm_ptphint = mpte;
				}
				if (mpte == NULL)
					goto retry;
				mpte->hold_count++;
			} else {
				mpte = _pmap_allocpte(pmap, ptepindex);
			}
		}
	} else {
		mpte = NULL;
	}

	/*
	 * This call to vtopte makes the assumption that we are
	 * entering the page into the current pmap.  In order to support
	 * quick entry into any pmap, one would likely use pmap_pte_quick.
	 * But that isn't as quick as vtopte.
	 */
	pte = vtopte(va);
	if (*pte) {
		if (mpte)
			pmap_unwire_pte_hold(pmap, va, mpte);
		alpha_pal_imb();		/* XXX overkill? */
		return 0;
	}

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	pmap_insert_entry(pmap, va, mpte, m);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	/*
	 * Now validate mapping with RO protection
	 */
	*pte = pmap_phys_to_pte(VM_PAGE_TO_PHYS(m)) | PG_V | PG_KRE | PG_URE | PG_MANAGED | PG_FOR | PG_FOE | PG_FOW;

	alpha_pal_imb();			/* XXX overkill? */
	return mpte;
}

/*
 * Make temporary mapping for a physical address. This is called
 * during dump.
 */
void *
pmap_kenter_temporary(vm_offset_t pa, int i)
{
	return (void *) ALPHA_PHYS_TO_K0SEG(pa - (i * PAGE_SIZE));
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
	vm_offset_t tmpidx;
	int psize;
	vm_page_t p, mpte;
	int objpgs;

	if (pmap == NULL || object == NULL)
		return;

	psize = alpha_btop(size);

	if ((object->type != OBJT_VNODE) ||
		((limit & MAP_PREFAULT_PARTIAL) && (psize > MAX_INIT_PT) &&
			(object->resident_page_count > MAX_INIT_PT))) {
		return;
	}

	if (psize + pindex > object->size) {
		if (object->size < pindex)
			return;
		psize = object->size - pindex;
	}

	mpte = NULL;
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
			/*
			 * don't allow an madvise to blow away our really
			 * free pages allocating pv entries.
			 */
			if ((limit & MAP_PREFAULT_MADVISE) &&
			    cnt.v_free_count < cnt.v_free_reserved) {
				break;
			}
			vm_page_lock_queues();
			if (((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
				(p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				vm_page_unlock_queues();
				mpte = pmap_enter_quick(pmap, 
					addr + alpha_ptob(tmpidx), p, mpte);
				vm_page_lock_queues();
				vm_page_flag_set(p, PG_MAPPED);
				vm_page_wakeup(p);
			}
			vm_page_unlock_queues();
			objpgs -= 1;
		}
	} else {
		/*
		 * else lookup the pages one-by-one.
		 */
		for (tmpidx = 0; tmpidx < psize; tmpidx += 1) {
			/*
			 * don't allow an madvise to blow away our really
			 * free pages allocating pv entries.
			 */
			if ((limit & MAP_PREFAULT_MADVISE) &&
			    cnt.v_free_count < cnt.v_free_reserved) {
				break;
			}
			p = vm_page_lookup(object, tmpidx + pindex);
			if (p == NULL)
				continue;
			vm_page_lock_queues();
			if ((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL &&
				(p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				vm_page_unlock_queues();
				mpte = pmap_enter_quick(pmap, 
					addr + alpha_ptob(tmpidx), p, mpte);
				vm_page_lock_queues();
				vm_page_flag_set(p, PG_MAPPED);
				vm_page_wakeup(p);
			}
			vm_page_unlock_queues();
		}
	}
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

	if (!curthread || (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)))
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
		pt_entry_t *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		if (!pmap_pte_v(pmap_lev1pte(pmap, addr))
		    || !pmap_pte_v(pmap_lev2pte(pmap, addr)))
			continue;

		pte = vtopte(addr);
		if (*pte)
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
		vm_page_lock_queues();
		if (((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			(m->busy == 0) &&
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if ((m->queue - m->pc) == PQ_CACHE) {
				vm_page_deactivate(m);
			}
			vm_page_busy(m);
			vm_page_unlock_queues();
			mpte = pmap_enter_quick(pmap, addr, m, mpte);
			vm_page_lock_queues();
			vm_page_flag_set(m, PG_MAPPED);
			vm_page_wakeup(m);
		}
		vm_page_unlock_queues();
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
	pt_entry_t *pte;

	if (pmap == NULL)
		return;

	pte = pmap_lev3pte(pmap, va);

	if (wired && !pmap_pte_w(pte))
		pmap->pm_stats.wired_count++;
	else if (!wired && pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pmap_pte_set_w(pte, wired);
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
 *	pmap_zero_page zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.
 */

void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va = ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(m));
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
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va = ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(m));
	bzero((char *)(caddr_t)va + off, size);
}


/*
 *	pmap_zero_page_idle zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.  This is for the vm_pagezero idle process.
 */

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va = ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(m));
	bzero((caddr_t) va, PAGE_SIZE);
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
	vm_offset_t src = ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = ALPHA_PHYS_TO_K0SEG(VM_PAGE_TO_PHYS(mdst));
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
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap, m)
	pmap_t pmap;
	vm_page_t m;
{
	pv_entry_t pv;
	int loops = 0;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	s = splvm();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pmap) {
			splx(s);
			return TRUE;
		}
		loops++;
		if (loops >= 16)
			break;
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
	pt_entry_t *pte, tpte;
	vm_page_t m;
	pv_entry_t pv, npv;
	int s;

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (!curthread || (pmap != vmspace_pmap(curthread->td_proc->p_vmspace))) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
#endif

	s = splvm();
	for(pv = TAILQ_FIRST(&pmap->pm_pvlist);
		pv;
		pv = npv) {

		if (pv->pv_va >= eva || pv->pv_va < sva) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
		pte = vtopte(pv->pv_va);
#else
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
#endif
		if (!pmap_pte_v(pte))
			panic("pmap_remove_pages: page on pm_pvlist has no pte\n");
		tpte = *pte;


/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (tpte & PG_W) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}
		*pte = 0;

		m = PHYS_TO_VM_PAGE(pmap_pte_pa(&tpte));

		pv->pv_pmap->pm_stats.resident_count--;

		npv = TAILQ_NEXT(pv, pv_plist);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);

		m->md.pv_list_count--;
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		if (TAILQ_FIRST(&m->md.pv_list) == NULL) {
			vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
		}

		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}
	splx(s);
	pmap_invalidate_all(pmap);
}

/*
 * this routine is used to modify bits in ptes
 */
static void
pmap_changebit(vm_page_t m, int bit, boolean_t setem)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	int changed;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	s = splvm();
	changed = 0;

	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		/*
		 * don't write protect pager mappings
		 */
		if (!setem && bit == (PG_UWE|PG_KWE)) {
			if (!pmap_track_modified(pv->pv_va))
				continue;
		}

#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (cb) at va: 0x%lx\n", pv->pv_va);
			continue;
		}
#endif

		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);

		changed = 0;
		if (setem) {
			*pte |= bit;
			changed = 1;
		} else {
			pt_entry_t pbits = *pte;
			if (pbits & bit) {
				changed = 1;
				*pte = pbits & ~bit;
			}
		}
		if (changed)
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
	}
	splx(s);
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
			pmap_changebit(m, PG_KWE|PG_UWE, FALSE);
		} else {
			pmap_remove_all(m);
		}
	}
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return (alpha_ptob(ppn));
}

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
	pv_entry_t pv;
	pt_entry_t *pte;
	int count;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return 0;

	/*
	 * Loop over current mappings looking for any which have don't
	 * have PG_FOR set (i.e. ones where we have taken an emulate
	 * reference trap recently).
	 */
	count = 0;
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);
		
		if (!(*pte & PG_FOR)) {
			count++;
			*pte |= PG_FOR | PG_FOE;
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
	}

	return count;
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
	pv_entry_t pv;
	pt_entry_t *pte;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	/*
	 * A page is modified if any mapping has had its PG_FOW flag
	 * cleared.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);
		if (!(*pte & PG_FOW))
			return 1;
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
	pt_entry_t *pte;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	/*
	 * Loop over current mappings setting PG_FOW where needed.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);
		
		if (!(*pte & PG_FOW)) {
			*pte |= PG_FOW;
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
	}
}

/*
 *	pmap_page_is_free:
 *
 *	Called when a page is freed to allow pmap to clean up
 *	any extra state associated with the page.  In this case
 *	clear modified/referenced bits.
 */
void
pmap_page_is_free(vm_page_t m)
{
	m->md.pv_flags = 0;
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
	pt_entry_t *pte;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	/*
	 * Loop over current mappings setting PG_FOR and PG_FOE where needed.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pte = pmap_lev3pte(pv->pv_pmap, pv->pv_va);
		
		if (!(*pte & (PG_FOR | PG_FOE))) {
			*pte |= (PG_FOR | PG_FOE);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
	}
}

/*
 * pmap_emulate_reference:
 *
 *	Emulate reference and/or modified bit hits.
 *	From NetBSD
 */
void
pmap_emulate_reference(struct vmspace *vm, vm_offset_t v, int user, int write)
{
	pt_entry_t faultoff, *pte;
	vm_offset_t pa;
	int user_addr;

	/*
	 * Convert process and virtual address to physical address.
	 */
	if (v >= VM_MIN_KERNEL_ADDRESS) {
		if (user)
			panic("pmap_emulate_reference: user ref to kernel");
		pte = vtopte(v);
		user_addr = 0;
	} else {
		KASSERT(vm != NULL, ("pmap_emulate_reference: bad vmspace"));
		pte = pmap_lev3pte(vm->vm_map.pmap, v);
		user_addr = 1;
	}
#ifdef DEBUG				/* These checks are more expensive */
	if (!pmap_pte_v(pte))
		panic("pmap_emulate_reference: invalid pte");
#if 0
	/*
	 * Can't do these, because cpu_fork and cpu_swapin call
	 * pmap_emulate_reference(), and the bits aren't guaranteed,
	 * for them...
	 */
	if (write) {
		if (!(*pte & (user ? PG_UWE : PG_UWE | PG_KWE)))
			panic("pmap_emulate_reference: write but unwritable");
		if (!(*pte & PG_FOW))
			panic("pmap_emulate_reference: write but not FOW");
	} else {
		if (!(*pte & (user ? PG_URE : PG_URE | PG_KRE)))
			panic("pmap_emulate_reference: !write but unreadable");
		if (!(*pte & (PG_FOR | PG_FOE)))
			panic("pmap_emulate_reference: !write but not FOR|FOE");
	}
#endif
	/* Other diagnostics? */
#endif
	pa = pmap_pte_pa(pte);

	KASSERT((*pte & PG_MANAGED) != 0,
	    ("pmap_emulate_reference(%p, 0x%lx, %d, %d): pa 0x%lx not managed",
	    curthread, v, user, write, pa));

	/*
	 * Twiddle the appropriate bits to reflect the reference
	 * and/or modification..
	 *
	 * The rules:
	 * 	(1) always mark page as used, and
	 *	(2) if it was a write fault, mark page as modified.
	 */
	if (write) {
		faultoff = PG_FOR | PG_FOE | PG_FOW;
	} else {
		faultoff = PG_FOR | PG_FOE;
	}

	*pte = (*pte & ~faultoff);
	ALPHA_TBIS(v);
}

/*
 * Miscellaneous support routines follow
 */

static void
alpha_protection_init()
{
	int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = PG_ASM;
			*up++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_ASM | PG_KRE;
			*up++ = PG_URE | PG_KRE;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = PG_ASM | PG_KWE;
			*up++ = PG_UWE | PG_KWE;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_ASM | PG_KWE | PG_KRE;
			*up++ = PG_UWE | PG_URE | PG_KWE | PG_KRE;
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
	return (void*) ALPHA_PHYS_TO_K0SEG(pa);
}

void
pmap_unmapdev(va, size)
	vm_offset_t va;
	vm_size_t size;
{
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap, addr)
	pmap_t pmap;
	vm_offset_t addr;
{
	pt_entry_t *pte;
	int val = 0;
	
	pte = pmap_lev3pte(pmap, addr);
	if (pte == 0) {
		return 0;
	}

	if (pmap_pte_v(pte)) {
		vm_page_t m;
		vm_offset_t pa;

		val = MINCORE_INCORE;
		if ((*pte & PG_MANAGED) == 0)
			return val;

		pa = pmap_pte_pa(pte);

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if ((*pte & PG_FOW) == 0)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else if (m->dirty || pmap_is_modified(m))
			val |= MINCORE_MODIFIED_OTHER;
		/*
		 * Referenced by us
		 */
		if ((*pte & (PG_FOR | PG_FOE)) == 0)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;

		/*
		 * Referenced by someone
		 */
		else if ((m->flags & PG_REFERENCED) || pmap_ts_referenced(m)) {
			val |= MINCORE_REFERENCED_OTHER;
			vm_page_flag_set(m, PG_REFERENCED);
		}
	} 
	return val;
}

void
pmap_activate(struct thread *td)
{
	pmap_t pmap;

	pmap = vmspace_pmap(td->td_proc->p_vmspace);

	critical_enter();
	if (pmap_active[PCPU_GET(cpuid)] && pmap != pmap_active[PCPU_GET(cpuid)]) {
		atomic_clear_32(&pmap_active[PCPU_GET(cpuid)]->pm_active,
				PCPU_GET(cpumask));
		pmap_active[PCPU_GET(cpuid)] = 0;
	}

	td->td_pcb->pcb_hw.apcb_ptbr =
		ALPHA_K0SEG_TO_PHYS((vm_offset_t) pmap->pm_lev1) >> PAGE_SHIFT;

	if (pmap->pm_asn[PCPU_GET(cpuid)].gen != PCPU_GET(current_asngen))
		pmap_get_asn(pmap);

	pmap_active[PCPU_GET(cpuid)] = pmap;
	atomic_set_32(&pmap->pm_active, PCPU_GET(cpumask));

	td->td_pcb->pcb_hw.apcb_asn = pmap->pm_asn[PCPU_GET(cpuid)].asn;
	critical_exit();

	if (td == curthread) {
		alpha_pal_swpctx((u_long)td->td_md.md_pcbpaddr);
	}
}

void
pmap_deactivate(struct thread *td)
{
	pmap_t pmap;

	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	atomic_clear_32(&pmap->pm_active, PCPU_GET(cpumask));
	pmap_active[PCPU_GET(cpuid)] = 0;
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

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		if (p->p_pid != pid)
			continue;

		if (p->p_vmspace) {
			int i,j;
			index = 0;
			pmap = vmspace_pmap(p->p_vmspace);
			for (i = 0; i < NPDEPG; i++) {
				pd_entry_t *pde;
				pt_entry_t *pte;
				vm_offset_t base = i << PDRSHIFT;
				
				pde = &pmap->pm_pdir[i];
				if (pde && pmap_pde_v(pde)) {
					for (j = 0; j < NPTEPG; j++) {
						vm_offset_t va = base + (j << PAGE_SHIFT);
						if (va >= (vm_offset_t) VM_MIN_KERNEL_ADDRESS) {
							if (index) {
								index = 0;
								printf("\n");
							}
							sx_sunlock(&allproc_lock);
							return npte;
						}
						pte = pmap_pte_quick(pmap, va);
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
	sx_sunlock(&allproc_lock);
	return npte;
}
#endif

#if defined(DEBUG)

static void	pads(pmap_t pm);
void		pmap_pvdump(vm_offset_t pa);

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
	for (i = 0; i < NPDEPG; i++)
		if (pm->pm_pdir[i])
			for (j = 0; j < NPTEPG; j++) {
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

void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	pv_entry_t pv;
	vm_page_t m;

	printf("pa %x", pa);
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		printf(" -> pmap %p, va %x", (void *)pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}
#endif
#endif
