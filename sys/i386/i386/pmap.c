/* 
 * Copyright (c) 1991 Regents of the University of California.
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
 *	$Id: pmap.c,v 1.14 1994/01/27 03:35:42 davidg Exp $
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
 * instructions. Jan 13, 1994.
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

#ifdef DEBUG
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
} enter_stats;
struct {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
} remove_stats;

int debugmap = 0;
int pmapdebug = 0 /* 0xffff */;
#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_PDRTAB	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

int pmapvacflush = 0;
#define	PVF_ENTER	0x01
#define	PVF_REMOVE	0x02
#define	PVF_PROTECT	0x04
#define	PVF_TOTAL	0x80
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[((vm_offset_t)(v) >> PD_SHIFT)&1023]))
#define pdir_pde(m, v) (m[((vm_offset_t)(v) >> PD_SHIFT)&1023])

#define pmap_pte_pa(pte)	(*(int *)(pte) & PG_FRAME)

#define pmap_pde_v(pte)		((pte)->pd_v)
#define pmap_pte_w(pte)		((pte)->pg_w)
/* #define pmap_pte_ci(pte)	((pte)->pg_ci) */
#define pmap_pte_m(pte)		((pte)->pg_m)
#define pmap_pte_u(pte)		((pte)->pg_u)
#define pmap_pte_v(pte)		((pte)->pg_v)
#define pmap_pte_set_w(pte, v)		((pte)->pg_w = (v))
#define pmap_pte_set_prot(pte, v)	((pte)->pg_prot = (v))

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

static inline boolean_t pmap_testbit();
static inline void pmap_changebit();
static inline int pmap_is_managed();
static inline void *vm_get_pmap();
static inline void vm_put_pmap();
static inline void pmap_use_pt();
inline struct pte *pmap_pte();
static inline pv_entry_t get_pv_entry();
void pmap_alloc_pv_entry();
void		pmap_clear_modify();
void		i386_protection_init();

#if BSDVM_COMPAT
#include "msgbuf.h"

/*
 * All those kernel PT submaps that BSD is so fond of
 */
struct pte	*CMAP1, *CMAP2, *mmap;
caddr_t		CADDR1, CADDR2, vmmap;
struct pte	*msgbufmap;
struct msgbuf	*msgbufp;
#endif

struct vm_map * pmap_fmap(pmap_t pmap) ;
void init_pv_entries(int) ;

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 * [ what about induced faults -wfj]
 */

