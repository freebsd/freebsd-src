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
 *	@(#)pmap.c	7.7 (Berkeley)	5/12/91
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

#include "opt_msgbuf.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#define PMAP_KEEP_PDIRS
#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#if defined(DIAGNOSTIC)
#define PMAP_DIAGNOSTIC
#endif

#define MINPV 2048

#if !defined(PMAP_DIAGNOSTIC)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
static pt_entry_t protection_codes[8];

struct pmap kernel_pmap_store;
LIST_HEAD(pmaplist, pmap);
static struct pmaplist allpmaps;
static struct mtx allpmaps_lock;

vm_paddr_t avail_start;		/* PA of first available physical page */
vm_paddr_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */

static int nkpt;
static int ndmpdp;
static vm_paddr_t dmaplimit;
vm_offset_t kernel_vm_end;

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
static u_int64_t	KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */

static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static struct vm_object pvzone_obj;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
int pmap_pagedaemon_waken;

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1 = 0;
static pt_entry_t *ptmmap;
caddr_t CADDR1 = 0, ptvmmap = 0;
static pt_entry_t *msgbufmap;
struct msgbuf *msgbufp = 0;

/*
 * Crashdump maps.
 */
static pt_entry_t *pt_crashdumpmap;
static caddr_t crashdumpmap;

static PMAP_INLINE void	free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(void);
static void	amd64_protection_init(void);
static void	pmap_changebit(vm_page_t m, int bit, boolean_t setem)
    __always_inline;

static int pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t sva);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va);
static int pmap_remove_entry(struct pmap *pmap, vm_page_t m,
					vm_offset_t va);
static void pmap_insert_entry(pmap_t pmap, vm_offset_t va,
		vm_page_t mpte, vm_page_t m);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va);

static vm_page_t _pmap_allocpte(pmap_t pmap, vm_pindex_t ptepindex);
static vm_page_t pmap_page_lookup(vm_object_t object, vm_pindex_t pindex);
static int pmap_unuse_pt(pmap_t, vm_offset_t, vm_page_t);
static vm_offset_t pmap_kmem_choose(vm_offset_t addr);
static void *pmap_pv_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait);

CTASSERT(1 << PDESHIFT == sizeof(pd_entry_t));
CTASSERT(1 << PTESHIFT == sizeof(pt_entry_t));

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
	return newaddr;
}

/********************/
/* Inline functions */
/********************/

/* Return a non-clipped PD index for a given VA */
static __inline vm_pindex_t
pmap_pde_pindex(vm_offset_t va)
{
	return va >> PDRSHIFT;
}


/* Return various clipped indexes for a given VA */
static __inline vm_pindex_t
pmap_pte_index(vm_offset_t va)
{

	return ((va >> PAGE_SHIFT) & ((1ul << NPTEPGSHIFT) - 1));
}

static __inline vm_pindex_t
pmap_pde_index(vm_offset_t va)
{

	return ((va >> PDRSHIFT) & ((1ul << NPDEPGSHIFT) - 1));
}

static __inline vm_pindex_t
pmap_pdpe_index(vm_offset_t va)
{

	return ((va >> PDPSHIFT) & ((1ul << NPDPEPGSHIFT) - 1));
}

static __inline vm_pindex_t
pmap_pml4e_index(vm_offset_t va)
{

	return ((va >> PML4SHIFT) & ((1ul << NPML4EPGSHIFT) - 1));
}

/* Return a pointer to the PML4 slot that corresponds to a VA */
static __inline pml4_entry_t *
pmap_pml4e(pmap_t pmap, vm_offset_t va)
{

	if (!pmap)
		return NULL;
	return (&pmap->pm_pml4[pmap_pml4e_index(va)]);
}

/* Return a pointer to the PDP slot that corresponds to a VA */
static __inline pdp_entry_t *
pmap_pdpe(pmap_t pmap, vm_offset_t va)
{
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;

	pml4e = pmap_pml4e(pmap, va);
	if (pml4e == NULL || (*pml4e & PG_V) == 0)
		return NULL;
	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(*pml4e & PG_FRAME);
	return (&pdpe[pmap_pdpe_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pd_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t va)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;

	pdpe = pmap_pdpe(pmap, va);
	if (pdpe == NULL || (*pdpe & PG_V) == 0)
		 return NULL;
	pde = (pd_entry_t *)PHYS_TO_DMAP(*pdpe & PG_FRAME);
	return (&pde[pmap_pde_index(va)]);
}

/* Return a pointer to the PT slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t *pte;

	pde = pmap_pde(pmap, va);
	if (pde == NULL || (*pde & PG_V) == 0)
		return NULL;
	if ((*pde & PG_PS) != 0)	/* compat with i386 pmap_pte() */
		return ((pt_entry_t *)pde);
	pte = (pt_entry_t *)PHYS_TO_DMAP(*pde & PG_FRAME);
	return (&pte[pmap_pte_index(va)]);
}


PMAP_INLINE pt_entry_t *
vtopte(vm_offset_t va)
{
	u_int64_t mask = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT + NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1);

	return (PTmap + (amd64_btop(va) & mask));
}

static u_int64_t
allocpages(int n)
{
	u_int64_t ret;

	ret = avail_start;
	bzero((void *)ret, n * PAGE_SIZE);
	avail_start += n * PAGE_SIZE;
	return (ret);
}

