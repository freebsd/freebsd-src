/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
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
 *	$Id: pmap.c,v 1.20 1994/03/07 11:38:34 davidg Exp $
 */

/*
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	Derived from: hp300/@(#)pmap.c	7.1 (Berkeley) 12/5/90
 */
/*
 * Major modifications by John S. Dyson primarily to support
 * pageable page tables, eliminating pmap_attributes, 
 * discontiguous memory pages, and using more efficient string
 * instructions. Jan 13, 1994.  Further modifications on Mar 2, 1994,
 * general clean-up and efficiency mods. 
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

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "malloc.h"
#include "user.h"
#include "i386/include/cpufunc.h"

#include "vm/vm.h"
#include "vm/vm_kern.h"
#include "vm/vm_page.h"

#include "i386/isa/isa.h"

/*
 * Allocate various and sundry SYSMAPs used in the days of old VM
 * and not yet converted.  XXX.
 */
#define BSDVM_COMPAT	1

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[((vm_offset_t)(v) >> PD_SHIFT)&1023]))
#define pdir_pde(m, v) (m[((vm_offset_t)(v) >> PD_SHIFT)&1023])

#define pmap_pte_pa(pte)	(*(int *)(pte) & PG_FRAME)

#define pmap_pde_v(pte)		((*(int *)pte & PG_V) != 0)
#define pmap_pte_w(pte)		((*(int *)pte & PG_W) != 0)
#define pmap_pte_m(pte)		((*(int *)pte & PG_M) != 0)
#define pmap_pte_u(pte)		((*(int *)pte & PG_U) != 0)
#define pmap_pte_v(pte)		((*(int *)pte & PG_V) != 0)
			
#define pmap_pte_set_w(pte, v)		((v)?(*(int *)pte |= PG_W):(*(int *)pte &= ~PG_W))
#define pmap_pte_set_prot(pte, v)	((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
int	protection_codes[8];

struct pmap	kernel_pmap_store;
pmap_t		kernel_pmap;

vm_offset_t	phys_avail[6];	/* 2 entries + 1 null */
vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_size_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
int		i386pagesperpage;	/* PAGE_SIZE / I386_PAGE_SIZE */
boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
vm_offset_t	vm_first_phys, vm_last_phys;

static inline boolean_t		pmap_testbit();
static inline void		pmap_changebit();
static inline int		pmap_is_managed();
static inline void		*vm_get_pmap();
static inline void		vm_put_pmap();
inline void			pmap_use_pt();
inline void			pmap_unuse_pt();
inline pt_entry_t * const	pmap_pte();
static inline pv_entry_t	get_pv_entry();
void				pmap_alloc_pv_entry();
void				pmap_clear_modify();
void				i386_protection_init();

#if BSDVM_COMPAT
#include "msgbuf.h"

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1, *CMAP2, *mmap;
caddr_t		CADDR1, CADDR2, vmmap;
pt_entry_t *msgbufmap;
struct msgbuf	*msgbufp;
#endif

void init_pv_entries(int) ;

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 * [ what about induced faults -wfj]
 */

inline pt_entry_t *
const pmap_pte(pmap, va)
	register pmap_t	pmap;
	vm_offset_t va;
{

	if (pmap && *pmap_pde(pmap, va)) {
		vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;
		/* are we current address space or kernel? */
		if ( (pmap == kernel_pmap) || (frame == ((int) PTDpde & PG_FRAME)))
			return ((pt_entry_t *) vtopte(va));
		/* otherwise, we are alternate address space */
		else {
			if ( frame != ((int) APTDpde & PG_FRAME) ) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				tlbflush();
			}
			return((pt_entry_t *) avtopte(va));
		}
	}
	return(0);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t va;
{
	pd_entry_t save;
	vm_offset_t pa;
	int s;

	if (pmap && *pmap_pde(pmap, va)) {
		vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;
		/* are we current address space or kernel? */
		if ( (pmap == kernel_pmap)
			|| (frame == ((int) PTDpde & PG_FRAME)) ) {
			pa = *(int *) vtopte(va);
		/* otherwise, we are alternate address space */
		} else {
			if ( frame != ((int) APTDpde & PG_FRAME)) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				tlbflush();
			}
			pa = *(int *) avtopte(va);
		}
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
		return pa;
	}
	return 0;
	
}

/*
 * determine if a page is managed (memory vs. device)
 */