inline struct pte *
pmap_pte(pmap, va)
	register pmap_t	pmap;
	vm_offset_t va;
{

	if (pmap && pmap_pde_v(pmap_pde(pmap, va))) {
		/* are we current address space or kernel? */
		if (pmap->pm_pdir[PTDPTDI].pd_pfnum == PTDpde.pd_pfnum
			|| pmap == kernel_pmap)
			return ((struct pte *) vtopte(va));

		/* otherwise, we are alternate address space */
		else {
			if (pmap->pm_pdir[PTDPTDI].pd_pfnum
				!= APTDpde.pd_pfnum) {
				APTDpde = pmap->pm_pdir[PTDPTDI];
				tlbflush();
			}
			return((struct pte *) avtopte(va));
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
	struct pde save;
	vm_offset_t pa;
	int s;

	s = splhigh();
	if (pmap && pmap_pde_v(pmap_pde(pmap, va))) {
		/* are we current address space or kernel? */
		if (pmap->pm_pdir[PTDPTDI].pd_pfnum == PTDpde.pd_pfnum
			|| pmap == kernel_pmap) {
			pa = *(int *) vtopte(va);
		/* otherwise, we are alternate address space */
		} else {
			if (pmap->pm_pdir[PTDPTDI].pd_pfnum
				!= APTDpde.pd_pfnum) {
				save = APTDpde;
				APTDpde = pmap->pm_pdir[PTDPTDI];
				tlbflush();
				pa = *(int *) avtopte(va);
				APTDpde = save;
				tlbflush();
			} else {
				tlbflush();
				pa = *(int *) avtopte(va);
			}
		}
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
		splx(s);
		return pa;
	}
	splx(s);
	return 0;
	
}

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
 * increment/decrement pmap wiring count
 */
static inline void
pmap_use_pt(pmap, va, use)
	pmap_t pmap;
	vm_offset_t va;
	int use;
{
	vm_offset_t pt, pa;
	pv_entry_t pv;
	vm_page_t m;

	if (va >= VM_MAX_ADDRESS)
		return; 

	pt = i386_trunc_page(vtopte(va));
	pa = pmap_extract(pmap, pt);
	if (pa == 0) {
		printf("Warning pmap_use_pt pte paging failure\n");
	}
	if (!pa || !pmap_is_managed(pa))
		return;
	pv = pa_to_pvh(pa);
	
	m = PHYS_TO_VM_PAGE(pa);
	if (use) {
		vm_page_wire(m);
	} else {
		vm_page_unwire(m);
	}
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
	struct pte *pte;
#endif
	extern vm_offset_t maxmem, physmem;
extern int IdlePTD;

	avail_start = firstaddr + DMAPAGES*NBPG;
	avail_end = maxmem << PG_SHIFT;

	/* XXX: allow for msgbuf */
	avail_end -= i386_round_page(sizeof(struct msgbuf));

	mem_size = physmem << PG_SHIFT;
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

	/* XXX: is it ok to wait here? */
	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
#ifdef notifwewait
	if (pmap == NULL)
		panic("pmap_create: cannot allocate a pmap");
#endif
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
	/* are we current address space or kernel? */
	if (pmap->pm_pdir[PTDPTDI].pd_pfnum == PTDpde.pd_pfnum
		|| pmap == kernel_pmap)
		ptp=PTmap;

	/* otherwise, we are alternate address space */
	else {
		if (pmap->pm_pdir[PTDPTDI].pd_pfnum
			!= APTDpde.pd_pfnum) {
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
	int s;
	int wired;
	s = splhigh();
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
	splx(s);
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
	vm_offset_t asva;
	vm_page_t m;
	int oldpte;

	if (pmap == NULL)
		return;

	ptp = get_pt_entry(pmap);

	
	/* this is essential since we must check the PDE(sva) for precense */
	while (sva <= eva && !pmap_pde_v(pmap_pde(pmap, sva)))
		sva = (sva & PD_MASK) + (1<<PD_SHIFT);
	sva = i386_btop(sva);
	eva = i386_btop(eva);

	for (; sva < eva; sva++) {
		/*
		 * Weed out invalid mappings.
		 * Note: we assume that the page directory table is
	 	 * always allocated, and in kernel virtual.
		 */

		if (!pmap_pde_v(pmap_pde(pmap, i386_ptob(sva))))
			{
			/* We can race ahead here, straight to next pde.. */
			sva = sva & ~((NBPG/PTESIZE) - 1);
			sva = sva + NBPG/PTESIZE - 1;
			continue;
			}

		ptq=ptp+sva;
			

		/*
		 * search for page table entries
		 */
		if (!pmap_pte_v(ptq)) {
			vm_offset_t nscan = ((sva + (NBPG/PTESIZE)) & ~((NBPG/PTESIZE) - 1)) - sva;
			if ((nscan + sva) > eva)
				nscan = eva - sva;
			if (nscan) {
				int found;

				asm("xorl %%eax,%%eax;cld;repe;scasl;jz 1f;incl %%eax;1:;"
					:"=D"(ptq),"=a"(found)
					:"c"(nscan),"0"(ptq)
					:"cx");

				if (found)
					ptq -= 1;

				sva = ptq - ptp;
			}
			if (sva >= eva)
				goto endofloop;
		}


		if (!(sva & 0x3ff)) /* Only check once in a while */
 		    {
		    	if (!pmap_pde_v(pmap_pde(pmap, i386_ptob(sva)))) {
			/* We can race ahead here, straight to next pde.. */
					sva = sva & ~((NBPG/PTESIZE) - 1);
					sva = sva + NBPG/PTESIZE - 1;
					continue;
				}
		    }

		if (!pmap_pte_v(ptq))
			continue;

		/*
		 * Update statistics
		 */
		if (pmap_pte_w(ptq))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		pa = pmap_pte_pa(ptq);
		oldpte = *(int *) ptq;

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
		*(int *)ptq = 0;

		/*
		 * Remove from the PV table (raise IPL since we
		 * may be called at interrupt time).
		 */
		if (!pmap_is_managed(pa))
			continue;
		if (oldpte & PG_M) {
			m = PHYS_TO_VM_PAGE(pa);
			m->flags &= ~PG_CLEAN;
		}
		pv = pa_to_pvh(pa);
		asva = i386_ptob(sva);
		pmap_remove_entry(pmap, pv, asva);
		pmap_use_pt(pmap, asva, 0); 
	}
endofloop:
	tlbflush();
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
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
	int s;

	/*
	 * Not one of ours
	 */
	if (!pmap_is_managed(pa))
		return;

	pa = i386_trunc_page(pa);
	pv = pa_to_pvh(pa);
	m = PHYS_TO_VM_PAGE(pa);

	while (pv->pv_pmap != NULL) {
		s = splhigh();
		pmap = pv->pv_pmap;
		ptp = get_pt_entry(pmap);
		va = i386_btop(pv->pv_va);
		pte = ptp + va;
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		if (pmap_pte_v(pte))
			pmap->pm_stats.resident_count--;

		if (*(int *)pte & PG_M) {
			m->flags &= ~PG_CLEAN;
		}

		*(int *)pte = 0;
		pmap_use_pt(pmap, pv->pv_va, 0);

		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			free_pv_entry(npv);
		} else {
			pv->pv_pmap = NULL;
		}
		
		splx(s);
	}

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

	for (va = sva; va < eva; va += PAGE_SIZE) {
		/*
		 * Page table page is not allocated.
		 * Skip it, we don't want to force allocation
		 * of unnecessary PTE pages just to set the protection.
		 */
		if (!pmap_pde_v(pmap_pde(pmap, va))) {
			/* XXX: avoid address wrap around */
			if (va >= i386_trunc_pdr((vm_offset_t)-1))
				break;
			va = i386_round_pdr(va + PAGE_SIZE) - PAGE_SIZE;
			continue;
		}

		pte = ptp + i386_btop(va);

		/*
		 * scan for a non-empty pte
		 */
		{
			int found=0;
			int svap = pte - ptp;
			vm_offset_t nscan =
				((svap + (NBPG/PTESIZE)) & ~((NBPG/PTESIZE) - 1)) - svap;
			if (nscan + svap > evap)
				nscan = evap - svap;
			if (nscan) {
				asm("xorl %%eax,%%eax;cld;repe;scasl;jz 1f;incl %%eax;1:;"
					:"=D"(pte),"=a"(found)
					:"c"(nscan),"0"(pte):"cx");

				pte -= 1;
				svap = pte - ptp;

			}
			if (svap >= evap)
				goto endofloop;
			va = i386_ptob(svap);
			if (!found)
				continue;
		}


		/*
		 * Page not valid.  Again, skip it.
		 * Should we do this?  Or set protection anyway?
		 */
		if (!pmap_pte_v(pte))
			continue;

		i386prot = pte_prot(pmap, prot);
		if (va < UPT_MAX_ADDRESS)
			i386prot |= PG_RW /*PG_u*/;
		if (i386prot != pte->pg_prot) {
			reqactivate = 1;
			pmap_pte_set_prot(pte, i386prot);
		}
	}
endofloop:
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
	register int npte;
	vm_offset_t opa;
	boolean_t cacheable = TRUE;
	boolean_t checkpv = TRUE;

	if (pmap == NULL)
		return;

	va = i386_trunc_page(va);
	pa = i386_trunc_page(pa);
	if (va > VM_MAX_KERNEL_ADDRESS)panic("pmap_enter: toobig");

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (!pmap_pde_v(pmap_pde(pmap, va))) {
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
		s = splhigh();
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
	}

	pmap_use_pt(pmap, va, 1);

	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volitile.
	 */
	if (pmap_initialized) {
		checkpv = cacheable = FALSE;
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
	npte = (pa & PG_FRAME) | pte_prot(pmap, prot) | PG_V;

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
		npte |= *(int *)pte & (PG_M|PG_U);

	if (wired)
		npte |= PG_W;
	if (va < UPT_MIN_ADDRESS)
		npte |= PG_u;
	else if (va < UPT_MAX_ADDRESS)
		npte |= PG_u | PG_RW;

	if (npte != *(int *)pte) {
		*(int *)pte = npte;
		tlbflush();
	}
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
	void pmap_copy_on_write();
        switch (prot) {
        case VM_PROT_READ:
        case VM_PROT_READ|VM_PROT_EXECUTE:
                pmap_copy_on_write(phys);
                break;
        case VM_PROT_ALL:
                break;
        default:
                pmap_remove_all(phys);
                break;
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
		pmap_pte_m(pte) = 1;
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
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
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

boolean_t
pmap_page_exists(pmap, pa)
	pmap_t pmap;
	vm_offset_t pa;
{
	register pv_entry_t pv;
	register int *pte;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splhigh();

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

static inline boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register pv_entry_t pv;
	register int *pte;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splhigh();

	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			if (bit & PG_M ) {
				if (pv->pv_va >= USRSTACK) {
					if (pv->pv_va < USRSTACK+(UPAGES*NBPG)) {
						splx(s);
						return TRUE;
					}
					else if (pv->pv_va < UPT_MAX_ADDRESS) {
						splx(s);
						return FALSE;
					}
				}
			}
			pte = (int *) pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & bit) {
				splx(s);
				return TRUE;
			}
		}
	}
	splx(s);
	return(FALSE);
}

static inline void
pmap_changebit(pa, bit, setem)
	vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	register int *pte, npte;
	vm_offset_t va;
	int s;
	int reqactivate = 0;

	if (!pmap_is_managed(pa))
		return;

	pv = pa_to_pvh(pa);
	s = splhigh();

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			va = pv->pv_va;

                        /*
                         * XXX don't write protect pager mappings
                         */
                        if (bit == PG_RO) {
                                extern vm_offset_t pager_sva, pager_eva;

                                if (va >= pager_sva && va < pager_eva)
                                        continue;
                        }

			pte = (int *) pmap_pte(pv->pv_pmap, va);
			if (setem)
				npte = *pte | bit;
			else
				npte = *pte & ~bit;
			if (*pte != npte) {
				*pte = npte;
				tlbflush();
			}
		}
	}
	splx(s);
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
	pmap_changebit(pa, PG_RO, TRUE);
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
	struct pte *ptep;

	if (pm == kernel_pmap) return;
	for (i = 0; i < 1024; i++) 
		if (pm->pm_pdir[i].pd_v)
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