static void
create_pagetables(void)
{
	int i;

	/* Allocate pages */
	KPTphys = allocpages(NKPT);
	KPML4phys = allocpages(1);
	KPDPphys = allocpages(NKPML4E);
	KPDphys = allocpages(NKPDPE);

	ndmpdp = (ptoa(Maxmem) + NBPDP - 1) >> PDPSHIFT;
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	DMPDPphys = allocpages(NDMPML4E);
	DMPDphys = allocpages(ndmpdp);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;

	/* Fill in the underlying page table pages */
	/* Read-only from zero to physfree */
	/* XXX not fully used, underneath 2M pages */
	for (i = 0; (i << PAGE_SHIFT) < avail_start; i++) {
		((pt_entry_t *)KPTphys)[i] = i << PAGE_SHIFT;
		((pt_entry_t *)KPTphys)[i] |= PG_RW | PG_V;
	}

	/* Now map the page tables at their location within PTmap */
	for (i = 0; i < NKPT; i++) {
		((pd_entry_t *)KPDphys)[i] = KPTphys + (i << PAGE_SHIFT);
		((pd_entry_t *)KPDphys)[i] |= PG_RW | PG_V;
	}

	/* Map from zero to end of allocations under 2M pages */
	/* This replaces some of the KPTphys entries above */
	for (i = 0; (i << PDRSHIFT) < avail_start; i++) {
		((pd_entry_t *)KPDphys)[i] = i << PDRSHIFT;
		((pd_entry_t *)KPDphys)[i] |= PG_RW | PG_V | PG_PS;
	}

	/* And connect up the PD to the PDP */
	for (i = 0; i < NKPDPE; i++) {
		((pdp_entry_t *)KPDPphys)[i + KPDPI] = KPDphys + (i << PAGE_SHIFT);
		((pdp_entry_t *)KPDPphys)[i + KPDPI] |= PG_RW | PG_V | PG_U;
	}


	/* Now set up the direct map space using 2MB pages */
	for (i = 0; i < NPDEPG * ndmpdp; i++) {
		((pd_entry_t *)DMPDphys)[i] = (vm_paddr_t)i << PDRSHIFT;
		((pd_entry_t *)DMPDphys)[i] |= PG_RW | PG_V | PG_PS;
	}

	/* And the direct map space's PDP */
	for (i = 0; i < ndmpdp; i++) {
		((pdp_entry_t *)DMPDPphys)[i] = DMPDphys + (i << PAGE_SHIFT);
		((pdp_entry_t *)DMPDPphys)[i] |= PG_RW | PG_V | PG_U;
	}

	/* And recursively map PML4 to itself in order to get PTmap */
	((pdp_entry_t *)KPML4phys)[PML4PML4I] = KPML4phys;
	((pdp_entry_t *)KPML4phys)[PML4PML4I] |= PG_RW | PG_V | PG_U;

	/* Connect the Direct Map slot up to the PML4 */
	((pdp_entry_t *)KPML4phys)[DMPML4I] = DMPDPphys;
	((pdp_entry_t *)KPML4phys)[DMPML4I] |= PG_RW | PG_V | PG_U;

	/* Connect the KVA slot up to the PML4 */
	((pdp_entry_t *)KPML4phys)[KPML4I] = KPDPphys;
	((pdp_entry_t *)KPML4phys)[KPML4I] |= PG_RW | PG_V | PG_U;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On amd64 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
void
pmap_bootstrap(firstaddr)
	vm_paddr_t *firstaddr;
{
	vm_offset_t va;
	pt_entry_t *pte;

	avail_start = *firstaddr;

	/*
	 * Create an initial set of page tables to run the kernel in.
	 */
	create_pagetables();
	*firstaddr = avail_start;

	virtual_avail = (vm_offset_t) KERNBASE + avail_start;
	virtual_avail = pmap_kmem_choose(virtual_avail);

	virtual_end = VM_MAX_KERNEL_ADDRESS;


	/* XXX do %cr0 as well */
	load_cr4(rcr4() | CR4_PGE | CR4_PSE);
	load_cr3(KPML4phys);

	/*
	 * Initialize protection array.
	 */
	amd64_protection_init();

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	kernel_pmap->pm_pml4 = (pdp_entry_t *) (KERNBASE + KPML4phys);
	kernel_pmap->pm_active = -1;	/* don't allow deactivation */
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	LIST_INIT(&allpmaps);
	mtx_init(&allpmaps_lock, "allpmaps", NULL, MTX_SPIN);
	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, kernel_pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);
	nkpt = NKPT;

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = vtopte(va);

	/*
	 * CMAP1 is only used for the memory test.
	 */
	SYSMAP(caddr_t, CMAP1, CADDR1, 1)

	/*
	 * Crashdump maps.
	 */
	SYSMAP(caddr_t, pt_crashdumpmap, crashdumpmap, MAXDUMPPGS);

	/*
	 * ptvmmap is used for reading arbitrary physical pages via /dev/mem.
	 * XXX ptmmap is not used.
	 */
	SYSMAP(caddr_t, ptmmap, ptvmmap, 1)

	/*
	 * msgbufp is used to map the system message buffer.
	 * XXX msgbufmap is not used.
	 */
	SYSMAP(struct msgbuf *, msgbufmap, msgbufp,
	       atop(round_page(MSGBUF_SIZE)))

	virtual_avail = va;

	*CMAP1 = 0;

	invltlb();
}

static void *
pmap_pv_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	*flags = UMA_SLAB_PRIV;
	return (void *)kmem_alloc(kernel_map, bytes);
}

void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	static vm_pindex_t colour;
	vm_page_t m;
	int pflags;
	void *va;

	*flags = UMA_SLAB_PRIV;

	if ((wait & (M_NOWAIT|M_USE_RESERVE)) == M_NOWAIT)
		pflags = VM_ALLOC_INTERRUPT;
	else
		pflags = VM_ALLOC_SYSTEM;

	if (wait & M_ZERO)
		pflags |= VM_ALLOC_ZERO;

	for (;;) {
		m = vm_page_alloc(NULL, colour++, pflags | VM_ALLOC_NOOBJ);
		if (m == NULL) {
			if (wait & M_NOWAIT)
				return (NULL);
			else
				VM_WAIT;
		} else
			break;
	}

	va = (void *)PHYS_TO_DMAP(m->phys_addr);
	if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
		pagezero(va);
	return (va);
}

void
uma_small_free(void *mem, int size, u_int8_t flags)
{
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)mem));
	vm_page_lock_queues();
	vm_page_free(m);
	vm_page_unlock_queues();
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
	vm_paddr_t phys_start, phys_end;
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
	pvzone = uma_zcreate("PV ENTRY", sizeof (struct pv_entry), NULL, NULL, 
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	uma_zone_set_allocf(pvzone, pmap_pv_allocf);
	uma_prealloc(pvzone, initial_pvs);

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
	TUNABLE_INT_FETCH("vm.pmap.pv_entries", &pv_entry_max);
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	uma_zone_set_obj(pvzone, &pvzone_obj, pv_entry_max);
}


/***************************************************
 * Low level helper routines.....
 ***************************************************/

#if defined(PMAP_DIAGNOSTIC)

/*
 * This code checks for non-writeable/modified pages.
 * This should be an invalid condition.
 */
static int
pmap_nw_modified(pt_entry_t ptea)
{
	int pte;

	pte = (int) ptea;

	if ((pte & (PG_M|PG_RW)) == PG_M)
		return 1;
	else
		return 0;
}
#endif


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
 * Normal invalidation functions.
 * We inline these within pmap.c for speed.
 */
PMAP_INLINE void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{

	if (pmap == kernel_pmap || pmap->pm_active)
		invlpg(va);
}