static inline int
pmap_is_managed(pa)
	vm_offset_t pa;
{
	int i;

	if (!pmap_initialized)
		return 0;

	for (i = 0; phys_avail[i + 1]; i += 2) {
		if (pa >= phys_avail[i] && pa < phys_avail[i + 1])
			return 1;
	}
	return 0;
}

/*
 * find the vm_page_t of a pte (only) given va of pte and pmap
 */
inline vm_page_t
pmap_pte_vm_page(pmap, pt)
	pmap_t pmap;
	vm_offset_t pt;
{
	pt = i386_trunc_page( pt);
	pt = (pt - UPT_MIN_ADDRESS) / NBPG;
	pt = ((vm_offset_t) pmap->pm_pdir[pt]) & PG_FRAME;
	return PHYS_TO_VM_PAGE(pt);
}

/*
 * Wire a page table page
 */
inline void
pmap_use_pt(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	vm_offset_t pt;

	if (va >= VM_MAX_ADDRESS || !pmap_initialized)
		return; 

	pt = (vm_offset_t) vtopte(va);
	/* vm_page_wire( pmap_pte_vm_page(pmap, pt)); */
	vm_page_hold( pmap_pte_vm_page(pmap, pt));
}

/*
 * Unwire a page table page
 */
inline void
pmap_unuse_pt(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	vm_offset_t pt;

	if (va >= VM_MAX_ADDRESS || !pmap_initialized)
		return; 

	pt = (vm_offset_t) vtopte(va);
/*	vm_page_unwire( pmap_pte_vm_page(pmap, pt)); */
	vm_page_unhold( pmap_pte_vm_page(pmap, pt));
}

/* [ macro again?, should I force kstack into user map here? -wfj ] */
void
pmap_activate(pmap, pcbp)
	register pmap_t pmap;
	struct pcb *pcbp;
{
	PMAP_ACTIVATE(pmap, pcbp);
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *
 *	On the I386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */

#define DMAPAGES 8
void
pmap_bootstrap(firstaddr, loadaddr)
	vm_offset_t firstaddr;
	vm_offset_t loadaddr;
{
#if BSDVM_COMPAT
	vm_offset_t va;
	pt_entry_t *pte;
#endif
	extern int IdlePTD;

	avail_start = firstaddr + DMAPAGES*NBPG;

	virtual_avail = (vm_offset_t) KERNBASE + avail_start;
	virtual_end = VM_MAX_KERNEL_ADDRESS;
	i386pagesperpage = PAGE_SIZE / NBPG;

	/*
	 * Initialize protection array.
	 */
	i386_protection_init();

	/*
	 * The kernel's pmap is statically allocated so we don't
	 * have to use pmap_create, which is unlikely to work
	 * correctly at this part of the boot sequence.
	 */
	kernel_pmap = &kernel_pmap_store;

	kernel_pmap->pm_pdir = (pd_entry_t *)(KERNBASE + IdlePTD);

	simple_lock_init(&kernel_pmap->pm_lock);
	kernel_pmap->pm_count = 1;

#if BSDVM_COMPAT
	/*
	 * Allocate all the submaps we need
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*NBPG); p = pte; pte += (n);

	va = virtual_avail;
	pte = pmap_pte(kernel_pmap, va);

	SYSMAP(caddr_t		,CMAP1		,CADDR1	   ,1		)
	SYSMAP(caddr_t		,CMAP2		,CADDR2	   ,1		)
	SYSMAP(caddr_t		,mmap		,vmmap	   ,1		)
	SYSMAP(struct msgbuf *	,msgbufmap	,msgbufp   ,1		)
	virtual_avail = va;
#endif
	/*
	 * reserve special hunk of memory for use by bus dma as a bounce
	 * buffer (contiguous virtual *and* physical memory). for now,
	 * assume vm does not use memory beneath hole, and we know that
	 * the bootstrap uses top 32k of base memory. -wfj
	 */
	{
		extern vm_offset_t isaphysmem;
		isaphysmem = va;

		virtual_avail = pmap_map(va, firstaddr,
				firstaddr + DMAPAGES*NBPG, VM_PROT_ALL);
	}

	*(int *)PTD = 0;
	tlbflush();

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
	vm_offset_t	phys_start, phys_end;
{
	vm_offset_t	addr, addr2;
	vm_size_t	npg, s;
	int		rv;
	int i;
	extern int KPTphys;
	extern int IdlePTD;

	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in locore.
	 */
	addr = atdevbase;
	(void) vm_map_find(kernel_map, NULL, (vm_offset_t) 0,
			   &addr, (0x100000-0xa0000), FALSE);

	addr = (vm_offset_t) KERNBASE + IdlePTD;
	vm_object_reference(kernel_object);
	(void) vm_map_find(kernel_map, kernel_object, addr,
			   &addr, (4 + NKPT) * NBPG, FALSE);


	/*
	 * calculate the number of pv_entries needed
	 */
	vm_first_phys = phys_avail[0];
	for (i = 0; phys_avail[i + 1]; i += 2) ;
	npg = (phys_avail[(i - 2) + 1] - vm_first_phys) / NBPG;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */
	s = (vm_size_t) (sizeof(struct pv_entry) * npg);
	s = i386_round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	pv_table = (pv_entry_t) addr;

	/*
	 * init the pv free list
	 */
	init_pv_entries(npg);
	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(virt, start, end, prot)
	vm_offset_t	virt;
	vm_offset_t	start;
	vm_offset_t	end;
	int		prot;
{
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return(virt);
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 *
 * [ just allocate a ptd and mark it uninitialize -- should we track
 *   with a table which process has which ptd? -wfj ]
 */

pmap_t
pmap_create(size)
	vm_size_t	size;
{
	register pmap_t pmap;

	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return(NULL);

	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(*pmap));
	pmap_pinit(pmap);
	return (pmap);
}


struct pmaplist {
	struct pmaplist *next;
};

static inline void *
vm_get_pmap()
{
	struct pmaplist *rtval;

	rtval = (struct pmaplist *)kmem_alloc(kernel_map, ctob(1));
	bzero(rtval, ctob(1));
	return rtval;
}

static inline void
vm_put_pmap(up)
	struct pmaplist *up;
{
	kmem_free(kernel_map, up, ctob(1));
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	/*
	 * No need to allocate page table space yet but we do need a
	 * valid page directory table.
	 */
	pmap->pm_pdir = (pd_entry_t *) vm_get_pmap();

	/* wire in kernel global address entries */
	bcopy(PTD+KPTDI, pmap->pm_pdir+KPTDI, NKPT*PTESIZE);

	/* install self-referential address mapping entry */
	*(int *)(pmap->pm_pdir+PTDPTDI) =
		((int)pmap_extract(kernel_pmap, (vm_offset_t)pmap->pm_pdir)) | PG_V | PG_KW;

	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	register pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	register struct pmap *pmap;
{
	vm_put_pmap((struct pmaplist *) pmap->pm_pdir);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{
	if (pmap != NULL) {
		simple_lock(&pmap->pm_lock);
		pmap->pm_count++;
		simple_unlock(&pmap->pm_lock);
	}
}

#define PV_FREELIST_MIN ((NBPG / sizeof (struct pv_entry)) / 2)

/*
 * Data for the pv entry allocation mechanism
 */
int pv_freelistcnt;
pv_entry_t pv_freelist;
vm_offset_t pvva;
int npvvapg;

/*
 * free the pv_entry back to the free list
 */
inline static void
free_pv_entry(pv)
	pv_entry_t pv;
{
	if (!pv) return;
	++pv_freelistcnt;
	pv->pv_next = pv_freelist;
	pv_freelist = pv;
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static inline pv_entry_t
get_pv_entry()
{
	pv_entry_t tmp;

	/*
	 * get more pv_entry pages if needed
	 */
	while (pv_freelistcnt < PV_FREELIST_MIN || pv_freelist == 0) { 
		pmap_alloc_pv_entry();
	}

	/*
	 * get a pv_entry off of the free list
	 */
	--pv_freelistcnt;
	tmp = pv_freelist;
	pv_freelist = tmp->pv_next;
	tmp->pv_pmap = 0;
	tmp->pv_va = 0;
	tmp->pv_next = 0;
	return tmp;
}

/*
 * this *strange* allocation routine *statistically* eliminates the
 * *possibility* of a malloc failure (*FATAL*) for a pv_entry_t data structure.
 * also -- this code is MUCH MUCH faster than the malloc equiv...
 */
void
pmap_alloc_pv_entry()
{
	/*
	 * do we have any pre-allocated map-pages left?
	 */
	if (npvvapg) {
		vm_page_t m;
		/*
		 * we do this to keep recursion away
		 */
		pv_freelistcnt += PV_FREELIST_MIN;
		/*
		 * allocate a physical page out of the vm system
		 */
		if (m = vm_page_alloc(kernel_object, pvva-vm_map_min(kernel_map))) {
			int newentries;
			int i;
			pv_entry_t entry;
			newentries = (NBPG/sizeof (struct pv_entry));
			/*
			 * wire the page
			 */
			vm_page_wire(m);
			m->flags &= ~PG_BUSY;
			/*
			 * let the kernel see it
			 */
			pmap_enter(vm_map_pmap(kernel_map), pvva,
				VM_PAGE_TO_PHYS(m), VM_PROT_DEFAULT,1);

			entry = (pv_entry_t) pvva;
			/*
			 * update the allocation pointers
			 */
			pvva += NBPG;
			--npvvapg;

			/*
			 * free the entries into the free list
			 */
			for (i = 0; i < newentries; i++) {
				free_pv_entry(entry);
				entry++;
			}
		}
		pv_freelistcnt -= PV_FREELIST_MIN;
	}
	if (!pv_freelist)
		panic("get_pv_entry: cannot get a pv_entry_t");
}



/*
 * init the pv_entry allocation system
 */
#define PVSPERPAGE 16
void
init_pv_entries(npg)
	int npg;
{
	/*
	 * allocate enough kvm space for PVSPERPAGE entries per page (lots)
	 * kvm space is fairly cheap, be generous!!!  (the system can panic
	 * if this is too small.)
	 */
	npvvapg = ((npg*PVSPERPAGE) * sizeof(struct pv_entry) + NBPG - 1)/NBPG;
	pvva = kmem_alloc_pageable(kernel_map, npvvapg * NBPG);
	/*
	 * get the first batch of entries
	 */
	free_pv_entry(get_pv_entry());
}

static pt_entry_t *
get_pt_entry(pmap)
	pmap_t pmap;
{
	pt_entry_t *ptp;
	vm_offset_t frame = (int) pmap->pm_pdir[PTDPTDI] & PG_FRAME;
	/* are we current address space or kernel? */
	if (pmap == kernel_pmap || frame == ((int) PTDpde & PG_FRAME)) {
		ptp=PTmap;
	/* otherwise, we are alternate address space */
	} else {
		if ( frame != ((int) APTDpde & PG_FRAME)) {
			APTDpde = pmap->pm_pdir[PTDPTDI];
			tlbflush();
		}
		ptp=APTmap;
	     }
	return ptp;
}

/*
 * If it is the first entry on the list, it is actually
 * in the header and we must copy the following entry up
 * to the header.  Otherwise we must search the list for
 * the entry.  In either case we free the now unused entry.
 */
void
pmap_remove_entry(pmap, pv, va)
	struct pmap *pmap;
	pv_entry_t pv;
	vm_offset_t va;
{
	pv_entry_t npv;
	int wired;
	disable_intr();
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			free_pv_entry(npv);
		} else {
			pv->pv_pmap = NULL;
		}
	} else {
		for (npv = pv->pv_next; npv; npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
				break;
			}
			pv = npv;
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
			free_pv_entry(npv);
		} 
	}
	enable_intr();
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	register vm_offset_t sva;
	register vm_offset_t eva;
{
	register pt_entry_t *ptp,*ptq;
	vm_offset_t pa;
	register pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m;
	pt_entry_t oldpte;
	int reqactivate = 0;

	if (pmap == NULL)
		return;

	ptp = get_pt_entry(pmap);

/*
 * special handling of removing one page.  a very
 * common operation and easy to short circuit some
 * code.
 */
	if( (sva + NBPG) == eva) {
		
		if( *pmap_pde( pmap, sva) == 0)
			return;

		ptq = ptp + i386_btop(sva);

		if( !*ptq)
			return;
		/*
		 * Update statistics
		 */
		if (pmap_pte_w(ptq))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		pa = pmap_pte_pa(ptq);
		oldpte = *ptq;
		*ptq = 0;

		if (pmap_is_managed(pa)) {
			if ((((int) oldpte & PG_M) && (sva < USRSTACK || sva > UPT_MAX_ADDRESS))
				|| (sva >= USRSTACK && sva < USRSTACK+(UPAGES*NBPG))) {
				m = PHYS_TO_VM_PAGE(pa);
				m->flags &= ~PG_CLEAN;
			}

			pv = pa_to_pvh(pa);
			pmap_remove_entry(pmap, pv, sva);
			pmap_unuse_pt(pmap, sva); 
		}
		/*
		 * Pageout daemon is the process that calls pmap_remove
		 * most often when the page is not owned by the current
		 * process. there are slightly more accurate checks, but
		 * they are not nearly as fast.
		 */
		if( (curproc != pageproc) || (pmap == kernel_pmap))
			tlbflush();
		return;
	}
	
	sva = i386_btop(sva);
	eva = i386_btop(eva);

	while (sva < eva) {
		/*
		 * Weed out invalid mappings.
		 * Note: we assume that the page directory table is
	 	 * always allocated, and in kernel virtual.
		 */

		if ( *pmap_pde(pmap, i386_ptob(sva)) == 0 ) {
			/* We can race ahead here, straight to next pde.. */
	nextpde:
			sva = ((sva + NPTEPG) & ~(NPTEPG - 1));
			continue;
		}

		ptq = ptp + sva;

		/*
		 * search for page table entries, use string operations
		 * that are much faster than
		 * explicitly scanning when page tables are not fully
		 * populated.
		 */
		if ( *ptq == 0) {
			vm_offset_t pdnxt = ((sva + NPTEPG) & ~(NPTEPG - 1));
			vm_offset_t nscan = pdnxt - sva;
			int found = 0;

			if ((nscan + sva) > eva)
				nscan = eva - sva;

			asm("xorl %%eax,%%eax;cld;repe;scasl;jz 1f;incl %%eax;1:;"
				:"=D"(ptq),"=a"(found)
				:"c"(nscan),"0"(ptq)
				:"cx");

			if( !found) {
				sva = pdnxt;
				continue;
			}
			ptq -= 1;

			sva = ptq - ptp;
		}

		/*
		 * Update statistics
		 */
		oldpte = *ptq;
		if (((int)oldpte) & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
		*ptq = 0;

		/*
		 * Remove from the PV table (raise IPL since we
		 * may be called at interrupt time).
		 */
		pa = ((int)oldpte) & PG_FRAME;
		if (!pmap_is_managed(pa)) {
			++sva;
			continue;
		}

		va = i386_ptob(sva);

		if ((((int) oldpte & PG_M) && (va < USRSTACK || va > UPT_MAX_ADDRESS))
			|| (va >= USRSTACK && va < USRSTACK+(UPAGES*NBPG))) {
			m = PHYS_TO_VM_PAGE(pa);
			m->flags &= ~PG_CLEAN;
		}

		pv = pa_to_pvh(pa);
		pmap_remove_entry(pmap, pv, va);
		pmap_unuse_pt(pmap, va); 
		++sva;
		reqactivate = 1;
	}
endofloop:
	/*
	 * only call tlbflush if the pmap has changed and the tlb
	 * *really* needs to be updated.
	 */
	if( reqactivate &&
		(curproc != pageproc) || (pmap == kernel_pmap))
		tlbflush();
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
pmap_remove_all(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv, npv;
	register pt_entry_t *pte, *ptp;
	vm_offset_t va;
	struct pmap *pmap;
	struct map *map;
	vm_page_t m;
	int rqactivate = 0;
	int s;

	/*
	 * Not one of ours
	 */
	if (!pmap_is_managed(pa))
		return;

	pa = i386_trunc_page(pa);
	pv = pa_to_pvh(pa);
	m = PHYS_TO_VM_PAGE(pa);

	s = splimp();
	while (pv->pv_pmap != NULL) {
		pmap = pv->pv_pmap;
		ptp = get_pt_entry(pmap);
		va = i386_btop(pv->pv_va);
		pte = ptp + va;
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		if ( *pte)
			pmap->pm_stats.resident_count--;
		
		/*
		 * update the vm_page_t clean bit
		 */
		if ( (m->flags & PG_CLEAN) &&
			((((int) *pte) & PG_M) && (pv->pv_va < USRSTACK || pv->pv_va > UPT_MAX_ADDRESS))
			|| (pv->pv_va >= USRSTACK && pv->pv_va < USRSTACK+(UPAGES*NBPG))) {
			m->flags &= ~PG_CLEAN;
		}

		*pte = 0;
		pmap_unuse_pt(pmap, pv->pv_va);

		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			free_pv_entry(npv);
		} else {
			pv->pv_pmap = NULL;
		}
		if( (curproc != pageproc) || (pmap == kernel_pmap))
			rqactivate = 1;
	}
	splx(s);

	if( rqactivate)
		tlbflush();
}


/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t	pmap;
	vm_offset_t	sva, eva;
	vm_prot_t	prot;
{
	register pt_entry_t *pte;
	register vm_offset_t va;
	int i386prot;
	register pt_entry_t *ptp;
	int reqactivate = 0;
	int evap = i386_btop(eva);
	int s;

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	ptp = get_pt_entry(pmap);

	va = sva;
	while (va < eva) {
		int found=0;
		int svap;
		vm_offset_t nscan;
		/*
		 * Page table page is not allocated.
		 * Skip it, we don't want to force allocation
		 * of unnecessary PTE pages just to set the protection.
		 */
		if (! *pmap_pde(pmap, va)) {
			/* XXX: avoid address wrap around */
nextpde:
			if (va >= i386_trunc_pdr((vm_offset_t)-1))
				break;
			va = i386_round_pdr(va + PAGE_SIZE);
			continue;
		}

		pte = ptp + i386_btop(va);

		if( *pte == 0) {
		/*
		 * scan for a non-empty pte
		 */
			svap = pte - ptp;
			nscan = ((svap + NPTEPG) & ~(NPTEPG - 1)) - svap;

			if (nscan + svap > evap)
				nscan = evap - svap;

			found = 0;
			if (nscan)
				asm("xorl %%eax,%%eax;cld;repe;scasl;jz 1f;incl %%eax;1:;"
					:"=D"(pte),"=a"(found)
					:"c"(nscan),"0"(pte):"cx");

			if( !found)
				goto nextpde;

			pte -= 1;
			svap = pte - ptp;

			va = i386_ptob(svap);
		}

		i386prot = pte_prot(pmap, prot);
		if (va < UPT_MAX_ADDRESS) {
			i386prot |= PG_u;
			if( va >= UPT_MIN_ADDRESS)
				i386prot |= PG_RW;
		}
		if (i386prot != ( (int) *pte & PG_PROT)) {
			reqactivate = 1;
			pmap_pte_set_prot(pte, i386prot);
		}
		va += PAGE_SIZE;
	}
endofloop:
	/*
	 * only if pte changed
	 */
	if( reqactivate)
		tlbflush();
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
pmap_enter(pmap, va, pa, prot, wired)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register pt_entry_t *pte;
	register pt_entry_t npte;
	vm_offset_t opa;
	int cacheable=1;

	if (pmap == NULL)
		return;

	va = i386_trunc_page(va);
	pa = i386_trunc_page(pa);
	if (va > VM_MAX_KERNEL_ADDRESS)panic("pmap_enter: toobig");

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if ( *pmap_pde(pmap, va) == 0) {
		pg("ptdi %x, va %x", pmap->pm_pdir[PTDPTDI], va);
	}

	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
		}
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		pmap_remove(pmap, va, va + PAGE_SIZE);
	}

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */
	if (pmap_is_managed(pa)) {
		register pv_entry_t pv, npv;
		int s;

		pv = pa_to_pvh(pa);
		s = splimp();
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
			npv = get_pv_entry();
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			pv->pv_next = npv;
		}
		splx(s); 
		cacheable = 1;
	} else {
		cacheable = 0;
	}

	pmap_use_pt(pmap, va);

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
	npte = (pt_entry_t) ( (int) (pa | pte_prot(pmap, prot) | PG_V));
	/*
	 * for correctness:
	 */
	if( !cacheable)
		(int) npte |= PG_N;

	/*
	 * When forking (copy-on-write, etc):
	 * A process will turn off write permissions for any of its writable
	 * pages.  If the data (object) is only referred to by one process, the
	 * processes map is modified directly as opposed to using the
	 * object manipulation routine.  When using pmap_protect, the
	 * modified bits are not kept in the vm_page_t data structure.  
	 * Therefore, when using pmap_enter in vm_fault to bring back
	 * writability of a page, there has been no memory of the
	 * modified or referenced bits except at the pte level.  
	 * this clause supports the carryover of the modified and
	 * used (referenced) bits.
	 */
	if (pa == opa)
		(int) npte |= (int) *pte & (PG_M|PG_U);


	if (wired)
		(int) npte |= PG_W;
	if (va < UPT_MIN_ADDRESS)
		(int) npte |= PG_u;
	else if (va < UPT_MAX_ADDRESS)
		(int) npte |= PG_u | PG_RW;

	/*
	 * only if pte changed
	 */
	if ((int) npte != (int) *pte) {
		*pte = npte;
		tlbflush();
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

static inline void
pmap_enter_quick(pmap, va, pa)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
{
	register pt_entry_t *pte;
	register pv_entry_t pv, npv;
	int s;

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */

	pte = vtopte(va);
	if (pmap_pte_pa(pte)) {
		pmap_remove(pmap, va, va + PAGE_SIZE);
	}

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * No entries yet, use header as the first entry
	 */
	if (pv->pv_pmap == NULL) {
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
	}
	/*
	 * There is at least one other VA mapping this page.
	 * Place this entry after the header.
	 */
	else {
		npv = get_pv_entry();
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
	}
	splx(s); 

	pmap_use_pt(pmap, va); 

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

validate:

	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	*pte = (pt_entry_t) ( (int) (pa | PG_RO | PG_V | PG_u));
}

/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap, addr, object, offset, size)
	pmap_t pmap;
	vm_offset_t addr;
	vm_object_t object;
	vm_offset_t offset;
	vm_offset_t size;
{

	vm_offset_t tmpoff;
	vm_page_t p;
	int s;
	vm_offset_t v, lastv=0;
	pt_entry_t pte;
	extern vm_map_t kernel_map;
	vm_offset_t objbytes;

	if (!pmap)
		return;

	/*
	 * if we are processing a major portion of the object, then
	 * scan the entire thing.
	 */
	if( size > object->size / 2) {
		objbytes = size;
		p = (vm_page_t) queue_first(&object->memq);
		while (!queue_end(&object->memq, (queue_entry_t) p) && objbytes != 0) {
			tmpoff = p->offset;
			if( tmpoff < offset) {
				p = (vm_page_t) queue_next(&p->listq);
				continue;
			}
			tmpoff -= offset;
			if( tmpoff >= size) {
				p = (vm_page_t) queue_next(&p->listq);
				continue;
			}
			
			if ((p->flags & (PG_BUSY|PG_FICTITIOUS)) == 0 ) {
				vm_page_hold(p);
				v = i386_trunc_page(((vm_offset_t)vtopte( addr+tmpoff)));
				/* a fault might occur here */
				*(volatile char *)v += 0;
				vm_page_unhold(p);
				pmap_enter_quick(pmap, addr+tmpoff, VM_PAGE_TO_PHYS(p));
			}
			p = (vm_page_t) queue_next(&p->listq);
			objbytes -= NBPG;
		}
	} else {
	/*
	 * else lookup the pages one-by-one.
	 */
		for(tmpoff = 0; tmpoff < size; tmpoff += NBPG) {
			if( p = vm_page_lookup(object, tmpoff + offset)) {
				if( (p->flags & (PG_BUSY|PG_FICTITIOUS)) == 0) {
					vm_page_hold(p);
					v = i386_trunc_page(((vm_offset_t)vtopte( addr+tmpoff)));
					/* a fault might occur here */
					*(volatile char *)v += 0;
					vm_page_unhold(p);
					pmap_enter_quick(pmap, addr+tmpoff, VM_PAGE_TO_PHYS(p));
				}
			}
		}
	}

	tlbflush();
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
	register pmap_t	pmap;
	vm_offset_t	va;
	boolean_t	wired;
{
	register pt_entry_t *pte;

	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);
	if (wired && !pmap_pte_w(pte) || !wired && pmap_pte_w(pte)) {
		if (wired)
			pmap->pm_stats.wired_count++;
		else
			pmap->pm_stats.wired_count--;
	}
	/*
	 * Wiring is not a hardware characteristic so there is no need
	 * to invalidate TLB.
	 */
	pmap_pte_set_w(pte, wired);
	/*
 	 * When unwiring, set the modified bit in the pte -- could have
	 * been changed by the kernel
 	 */
	if (!wired)
		(int) *pte |= PG_M;
}



/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap, src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
}
/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void
pmap_update()
{
	tlbflush();
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
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(phys)
	vm_offset_t phys;
{
	*(int *)CMAP2 = PG_V | PG_KW | i386_trunc_page(phys);
	tlbflush();
	bzero(CADDR2,NBPG);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t src;
	vm_offset_t dst;
{
	*(int *)CMAP1 = PG_V | PG_KW | i386_trunc_page(src);
	*(int *)CMAP2 = PG_V | PG_KW | i386_trunc_page(dst);
	tlbflush();

#if __GNUC__ > 1
	memcpy(CADDR2, CADDR1, NBPG);
#else
	bcopy(CADDR1, CADDR2, NBPG); 
#endif
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
	pmap_t		pmap;
	vm_offset_t	sva, eva;
	boolean_t	pageable;
{
}

/*
 * this routine returns true if a physical page resides
 * in the given pmap.
 */
boolean_t
pmap_page_exists(pmap, pa)
	pmap_t pmap;
	vm_offset_t pa;
{
	register pv_entry_t pv;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splimp();

	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			if (pv->pv_pmap == pmap) {
				splx(s);
				return TRUE;
			}
		}
	}
	splx(s);
	return(FALSE);
}

/*
 * pmap_testbit tests bits in pte's
 * note that the testbit/changebit routines are inline,
 * and a lot of things compile-time evaluate.
 */
static inline boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register pv_entry_t pv;
	pt_entry_t *pte;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	disable_intr();

	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			/*
			 * if the bit being tested is the modified bit,
			 * then mark UPAGES as always modified, and
			 * ptes as never modified.
			 */
			if (bit & PG_M ) {
				if (pv->pv_va >= USRSTACK) {
					if (pv->pv_va < USRSTACK+(UPAGES*NBPG)) {
						enable_intr();
						return TRUE;
					}
					else if (pv->pv_va < UPT_MAX_ADDRESS) {
						enable_intr();
						return FALSE;
					}
				}
			}
			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
			if ((int) *pte & bit) {
				enable_intr();
				return TRUE;
			}
		}
	}
	enable_intr();
	return(FALSE);
}