PMAP_INLINE void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	if (pmap == kernel_pmap || pmap->pm_active)
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
}

PMAP_INLINE void
pmap_invalidate_all(pmap_t pmap)
{

	if (pmap == kernel_pmap || pmap->pm_active)
		invltlb();
}

/*
 * Are we current address space or kernel?
 */
static __inline int
pmap_is_current(pmap_t pmap)
{
	return (pmap == kernel_pmap ||
	    (pmap->pm_pml4[PML4PML4I] & PG_FRAME) == (PML4pml4e[0] & PG_FRAME));
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t 
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	vm_paddr_t rtval;
	pt_entry_t *pte;
	pd_entry_t pde, *pdep;

	if (pmap == 0)
		return 0;
	pdep = pmap_pde(pmap, va);
	if (pdep) {
		pde = *pdep;
		if (pde) {
			if ((pde & PG_PS) != 0) {
				rtval = (pde & ~PDRMASK) | (va & PDRMASK);
				return rtval;
			}
			pte = pmap_pte(pmap, va);
			rtval = ((*pte & PG_FRAME) | (va & PAGE_MASK));
			return rtval;
		}
	}
	return 0;

}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	pd_entry_t *pde;
	vm_paddr_t pa;

	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS) {
		pa = DMAP_TO_PHYS(va);
	} else {
		pde = pmap_pde(kernel_pmap, va);
		if (*pde & PG_PS) {
			pa = (*pde & ~(NBPDR - 1)) | (va & (NBPDR - 1));
		} else {
			pa = *vtopte(va);
			pa = (pa & PG_FRAME) | (va & PAGE_MASK);
		}
	}
	return pa;
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Add a wired page to the kva.
 * Note: not SMP coherent.
 */
PMAP_INLINE void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store(pte, pa | PG_RW | PG_V | PG_G);
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_clear(pte);
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
pmap_qenter(vm_offset_t sva, vm_page_t *m, int count)
{
	vm_offset_t va;

	va = sva;
	while (count-- > 0) {
		pmap_kenter(va, VM_PAGE_TO_PHYS(*m));
		va += PAGE_SIZE;
		m++;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
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
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

static vm_page_t
pmap_page_lookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

retry:
	m = vm_page_lookup(object, pindex);
	if (m != NULL) {
		vm_page_lock_queues();
		if (vm_page_sleep_if_busy(m, FALSE, "pplookp"))
			goto retry;
		vm_page_unlock_queues();
	}
	return m;
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

	while (vm_page_sleep_if_busy(m, FALSE, "pmuwpt"))
		vm_page_lock_queues();

	if (m->hold_count == 0) {
		vm_offset_t pteva;

		/*
		 * unmap the page table page
		 */
		if (m->pindex >= (NUPDE + NUPDPE)) {
			/* PDP page */
			pml4_entry_t *pml4;
			pml4 = pmap_pml4e(pmap, va);
			pteva = (vm_offset_t) PDPmap + amd64_ptob(m->pindex - (NUPDE + NUPDPE));
			*pml4 = 0;
		} else if (m->pindex >= NUPDE) {
			/* PD page */
			pdp_entry_t *pdp;
			pdp = pmap_pdpe(pmap, va);
			pteva = (vm_offset_t) PDmap + amd64_ptob(m->pindex - NUPDE);
			*pdp = 0;
		} else {
			/* PTE page */
			pd_entry_t *pd;
			pd = pmap_pde(pmap, va);
			pteva = (vm_offset_t) PTmap + amd64_ptob(m->pindex);
			*pd = 0;
		}
		--pmap->pm_stats.resident_count;
		if (m->pindex < NUPDE) {
			/* We just released a PT, unhold the matching PD */
			vm_page_t pdpg;

			pdpg = vm_page_lookup(pmap->pm_pteobj, NUPDE +
			    ((va >> PDPSHIFT) & (NUPDPE - 1)));
			while (vm_page_sleep_if_busy(pdpg, FALSE, "pulook"))
				vm_page_lock_queues();
			vm_page_unhold(pdpg);
			if (pdpg->hold_count == 0)
				_pmap_unwire_pte_hold(pmap, va, pdpg);
		}
		if (m->pindex >= NUPDE && m->pindex < (NUPDE + NUPDPE)) {
			/* We just released a PD, unhold the matching PDP */
			vm_page_t pdppg;

			pdppg = vm_page_lookup(pmap->pm_pteobj, NUPDE + NUPDPE +
			    ((va >> PML4SHIFT) & (NUPML4E - 1)));
			while (vm_page_sleep_if_busy(pdppg, FALSE, "pulooK"))
				vm_page_lock_queues();
			vm_page_unhold(pdppg);
			if (pdppg->hold_count == 0)
				_pmap_unwire_pte_hold(pmap, va, pdppg);
		}
		if (pmap_is_current(pmap)) {
			/*
			 * Do an invltlb to make the invalidated mapping
			 * take effect immediately.
			 */
			pmap_invalidate_page(pmap, pteva);
		}

		/*
		 * If the page is finally unwired, simply free it.
		 */
		--m->wire_count;
		if (m->wire_count == 0) {
			vm_page_busy(m);
			vm_page_free_zero(m);
			atomic_subtract_int(&cnt.v_wire_count, 1);
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
	vm_pindex_t ptepindex;

	if (va >= VM_MAXUSER_ADDRESS)
		return 0;

	if (mpte == NULL) {
		ptepindex = pmap_pde_pindex(va);
		if (pmap->pm_pteobj->root &&
		    pmap->pm_pteobj->root->pindex == ptepindex) {
			mpte = pmap->pm_pteobj->root;
		} else {
			while ((mpte = vm_page_lookup(pmap->pm_pteobj, ptepindex)) != NULL &&
			    vm_page_sleep_if_busy(mpte, FALSE, "pulook"))
				vm_page_lock_queues();
		}
	}

	return pmap_unwire_pte_hold(pmap, va, mpte);
}

void
pmap_pinit0(pmap)
	struct pmap *pmap;
{

	pmap->pm_pml4 = (pml4_entry_t *)(KERNBASE + KPML4phys);
	pmap->pm_active = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	vm_page_t pml4pg;

	/*
	 * allocate object for the ptes
	 */
	if (pmap->pm_pteobj == NULL)
		pmap->pm_pteobj = vm_object_allocate(OBJT_DEFAULT, NUPDE + NUPDPE + NUPML4E + 1);

	/*
	 * allocate the page directory page
	 */
	pml4pg = vm_page_grab(pmap->pm_pteobj, NUPDE + NUPDPE + NUPML4E,
	    VM_ALLOC_NORMAL | VM_ALLOC_RETRY | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	vm_page_lock_queues();
	vm_page_flag_clear(pml4pg, PG_BUSY);
	pml4pg->valid = VM_PAGE_BITS_ALL;
	vm_page_unlock_queues();

	pmap->pm_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml4pg));

	if ((pml4pg->flags & PG_ZERO) == 0)
		bzero(pmap->pm_pml4, PAGE_SIZE);

	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);

	/* Wire in kernel global address entries. */
	pmap->pm_pml4[KPML4I] = KPDPphys | PG_RW | PG_V | PG_U;
	pmap->pm_pml4[DMPML4I] = DMPDPphys | PG_RW | PG_V | PG_U;

	/* install self-referential address mapping entry(s) */
	pmap->pm_pml4[PML4PML4I] = VM_PAGE_TO_PHYS(pml4pg) | PG_V | PG_RW | PG_A | PG_M;

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
	/* XXX: Remove this stub when no longer called */
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap, ptepindex)
	pmap_t	pmap;
	vm_pindex_t ptepindex;
{
	vm_page_t m, pdppg, pdpg;

	/*
	 * Find or fabricate a new pagetable page
	 */
	m = vm_page_grab(pmap->pm_pteobj, ptepindex,
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO | VM_ALLOC_RETRY);

	KASSERT(m->queue == PQ_NONE,
		("_pmap_allocpte: %p->queue != PQ_NONE", m));

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

	if (ptepindex >= (NUPDE + NUPDPE)) {
		pml4_entry_t *pml4;
		vm_pindex_t pml4index;

		/* Wire up a new PDPE page */
		pml4index = ptepindex - (NUPDE + NUPDPE);
		pml4 = &pmap->pm_pml4[pml4index];
		*pml4 = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;

	} else if (ptepindex >= NUPDE) {
		vm_pindex_t pml4index;
		vm_pindex_t pdpindex;
		pml4_entry_t *pml4;
		pdp_entry_t *pdp;

		/* Wire up a new PDE page */
		pdpindex = ptepindex - NUPDE;
		pml4index = pdpindex >> NPML4EPGSHIFT;

		pml4 = &pmap->pm_pml4[pml4index];
		if ((*pml4 & PG_V) == 0) {
			/* Have to allocate a new pdp, recurse */
			_pmap_allocpte(pmap, NUPDE + NUPDPE + pml4index);
		} else {
			/* Add reference to pdp page */
			pdppg = pmap_page_lookup(pmap->pm_pteobj,
			    NUPDE + NUPDPE + pml4index);
			pdppg->hold_count++;
		}
		pdp = (pdp_entry_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);

		/* Now find the pdp page */
		pdp = &pdp[pdpindex & ((1ul << NPDPEPGSHIFT) - 1)];
		*pdp = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;

	} else {
		vm_pindex_t pml4index;
		vm_pindex_t pdpindex;
		pml4_entry_t *pml4;
		pdp_entry_t *pdp;
		pd_entry_t *pd;

		/* Wire up a new PTE page */
		pdpindex = ptepindex >> NPDPEPGSHIFT;
		pml4index = pdpindex >> NPML4EPGSHIFT;

		/* First, find the pdp and check that its valid. */
		pml4 = &pmap->pm_pml4[pml4index];
		if ((*pml4 & PG_V) == 0) {
			/* Have to allocate a new pd, recurse */
			_pmap_allocpte(pmap, NUPDE + pdpindex);
			pdp = (pdp_entry_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);
			pdp = &pdp[pdpindex & ((1ul << NPDPEPGSHIFT) - 1)];
		} else {
			pdp = (pdp_entry_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);
			pdp = &pdp[pdpindex & ((1ul << NPDPEPGSHIFT) - 1)];
			if ((*pdp & PG_V) == 0) {
				/* Have to allocate a new pd, recurse */
				_pmap_allocpte(pmap, NUPDE + pdpindex);
			} else {
				/* Add reference to the pd page */
				pdpg = pmap_page_lookup(pmap->pm_pteobj,
				    NUPDE + pdpindex);
				pdpg->hold_count++;
			}
		}
		pd = (pd_entry_t *)PHYS_TO_DMAP(*pdp & PG_FRAME);

		/* Now we know where the page directory page is */
		pd = &pd[ptepindex & ((1ul << NPDEPGSHIFT) - 1)];
		*pd = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;
	}

	/*
	 * Try to use the new mapping, but if we cannot, then
	 * do it with the routine that maps the page explicitly.
	 */
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	vm_page_lock_queues();
	m->valid = VM_PAGE_BITS_ALL;
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_wakeup(m);
	vm_page_unlock_queues();

	return m;
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va)
{
	vm_pindex_t ptepindex;
	pd_entry_t *pd;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_pde_pindex(va);

	/*
	 * Get the page directory entry
	 */
	pd = pmap_pde(pmap, va);

	/*
	 * This supports switching from a 2MB page to a
	 * normal 4K page.
	 */
	if (pd != 0 && (*pd & (PG_PS | PG_V)) == (PG_PS | PG_V)) {
		*pd = 0;
		pd = 0;
		pmap_invalidate_all(kernel_pmap);
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (pd != 0 && (*pd & PG_V) != 0) {
		/*
		 * In order to get the page table page, try the
		 * hint first.
		 */
		if (pmap->pm_pteobj->root &&
			(pmap->pm_pteobj->root->pindex == ptepindex)) {
			m = pmap->pm_pteobj->root;
		} else {
			m = pmap_page_lookup(pmap->pm_pteobj, ptepindex);
		}
		m->hold_count++;
		return m;
	}
	/*
	 * Here if the pte page isn't mapped, or if it has been deallocated.
	 */
	m = _pmap_allocpte(pmap, ptepindex);
	return m;
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
	vm_object_t object;
	vm_page_t m;

	object = pmap->pm_pteobj;

	KASSERT(object->ref_count == 1,
	    ("pmap_release: pteobj reference count %d != 1",
	    object->ref_count));
	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	mtx_lock_spin(&allpmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);

	vm_page_lock_queues();
	while ((m = TAILQ_FIRST(&object->memq)) != NULL) {
		m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_busy(m);
		vm_page_free(m);
	}
	KASSERT(TAILQ_EMPTY(&object->memq),
	    ("pmap_release: leaking page table pages"));
	vm_page_unlock_queues();
}

static int
kvm_size(SYSCTL_HANDLER_ARGS)
{
	unsigned long ksize = VM_MAX_KERNEL_ADDRESS - KERNBASE;

	return sysctl_handle_long(oidp, &ksize, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_size, CTLTYPE_LONG|CTLFLAG_RD, 
    0, 0, kvm_size, "IU", "Size of KVM");

static int
kvm_free(SYSCTL_HANDLER_ARGS)
{
	unsigned long kfree = VM_MAX_KERNEL_ADDRESS - kernel_vm_end;

	return sysctl_handle_long(oidp, &kfree, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_free, CTLTYPE_LONG|CTLFLAG_RD, 
    0, 0, kvm_free, "IU", "Amount of KVM free");

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	int s;
	vm_paddr_t paddr;
	vm_page_t nkpg;
	pd_entry_t *pde, newpdir;
	pdp_entry_t newpdp;

	s = splhigh();
	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
	if (kernel_vm_end == 0) {
		kernel_vm_end = KERNBASE;
		nkpt = 0;
		while ((*pmap_pde(kernel_pmap, kernel_vm_end) & PG_V) != 0) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			nkpt++;
		}
	}
	addr = roundup2(addr, PAGE_SIZE * NPTEPG);
	while (kernel_vm_end < addr) {
		pde = pmap_pde(kernel_pmap, kernel_vm_end);
		if (pde == NULL) {
			/* We need a new PDP entry */
			nkpg = vm_page_alloc(NULL, nkpt,
			    VM_ALLOC_NOOBJ | VM_ALLOC_SYSTEM | VM_ALLOC_WIRED);
			if (!nkpg)
				panic("pmap_growkernel: no memory to grow kernel");
			pmap_zero_page(nkpg);
			paddr = VM_PAGE_TO_PHYS(nkpg);
			newpdp = (pdp_entry_t)
				(paddr | PG_V | PG_RW | PG_A | PG_M);
			*pmap_pdpe(kernel_pmap, kernel_vm_end) = newpdp;
			continue; /* try again */
		}
		if ((*pde & PG_V) != 0) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			continue;
		}

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = vm_page_alloc(NULL, nkpt,
		    VM_ALLOC_NOOBJ | VM_ALLOC_SYSTEM | VM_ALLOC_WIRED);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");

		nkpt++;

		pmap_zero_page(nkpg);
		paddr = VM_PAGE_TO_PHYS(nkpg);
		newpdir = (pd_entry_t) (paddr | PG_V | PG_RW | PG_A | PG_M);
		*pmap_pde(kernel_pmap, kernel_vm_end) = newpdir;

		kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	}
	splx(s);
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
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
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
			vm_page_flag_clear(m, PG_WRITEABLE);

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

	oldpte = pte_load_clear(ptq);
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	/*
	 * Machines that don't support invlpg, also don't support
	 * PG_G.
	 */
	if (oldpte & PG_G)
		pmap_invalidate_page(kernel_pmap, va);
	pmap->pm_stats.resident_count -= 1;
	if (oldpte & PG_MANAGED) {
		m = PHYS_TO_VM_PAGE(oldpte);
		if (oldpte & PG_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified((pt_entry_t) oldpte)) {
				printf(
	"pmap_remove: modified page not writable: va: 0x%x, pte: 0x%x\n",
				    va, oldpte);
			}
#endif
			if (pmap_track_modified(va))
				vm_page_dirty(m);
		}
		if (oldpte & PG_A)
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
	pt_entry_t *pte;

	pte = pmap_pte(pmap, va);
	if (pte == NULL || (*pte & PG_V) == 0)
		return;
	pmap_remove_pte(pmap, pte, va);
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
	vm_offset_t pdnxt;
	pd_entry_t ptpaddr, *pde;
	pt_entry_t *pte;
	int anyvalid;

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
		pde = pmap_pde(pmap, sva);
		if (pde && (*pde & PG_PS) == 0) {
			pmap_remove_page(pmap, sva);
			return;
		}
	}

	anyvalid = 0;

	for (; sva < eva; sva = pdnxt) {

		if (pmap->pm_stats.resident_count == 0)
			break;

		/*
		 * Calculate index for next page table.
		 */
		pdnxt = (sva + NBPDR) & ~PDRMASK;

		pde = pmap_pde(pmap, sva);
		if (pde == 0)
			continue;
		ptpaddr = *pde;

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			*pde = 0;
			pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
			anyvalid = 1;
			continue;
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (pdnxt > eva)
			pdnxt = eva;

		for (; sva != pdnxt; sva += PAGE_SIZE) {
			pte = pmap_pte(pmap, sva);
			if (pte == NULL || *pte == 0)
				continue;
			anyvalid = 1;
			if (pmap_remove_pte(pmap, pte, sva))
				break;
		}
	}

	if (anyvalid)
		pmap_invalidate_all(pmap);
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
	register pv_entry_t pv;
	pt_entry_t *pte, tpte;
	int s;

#if defined(PMAP_DIAGNOSTIC)
	/*
	 * XXX This makes pmap_remove_all() illegal for non-managed pages!
	 */
	if (!pmap_initialized || (m->flags & PG_FICTITIOUS)) {
		panic("pmap_remove_all: illegal for unmanaged page, va: 0x%x",
		    VM_PAGE_TO_PHYS(m));
	}
#endif
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	s = splvm();
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pv->pv_pmap->pm_stats.resident_count--;
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		tpte = pte_load_clear(pte);
		if (tpte & PG_W)
			pv->pv_pmap->pm_stats.wired_count--;
		if (tpte & PG_A)
			vm_page_flag_set(m, PG_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PG_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified((pt_entry_t) tpte)) {
				printf(
	"pmap_remove_all: modified page not writable: va: 0x%x, pte: 0x%x\n",
				    pv->pv_va, tpte);
			}
#endif
			if (pmap_track_modified(pv->pv_va))
				vm_page_dirty(m);
		}
		pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
	splx(s);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t pdnxt;
	pd_entry_t ptpaddr, *pde;
	int anychanged;

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	anychanged = 0;

	for (; sva < eva; sva = pdnxt) {

		pdnxt = (sva + NBPDR) & ~PDRMASK;

		pde = pmap_pde(pmap, sva);
		if (pde == NULL)
			continue;
		ptpaddr = *pde;

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			*pde &= ~(PG_M|PG_RW);
			pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
			anychanged = 1;
			continue;
		}

		if (pdnxt > eva)
			pdnxt = eva;

		for (; sva != pdnxt; sva += PAGE_SIZE) {
			pt_entry_t pbits;
			pt_entry_t *pte;
			vm_page_t m;

			pte = pmap_pte(pmap, sva);
			if (pte == NULL)
				continue;
			pbits = *pte;
			if (pbits & PG_MANAGED) {
				m = NULL;
				if (pbits & PG_A) {
					m = PHYS_TO_VM_PAGE(pbits);
					vm_page_flag_set(m, PG_REFERENCED);
					pbits &= ~PG_A;
				}
				if ((pbits & PG_M) != 0 &&
				    pmap_track_modified(sva)) {
					if (m == NULL)
						m = PHYS_TO_VM_PAGE(pbits);
					vm_page_dirty(m);
					pbits &= ~PG_M;
				}
			}

			pbits &= ~PG_RW;

			if (pbits != *pte) {
				pte_store(pte, pbits);
				anychanged = 1;
			}
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
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
	vm_paddr_t pa;
	register pt_entry_t *pte;
	vm_paddr_t opa;
	pt_entry_t origpte, newpte;
	vm_page_t mpte;

	if (pmap == NULL)
		return;

	va &= PG_FRAME;
#ifdef PMAP_DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
	if ((va >= UPT_MIN_ADDRESS) && (va < UPT_MAX_ADDRESS))
		panic("pmap_enter: invalid to pmap_enter page table pages (va: 0x%x)", va);
#endif

	mpte = NULL;
	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_allocpte(pmap, va);
	}