/*
 * this routine is used to modify bits in ptes
 */
static inline void
pmap_changebit(pa, bit, setem)
	vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	register pt_entry_t *pte, npte;
	vm_offset_t va;
	int s;
	int reqactivate = 0;

	if (!pmap_is_managed(pa))
		return;

	pv = pa_to_pvh(pa);
	disable_intr();

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			va = pv->pv_va;

			/*
			 * don't write protect pager mappings
			 */
			if (!setem && (bit == PG_RW)) {
				extern vm_offset_t pager_sva, pager_eva;

				if (va >= pager_sva && va < pager_eva)
					continue;
			}

			pte = pmap_pte(pv->pv_pmap, va);
			if (setem)
				(int) npte = (int) *pte | bit;
			else
				(int) npte = (int) *pte & ~bit;
			if (*pte != npte) {
				*pte = npte;
				reqactivate = 1;
			}
		}
	}
	enable_intr();
	/*
	 * tlbflush only if we need to
	 */
	if( reqactivate && (curproc != pageproc))
		tlbflush();
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(phys, prot)
        vm_offset_t     phys;
        vm_prot_t       prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
			pmap_changebit(phys, PG_RW, FALSE);
		else
			pmap_remove_all(phys);
	}
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(pa)
	vm_offset_t	pa;
{
	pmap_changebit(pa, PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(pa)
	vm_offset_t	pa;
{
	pmap_changebit(pa, PG_U, FALSE);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t
pmap_is_referenced(pa)
	vm_offset_t	pa;
{
	return(pmap_testbit(pa, PG_U));
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t
pmap_is_modified(pa)
	vm_offset_t	pa;
{
	return(pmap_testbit(pa, PG_M));
}

/*
 *	Routine:	pmap_copy_on_write
 *	Function:
 *		Remove write privileges from all
 *		physical maps for this physical page.
 */
void
pmap_copy_on_write(pa)
	vm_offset_t pa;
{
	pmap_changebit(pa, PG_RW, FALSE);
}


vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(i386_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

void
i386_protection_init()
{
	register int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_RO;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
			break;
		}
	}
}

#ifdef DEBUG
void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;

	printf("pa %x", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next) {
		printf(" -> pmap %x, va %x, flags %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_flags);
		pads(pv->pv_pmap);
	}
	printf(" ");
}

/* print address space of pmap*/
void
pads(pm)
	pmap_t pm;
{
	unsigned va, i, j;
	pt_entry_t *ptep;

	if (pm == kernel_pmap) return;
	for (i = 0; i < 1024; i++) 
		if (pm->pm_pdir[i])
			for (j = 0; j < 1024 ; j++) {
				va = (i<<PD_SHIFT)+(j<<PG_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
						continue;
				if (pm != kernel_pmap && va > UPT_MAX_ADDRESS)
						continue;
				ptep = pmap_pte(pm, va);
				if (pmap_pte_v(ptep)) 
					printf("%x:%x ", va, *(int *)ptep); 
			} ;
				
}
#endif