#if 0 && defined(PMAP_DIAGNOSTIC)
	else {
		pd_entry_t *pdeaddr = pmap_pde(pmap, va);
		origpte = *pdeaddr;
		if ((origpte & PG_V) == 0) { 
			panic("pmap_enter: invalid kernel page table page, pde=%p, va=%p\n",
				origpte, va);
		}
	}
#endif

	pte = pmap_pte(pmap, va);

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL)
		panic("pmap_enter: invalid page directory va=%#lx\n", va);

	pa = VM_PAGE_TO_PHYS(m) & PG_FRAME;
	origpte = *pte;
	opa = origpte & PG_FRAME;

	if (origpte & PG_PS)
		panic("pmap_enter: attempted pmap_enter on 2MB page");

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

#if defined(PMAP_DIAGNOSTIC)
		if (pmap_nw_modified((pt_entry_t) origpte)) {
			printf(
	"pmap_enter: modified page not writable: va: 0x%x, pte: 0x%x\n",
			    va, origpte);
		}
#endif

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->hold_count--;

		if ((prot & VM_PROT_WRITE) && (origpte & PG_V)) {
			if ((origpte & PG_RW) == 0) {
				pte_store(pte, origpte | PG_RW);
				pmap_invalidate_page(pmap, va);
			}
			return;
		}

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (origpte & PG_MANAGED) {
			if ((origpte & PG_M) && pmap_track_modified(va)) {
				vm_page_t om;
				om = PHYS_TO_VM_PAGE(opa);
				vm_page_dirty(om);
			}
			pa |= PG_MANAGED;
		}
		goto validate;
	} 
	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		int err;
		vm_page_lock_queues();
		err = pmap_remove_pte(pmap, pte, va);
		vm_page_unlock_queues();
		if (err)
			panic("pmap_enter: pte vanished, va: 0x%lx", va);
	}

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if (pmap_initialized && 
	    (m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0) {
		pmap_insert_entry(pmap, va, mpte, m);
		pa |= PG_MANAGED;
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
	newpte = (pt_entry_t)(pa | pte_prot(pmap, prot) | PG_V);

	if (wired)
		newpte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		newpte |= PG_U;
	if (pmap == kernel_pmap)
		newpte |= PG_G;

	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if ((origpte & ~(PG_M|PG_A)) != newpte) {
		pte_store(pte, newpte | PG_A);
		/*if (origpte)*/ {
			pmap_invalidate_page(pmap, va);
		}
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

vm_page_t
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_page_t mpte)
{
	pt_entry_t *pte;
	vm_paddr_t pa;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		vm_pindex_t ptepindex;
		pd_entry_t *ptepa;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = pmap_pde_pindex(va);
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->hold_count++;
		} else {
retry:
			/*
			 * Get the page directory entry
			 */
			ptepa = pmap_pde(pmap, va);

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (ptepa && (*ptepa & PG_V) != 0) {
				if (*ptepa & PG_PS)
					panic("pmap_enter_quick: unexpected mapping into 2MB page");
				if (pmap->pm_pteobj->root &&
					(pmap->pm_pteobj->root->pindex == ptepindex)) {
					mpte = pmap->pm_pteobj->root;
				} else {
					mpte = pmap_page_lookup(pmap->pm_pteobj, ptepindex);
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
	 * quick entry into any pmap, one would likely use pmap_pte.
	 * But that isn't as quick as vtopte.
	 */
	pte = vtopte(va);
	if (*pte) {
		if (mpte != NULL) {
			vm_page_lock_queues();
			pmap_unwire_pte_hold(pmap, va, mpte);
			vm_page_unlock_queues();
		}
		return 0;
	}

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if ((m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0)
		pmap_insert_entry(pmap, va, mpte, m);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * Now validate mapping with RO protection
	 */
	if (m->flags & (PG_FICTITIOUS|PG_UNMANAGED))
		pte_store(pte, pa | PG_V | PG_U);
	else
		pte_store(pte, pa | PG_V | PG_U | PG_MANAGED);

	return mpte;
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_offset_t pa, int i)
{
	vm_offset_t va;

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	pmap_kenter(va, pa);
	invlpg(va);
	return ((void *)crashdumpmap);
}

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size)
{
	vm_page_t p;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT(object->type == OBJT_DEVICE,
	    ("pmap_object_init_pt: non-device object"));
	if (((addr & (NBPDR - 1)) == 0) && ((size & (NBPDR - 1)) == 0)) {
		int i;
		vm_page_t m[1];
		int npdes;
		pd_entry_t ptepa, *pde;

		pde = pmap_pde(pmap, addr);
		if (pde != 0 && (*pde & PG_V) != 0)
			return;
retry:
		p = vm_page_lookup(object, pindex);
		if (p != NULL) {
			vm_page_lock_queues();
			if (vm_page_sleep_if_busy(p, FALSE, "init4p"))
				goto retry;
		} else {
			p = vm_page_alloc(object, pindex, VM_ALLOC_NORMAL);
			if (p == NULL)
				return;
			m[0] = p;

			if (vm_pager_get_pages(object, m, 1, 0) != VM_PAGER_OK) {
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
				return;
			}

			p = vm_page_lookup(object, pindex);
			vm_page_lock_queues();
			vm_page_wakeup(p);
		}
		vm_page_unlock_queues();

		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1))
			return;

		p->valid = VM_PAGE_BITS_ALL;

		pmap->pm_stats.resident_count += size >> PAGE_SHIFT;
		npdes = size >> PDRSHIFT;
		for(i = 0; i < npdes; i++) {
			pde_store(pde, ptepa | PG_U | PG_RW | PG_V | PG_PS);
			ptepa += NBPDR;
			pde++;
		}
		pmap_invalidate_all(pmap);
	}
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
	-1 * PAGE_SIZE, 1 * PAGE_SIZE,
	-2 * PAGE_SIZE, 2 * PAGE_SIZE,
	-3 * PAGE_SIZE, 3 * PAGE_SIZE,
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
	pd_entry_t *pde;

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
		vm_object_t backing_object, lobject;
		pt_entry_t *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		pde = pmap_pde(pmap, addr);
		if (pde == NULL || (*pde & PG_V) == 0) 
			continue;

		pte = vtopte(addr);
		if ((*pte & PG_V) == 0)
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = object;
		VM_OBJECT_LOCK(lobject);
		while ((m = vm_page_lookup(lobject, pindex)) == NULL &&
		    lobject->type == OBJT_DEFAULT &&
		    (backing_object = lobject->backing_object) != NULL) {
			if (lobject->backing_object_offset & PAGE_MASK)
				break;
			pindex += lobject->backing_object_offset >> PAGE_SHIFT;
			VM_OBJECT_LOCK(backing_object);
			VM_OBJECT_UNLOCK(lobject);
			lobject = backing_object;
		}
		VM_OBJECT_UNLOCK(lobject);
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
	register pt_entry_t *pte;

	if (pmap == NULL)
		return;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pte = pmap_pte(pmap, va);
	if (wired && (*pte & PG_W) == 0) {
		pmap->pm_stats.wired_count++;
		*pte |= PG_W;
	} else if (!wired && (*pte & PG_W) != 0) {
		pmap->pm_stats.wired_count--;
		*pte &= ~PG_W;
	}
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
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t pdnxt;
	vm_page_t m;

	if (dst_addr != src_addr)
		return;

	if (!pmap_is_current(src_pmap))
		return;

	for (addr = src_addr; addr < end_addr; addr = pdnxt) {
		pt_entry_t *src_pte, *dst_pte;
		vm_page_t dstmpte, srcmpte;
		pd_entry_t srcptepaddr, *pde;
		vm_pindex_t ptepindex;

		if (addr >= UPT_MIN_ADDRESS)
			panic("pmap_copy: invalid to pmap_copy page tables\n");

		/*
		 * Don't let optional prefaulting of pages make us go
		 * way below the low water mark of free pages or way
		 * above high water mark of used pv entries.
		 */
		if (cnt.v_free_count < cnt.v_free_reserved ||
		    pv_entry_count > pv_entry_high_water)
			break;
		
		pdnxt = (addr + NBPDR) & ~PDRMASK;
		ptepindex = pmap_pde_pindex(addr);

		pde = pmap_pde(src_pmap, addr);
		if (pde)
			srcptepaddr = *pde;
		else
			continue;
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			pde = pmap_pde(dst_pmap, addr);
			if (pde == 0) {
				/*
				 * XXX should do an allocpte here to
				 * instantiate the pde
				 */
				continue;
			}
			if (*pde == 0) {
				*pde = srcptepaddr;
				dst_pmap->pm_stats.resident_count +=
				    NBPDR / PAGE_SIZE;
			}
			continue;
		}

		srcmpte = vm_page_lookup(src_pmap->pm_pteobj, ptepindex);
		if ((srcmpte == NULL) ||
		    (srcmpte->hold_count == 0) || (srcmpte->flags & PG_BUSY))
			continue;

		if (pdnxt > end_addr)
			pdnxt = end_addr;

		src_pte = vtopte(addr);
		while (addr < pdnxt) {
			pt_entry_t ptetemp;
			ptetemp = *src_pte;
			/*
			 * we only virtual copy managed pages
			 */
			if ((ptetemp & PG_MANAGED) != 0) {
				/*
				 * We have to check after allocpte for the
				 * pte still being around...  allocpte can
				 * block.
				 */
				dstmpte = pmap_allocpte(dst_pmap, addr);
				dst_pte = pmap_pte(dst_pmap, addr);
				if ((*dst_pte == 0) && (ptetemp = *src_pte)) {
					/*
					 * Clear the modified and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					m = PHYS_TO_VM_PAGE(ptetemp);
					*dst_pte = ptetemp & ~(PG_M | PG_A);
					dst_pmap->pm_stats.resident_count++;
					pmap_insert_entry(dst_pmap, addr,
						dstmpte, m);
	 			} else {
					vm_page_lock_queues();
					pmap_unwire_pte_hold(dst_pmap, addr, dstmpte);
					vm_page_unlock_queues();
				}
				if (dstmpte->hold_count >= srcmpte->hold_count)
					break;
			}
			addr += PAGE_SIZE;
			src_pte++;
		}
	}
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

	bcopy((void *)src, (void *)dst, PAGE_SIZE);
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
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
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
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	s = splvm();
	for (pv = TAILQ_FIRST(&pmap->pm_pvlist); pv; pv = npv) {

		if (pv->pv_va >= eva || pv->pv_va < sva) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
		pte = vtopte(pv->pv_va);
#else
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
#endif
		tpte = *pte;

		if (tpte == 0) {
			printf("TPTE at %p  IS ZERO @ VA %08lx\n",
							pte, pv->pv_va);
			panic("bad pte");
		}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (tpte & PG_W) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}

		m = PHYS_TO_VM_PAGE(tpte);
		KASSERT(m->phys_addr == (tpte & PG_FRAME),
		    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
		    m, (uintmax_t)m->phys_addr, (uintmax_t)tpte));

		KASSERT(m < &vm_page_array[vm_page_array_size],
			("pmap_remove_pages: bad tpte %#jx", (uintmax_t)tpte));

		pv->pv_pmap->pm_stats.resident_count--;

		pte_clear(pte);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PG_M) {
			vm_page_dirty(m);
		}

		npv = TAILQ_NEXT(pv, pv_plist);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);

		m->md.pv_list_count--;
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		if (TAILQ_FIRST(&m->md.pv_list) == NULL) {
			vm_page_flag_clear(m, PG_WRITEABLE);
		}

		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}
	splx(s);
	pmap_invalidate_all(pmap);
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
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	s = splvm();
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		/*
		 * if the bit being tested is the modified bit, then
		 * mark clean_map and ptes as never
		 * modified.
		 */
		if (!pmap_track_modified(pv->pv_va))
			continue;
#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (tb) at va: 0x%x\n", pv->pv_va);
			continue;
		}
#endif
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		if (*pte & PG_M) {
			splx(s);
			return TRUE;
		}
	}
	splx(s);
	return (FALSE);
}

/*
 * this routine is used to modify bits in ptes
 */
static __inline void
pmap_changebit(vm_page_t m, int bit, boolean_t setem)
{
	register pv_entry_t pv;
	register pt_entry_t *pte;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS) ||
	    (!setem && bit == PG_RW && (m->flags & PG_WRITEABLE) == 0))
		return;

	s = splvm();
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		/*
		 * don't write protect pager mappings
		 */
		if (!setem && (bit == PG_RW)) {
			if (!pmap_track_modified(pv->pv_va))
				continue;
		}

#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (cb) at va: 0x%x\n", pv->pv_va);
			continue;
		}
#endif

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);

		if (setem) {
			*pte |= bit;
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		} else {
			pt_entry_t pbits = *pte;
			if (pbits & bit) {
				if (bit == PG_RW) {
					if (pbits & PG_M) {
						vm_page_dirty(m);
					}
					pte_store(pte, pbits & ~(PG_M|PG_RW));
				} else {
					pte_store(pte, pbits & ~bit);
				}
				pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			}
		}
	}
	if (!setem && bit == PG_RW)
		vm_page_flag_clear(m, PG_WRITEABLE);
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
			pmap_changebit(m, PG_RW, FALSE);
		} else {
			pmap_remove_all(m);
		}
	}
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
	register pv_entry_t pv, pvf, pvn;
	pt_entry_t *pte;
	pt_entry_t v;
	int s;
	int rtval = 0;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return (rtval);

	s = splvm();
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {

		pvf = pv;

		do {
			pvn = TAILQ_NEXT(pv, pv_list);

			TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);

			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);

			if (!pmap_track_modified(pv->pv_va))
				continue;

			pte = pmap_pte(pv->pv_pmap, pv->pv_va);

			if (pte && ((v = pte_load(pte)) & PG_A) != 0) {
				pte_store(pte, v & ~PG_A);
				pmap_invalidate_page(pv->pv_pmap, pv->pv_va);

				rtval++;
				if (rtval > 4) {
					break;
				}
			}
		} while ((pv = pvn) != NULL && pv != pvf);
	}
	splx(s);

	return (rtval);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	pmap_changebit(m, PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_page_t m)
{
	pmap_changebit(m, PG_A, FALSE);
}

/*
 * Miscellaneous support routines follow
 */

static void
amd64_protection_init()
{
	register long *kp, prot;

#if 0
#define PG_NX (1ul << 63)
#else
#define PG_NX 0
#endif

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = PG_NX;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = 0;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = PG_RW | PG_NX;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
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
	vm_paddr_t pa;
	vm_size_t size;
{
	vm_offset_t va, tmpva, offset;

	/* If this fits within the direct map window, use it */
	if (pa < dmaplimit && (pa + size) < dmaplimit)
		return ((void *)PHYS_TO_DMAP(pa));
	offset = pa & PAGE_MASK;
	size = roundup(offset + size, PAGE_SIZE);
	va = kmem_alloc_pageable(kernel_map, size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");
	pa = pa & PG_FRAME;
	for (tmpva = va; size > 0; ) {
		pmap_kenter(tmpva, pa);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, va, tmpva);
	return ((void *)(va + offset));
}

void
pmap_unmapdev(va, size)
	vm_offset_t va;
	vm_size_t size;
{
	vm_offset_t base, offset, tmpva;
	pt_entry_t *pte;

	/* If we gave a direct map region in pmap_mapdev, do nothing */
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS)
		return;
	base = va & PG_FRAME;
	offset = va & PAGE_MASK;
	size = roundup(offset + size, PAGE_SIZE);
	for (tmpva = base; tmpva < (base + size); tmpva += PAGE_SIZE) {
		pte = vtopte(tmpva);
		pte_clear(pte);
	}
	pmap_invalidate_range(kernel_pmap, va, tmpva);
	kmem_free(kernel_map, base, size);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap, addr)
	pmap_t pmap;
	vm_offset_t addr;
{
	pt_entry_t *ptep, pte;
	vm_page_t m;
	int val = 0;
	
	ptep = pmap_pte(pmap, addr);
	if (ptep == 0) {
		return 0;
	}

	if ((pte = *ptep) != 0) {
		vm_paddr_t pa;

		val = MINCORE_INCORE;
		if ((pte & PG_MANAGED) == 0)
			return val;

		pa = pte & PG_FRAME;

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if (pte & PG_M)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		else {
			/*
			 * Modified by someone else
			 */
			vm_page_lock_queues();
			if (m->dirty || pmap_is_modified(m))
				val |= MINCORE_MODIFIED_OTHER;
			vm_page_unlock_queues();
		}
		/*
		 * Referenced by us
		 */
		if (pte & PG_A)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;
		else {
			/*
			 * Referenced by someone else
			 */
			vm_page_lock_queues();
			if ((m->flags & PG_REFERENCED) ||
			    pmap_ts_referenced(m)) {
				val |= MINCORE_REFERENCED_OTHER;
				vm_page_flag_set(m, PG_REFERENCED);
			}
			vm_page_unlock_queues();
		}
	} 
	return val;
}

void
pmap_activate(struct thread *td)
{
	struct proc *p = td->td_proc;
	pmap_t	pmap;
	u_int64_t  cr3;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	pmap->pm_active |= PCPU_GET(cpumask);
	cr3 = vtophys(pmap->pm_pml4);
	/* XXXKSE this is wrong.
	 * pmap_activate is for the current thread on the current cpu
	 */
	if (p->p_flag & P_SA) {
		/* Make sure all other cr3 entries are updated. */
		/* what if they are running?  XXXKSE (maybe abort them) */
		FOREACH_THREAD_IN_PROC(p, td) {
			td->td_pcb->pcb_cr3 = cr3;
		}
	} else {
		td->td_pcb->pcb_cr3 = cr3;
	}
	load_cr3(cr3);
	critical_exit();
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{

	if ((obj == NULL) || (size < NBPDR) || (obj->type != OBJT_DEVICE)) {
		return addr;
	}

	addr = (addr + (NBPDR - 1)) & ~(NBPDR - 1);
	return addr;
}
