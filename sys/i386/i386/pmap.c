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
 *	$Id: pmap.c,v 1.108 1996/06/26 05:05:52 dyson Exp $
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

#include <machine/pcb.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>

#define PMAP_KEEP_PDIRS

#if defined(DIAGNOSTIC)
#define PMAP_DIAGNOSTIC
#endif

#if !defined(SMALL_KERNEL)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

static void	init_pv_entries __P((int));

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[(vm_offset_t)(v) >> PDRSHIFT]))
#define pdir_pde(m, v) (m[(vm_offset_t)(v) >> PDRSHIFT])

#define pmap_pde_v(pte)		((*(int *)pte & PG_V) != 0)
#define pmap_pte_w(pte)		((*(int *)pte & PG_W) != 0)
#define pmap_pte_m(pte)		((*(int *)pte & PG_M) != 0)
#define pmap_pte_u(pte)		((*(int *)pte & PG_A) != 0)
#define pmap_pte_v(pte)		((*(int *)pte & PG_V) != 0)

#define pmap_pte_set_w(pte, v) ((v)?(*(int *)pte |= PG_W):(*(int *)pte &= ~PG_W))
#define pmap_pte_set_prot(pte, v) ((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
static int protection_codes[8];

static struct pmap kernel_pmap_store;
pmap_t kernel_pmap;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */
static vm_offset_t vm_first_phys;

static int nkpt;
static vm_page_t nkpg;
vm_offset_t kernel_vm_end;

extern vm_offset_t clean_sva, clean_eva;
extern int cpu_class;

#define PV_FREELIST_MIN ((PAGE_SIZE / sizeof (struct pv_entry)) / 2)

/*
 * Data for the pv entry allocation mechanism
 */
static int pv_freelistcnt;
TAILQ_HEAD (,pv_entry) pv_freelist;
static vm_offset_t pvva;
static int npvvapg;

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1;
static pt_entry_t *CMAP2, *ptmmap;
caddr_t CADDR1, ptvmmap;
static caddr_t CADDR2;
static pt_entry_t *msgbufmap;
struct msgbuf *msgbufp;

pt_entry_t *PMAP1;
unsigned *PADDR1;

static void	free_pv_entry __P((pv_entry_t pv));
static unsigned * get_ptbase __P((pmap_t pmap));
static pv_entry_t get_pv_entry __P((void));
static void	i386_protection_init __P((void));
static void	pmap_alloc_pv_entry __P((void));

static int	pmap_is_managed __P((vm_offset_t pa));
static int	pmap_remove_all __P((vm_offset_t pa));
static vm_page_t pmap_enter_quick __P((pmap_t pmap, vm_offset_t va,
				      vm_offset_t pa, vm_page_t mpte));
static int pmap_remove_pte __P((struct pmap *pmap, unsigned *ptq,
					vm_offset_t sva));
static void pmap_remove_page __P((struct pmap *pmap, vm_offset_t va));
static int pmap_remove_entry __P((struct pmap *pmap, pv_table_t *pv,
					vm_offset_t va));
static void pmap_insert_entry __P((pmap_t pmap, vm_offset_t va,
		vm_page_t mpte, vm_offset_t pa));

static vm_page_t pmap_allocpte __P((pmap_t pmap, vm_offset_t va));

static int pmap_release_free_page __P((pmap_t pmap, vm_page_t p));
static vm_page_t _pmap_allocpte __P((pmap_t pmap, int ptepindex));
unsigned * __pure pmap_pte_quick __P((pmap_t pmap, vm_offset_t va));
int pmap_tcbit __P((vm_offset_t pa, int bit));
static vm_page_t pmap_page_alloc __P((vm_object_t object, vm_pindex_t pindex));

#define PDSTACKMAX 6
static vm_offset_t pdstack[PDSTACKMAX];
static int pdstackptr;

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On the i386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
void
pmap_bootstrap(firstaddr, loadaddr)
	vm_offset_t firstaddr;
	vm_offset_t loadaddr;
{
	vm_offset_t va;
	pt_entry_t *pte;

	avail_start = firstaddr;

	/*
	 * XXX The calculation of virtual_avail is wrong. It's NKPT*PAGE_SIZE too
	 * large. It should instead be correctly calculated in locore.s and
	 * not based on 'first' (which is a physical address, not a virtual
	 * address, for the start of unused physical memory). The kernel
	 * page tables are NOT double mapped and thus should not be included
	 * in this calculation.
	 */
	virtual_avail = (vm_offset_t) KERNBASE + firstaddr;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	i386_protection_init();

	/*
	 * The kernel's pmap is statically allocated so we don't have to use
	 * pmap_create, which is unlikely to work correctly at this part of
	 * the boot sequence (XXX and which no longer exists).
	 */
	kernel_pmap = &kernel_pmap_store;

	kernel_pmap->pm_pdir = (pd_entry_t *) (KERNBASE + IdlePTD);

	kernel_pmap->pm_count = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist.pv_list);
	nkpt = NKPT;

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = (pt_entry_t *) pmap_pte(kernel_pmap, va);

	/*
	 * CMAP1/CMAP2 are used for zeroing and copying pages.
	 */
	SYSMAP(caddr_t, CMAP1, CADDR1, 1)
	SYSMAP(caddr_t, CMAP2, CADDR2, 1)

	/*
	 * ptmmap is used for reading arbitrary physical pages via /dev/mem.
	 */
	SYSMAP(caddr_t, ptmmap, ptvmmap, 1)

	/*
	 * msgbufmap is used to map the system message buffer.
	 */
	SYSMAP(struct msgbuf *, msgbufmap, msgbufp, 1)

	/*
	 * ptemap is used for pmap_pte_quick
	 */
	SYSMAP(unsigned *, PMAP1, PADDR1, 1);

	virtual_avail = va;

	*(int *) CMAP1 = *(int *) CMAP2 = *(int *) PTD = 0;
	pmap_update();

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
	vm_offset_t addr;
	vm_size_t npg, s;
	int i;

	/*
	 * calculate the number of pv_entries needed
	 */
	vm_first_phys = phys_avail[0];
	for (i = 0; phys_avail[i + 1]; i += 2);
	npg = (phys_avail[(i - 2) + 1] - vm_first_phys) / PAGE_SIZE;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */
	s = (vm_size_t) (sizeof(pv_table_t) * npg);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	pv_table = (pv_table_t *) addr;
	for(i=0;i<npg;i++) {
		pv_table[i].pv_list_count = 0;
		TAILQ_INIT(&pv_table[i].pv_list);
	}
	TAILQ_INIT(&pv_freelist);

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
	vm_offset_t virt;
	vm_offset_t start;
	vm_offset_t end;
	int prot;
{
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return (virt);
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
pmap_nw_modified(pt_entry_t ptea) {
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
pmap_track_modified( vm_offset_t va) {
	if ((va < clean_sva) || (va >= clean_eva)) 
		return 1;
	else
		return 0;
}

/*
 * The below are finer grained pmap_update routines.  These eliminate
 * the gratuitious tlb flushes on non-i386 architectures.
 */
static PMAP_INLINE void
pmap_update_1pg( vm_offset_t va) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		pmap_update();
	else
#endif
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va));
}

static PMAP_INLINE void
pmap_update_2pg( vm_offset_t va1, vm_offset_t va2) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386) {
		pmap_update();
	} else
#endif
	{
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va1));
		__asm __volatile(".byte 0xf,0x1,0x38": :"a" (va2));
	}
}

static __pure unsigned *
get_ptbase(pmap)
	pmap_t pmap;
{
	unsigned frame = (unsigned) pmap->pm_pdir[PTDPTDI] & PG_FRAME;

	/* are we current address space or kernel? */
	if (pmap == kernel_pmap || frame == (((unsigned) PTDpde) & PG_FRAME)) {
		return (unsigned *) PTmap;
	}
	/* otherwise, we are alternate address space */
	if (frame != (((unsigned) APTDpde) & PG_FRAME)) {
		APTDpde = (pd_entry_t) (frame | PG_RW | PG_V);
		pmap_update();
	}
	return (unsigned *) APTmap;
}

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 */

unsigned * __pure
pmap_pte(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	if (pmap && *pmap_pde(pmap, va)) {
		return get_ptbase(pmap) + i386_btop(va);
	}
	return (0);
}

/*
 * Super fast pmap_pte routine best used when scanning
 * the pv lists.  This eliminates many coarse-grained
 * pmap_update calls.
 */
unsigned * __pure
pmap_pte_quick(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	unsigned pde, newpf;
	if (pde = (unsigned) pmap->pm_pdir[va >> PDRSHIFT]) {
		unsigned frame = (unsigned) pmap->pm_pdir[PTDPTDI] & PG_FRAME;
		/* are we current address space or kernel? */
		if (pmap == kernel_pmap || frame == (((unsigned) PTDpde) & PG_FRAME)) {
			return (unsigned *) PTmap + i386_btop(va);
		}
		newpf = pde & PG_FRAME;
		if ( ((* (unsigned *) PMAP1) & PG_FRAME) != newpf) {
			* (unsigned *) PMAP1 = newpf | PG_RW | PG_V;
			pmap_update_1pg((vm_offset_t) PADDR1);
		}
		return PADDR1 + ((unsigned) i386_btop(va) & (NPTEPG - 1));
	}
	return (0);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_offset_t __pure
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	if (pmap && *pmap_pde(pmap, va)) {
		unsigned *pte;
		pte = get_ptbase(pmap) + i386_btop(va);
		return ((*pte & PG_FRAME) | (va & PAGE_MASK));
	}
	return 0;

}

/*
 * determine if a page is managed (memory vs. device)
 */
static PMAP_INLINE __pure int
pmap_is_managed(pa)
	vm_offset_t pa;
{
	int i;

	if (!pmap_initialized)
		return 0;

	for (i = 0; phys_avail[i + 1]; i += 2) {
		if (pa < phys_avail[i + 1] && pa >= phys_avail[i])
			return 1;
	}
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
pmap_qenter(va, m, count)
	vm_offset_t va;
	vm_page_t *m;
	int count;
{
	int i;
	register unsigned *pte;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		unsigned npte = VM_PAGE_TO_PHYS(m[i]) | PG_RW | PG_V;
		unsigned opte;
		pte = (unsigned *)vtopte(tva);
		opte = *pte;
		*pte = npte;
		if (opte & PG_V)
			pmap_update_1pg(tva);
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
	register unsigned *pte;

	for (i = 0; i < count; i++) {
		pte = (unsigned *)vtopte(va);
		*pte = 0;
		pmap_update_1pg(va);
		va += PAGE_SIZE;
	}
}

/*
 * add a wired page to the kva
 * note that in order for the mapping to take effect -- you
 * should do a pmap_update after doing the pmap_kenter...
 */
PMAP_INLINE void 
pmap_kenter(va, pa)
	vm_offset_t va;
	register vm_offset_t pa;
{
	register unsigned *pte;
	unsigned npte, opte;

	npte = pa | PG_RW | PG_V;
	pte = (unsigned *)vtopte(va);
	opte = *pte;
	*pte = npte;
	if (opte & PG_V)
		pmap_update_1pg(va);
}

/*
 * remove a page from the kernel pagetables
 */
PMAP_INLINE void
pmap_kremove(va)
	vm_offset_t va;
{
	register unsigned *pte;

	pte = (unsigned *)vtopte(va);
	*pte = 0;
	pmap_update_1pg(va);
}

static vm_page_t
pmap_page_alloc(object, pindex)
	vm_object_t object;
	vm_pindex_t pindex;
{
	vm_page_t m;
	m = vm_page_alloc(object, pindex, VM_ALLOC_ZERO);
	if (m == NULL) {
		VM_WAIT;
	}
	return m;
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{
	vm_page_t ptdpg;
	/*
	 * No need to allocate page table space yet but we do need a valid
	 * page directory table.
	 */

	if (pdstackptr > 0) {
		--pdstackptr;
		pmap->pm_pdir = (pd_entry_t *)pdstack[pdstackptr];
	} else {
		pmap->pm_pdir =
			(pd_entry_t *)kmem_alloc_pageable(kernel_map, PAGE_SIZE);
	}

	/*
	 * allocate object for the ptes
	 */
	pmap->pm_pteobj = vm_object_allocate( OBJT_DEFAULT, PTDPTDI + 1);

	/*
	 * allocate the page directory page
	 */
retry:
	ptdpg = pmap_page_alloc( pmap->pm_pteobj, PTDPTDI);
	if (ptdpg == NULL)
		goto retry;

	ptdpg->wire_count = 1;
	++cnt.v_wire_count;
	ptdpg->flags &= ~(PG_MAPPED|PG_BUSY);	/* not mapped normally */
	ptdpg->valid = VM_PAGE_BITS_ALL;

	pmap_kenter((vm_offset_t) pmap->pm_pdir, VM_PAGE_TO_PHYS(ptdpg));
	if ((ptdpg->flags & PG_ZERO) == 0)
		bzero(pmap->pm_pdir, PAGE_SIZE);

	/* wire in kernel global address entries */
	bcopy(PTD + KPTDI, pmap->pm_pdir + KPTDI, nkpt * PTESIZE);

	/* install self-referential address mapping entry */
	*(unsigned *) (pmap->pm_pdir + PTDPTDI) =
		VM_PAGE_TO_PHYS(ptdpg) | PG_V | PG_RW;

	pmap->pm_count = 1;
	pmap->pm_ptphint = NULL;
	TAILQ_INIT(&pmap->pm_pvlist.pv_list);
}

static int
pmap_release_free_page(pmap, p)
	struct pmap *pmap;
	vm_page_t p;
{
	int s;
	unsigned *pde = (unsigned *) pmap->pm_pdir;
	/*
	 * This code optimizes the case of freeing non-busy
	 * page-table pages.  Those pages are zero now, and
	 * might as well be placed directly into the zero queue.
	 */
	s = splvm();
	if (p->flags & PG_BUSY) {
		p->flags |= PG_WANTED;
		tsleep(p, PVM, "pmaprl", 0);
		splx(s);
		return 0;
	}

	/*
	 * Remove the page table page from the processes address space.
	 */
	pde[p->pindex] = 0;
	--pmap->pm_stats.resident_count;

	if (p->hold_count)  {
		panic("pmap_release: freeing held page table page");
	}
	/*
	 * Page directory pages need to have the kernel
	 * stuff cleared, so they can go into the zero queue also.
	 */
	if (p->pindex == PTDPTDI) {
		bzero(pde + KPTDI, nkpt * PTESIZE);
		pde[APTDPTDI] = 0;
		pmap_kremove((vm_offset_t) pmap->pm_pdir);
	}

	if (pmap->pm_ptphint == p)
		pmap->pm_ptphint = NULL;

	vm_page_free_zero(p);
	splx(s);
	return 1;
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap, ptepindex)
	pmap_t	pmap;
	int ptepindex;
{
	vm_offset_t ptepa;
	vm_page_t m;

	/*
	 * Find or fabricate a new pagetable page
	 */
retry:
	m = vm_page_lookup(pmap->pm_pteobj, ptepindex);
	if (m == NULL) {
		m = pmap_page_alloc(pmap->pm_pteobj, ptepindex);
		if (m == NULL)
			goto retry;
		if ((m->flags & PG_ZERO) == 0)
			pmap_zero_page(VM_PAGE_TO_PHYS(m));
		m->flags &= ~(PG_ZERO|PG_BUSY);
		m->valid = VM_PAGE_BITS_ALL;
	} else {
		if ((m->flags & PG_BUSY) || m->busy) {
			m->flags |= PG_WANTED;
			tsleep(m, PVM, "ptewai", 0);
			goto retry;
		}
	}

	if (m->queue != PQ_NONE) {
		int s = splvm();
		vm_page_unqueue(m,1);
		splx(s);
	}

	if (m->wire_count == 0)
		++cnt.v_wire_count;
	++m->wire_count;

	/*
	 * Increment the hold count for the page table page
	 * (denoting a new mapping.)
	 */
	++m->hold_count;

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	pmap->pm_stats.resident_count++;

	ptepa = VM_PAGE_TO_PHYS(m);
	pmap->pm_pdir[ptepindex] = (pd_entry_t) (ptepa | PG_U | PG_RW | PG_V);

	m->flags |= PG_MAPPED;

	return m;
}

PMAP_INLINE static vm_page_t
pmap_allocpte(pmap, va)
	pmap_t	pmap;
	vm_offset_t va;
{
	int ptepindex;
	vm_offset_t ptepa;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = va >> PDRSHIFT;

	/*
	 * Get the page directory entry
	 */
	ptepa = (vm_offset_t) pmap->pm_pdir[ptepindex];

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (ptepa) {
		if (pmap->pm_ptphint &&
			(pmap->pm_ptphint->pindex == ptepindex)) {
			m = pmap->pm_ptphint;
		} else {
			m = vm_page_lookup( pmap->pm_pteobj, ptepindex);
			pmap->pm_ptphint = m;
		}
		++m->hold_count;
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
pmap_release(pmap)
	register struct pmap *pmap;
{
	vm_page_t p,n,ptdpg;
	vm_object_t object = pmap->pm_pteobj;

	if (object->ref_count != 1)
		panic("pmap_release: pteobj reference count != 1");
	
	ptdpg = NULL;
retry:
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = n) {
		n = TAILQ_NEXT(p, listq);
		if (p->pindex == PTDPTDI) {
			ptdpg = p;
			continue;
		}
		if (!pmap_release_free_page(pmap, p))
			goto retry;
	}
	if (ptdpg == NULL)
		panic("pmap_release: missing page table directory page");

	if (!pmap_release_free_page(pmap, ptdpg))
		goto retry;
		
	vm_object_deallocate(object);
	if (pdstackptr < PDSTACKMAX) {
		pdstack[pdstackptr] = (vm_offset_t) pmap->pm_pdir;
		++pdstackptr;
	} else {
		kmem_free(kernel_map, (vm_offset_t) pmap->pm_pdir, PAGE_SIZE);
	}
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	struct proc *p;
	struct pmap *pmap;
	int s;

	s = splhigh();
	if (kernel_vm_end == 0) {
		kernel_vm_end = KERNBASE;
		nkpt = 0;
		while (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			++nkpt;
		}
	}
	addr = (addr + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	while (kernel_vm_end < addr) {
		if (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			continue;
		}
		++nkpt;
		if (!nkpg) {
			nkpg = vm_page_alloc(kernel_object, 0, VM_ALLOC_SYSTEM);
			if (!nkpg)
				panic("pmap_growkernel: no memory to grow kernel");
			vm_page_wire(nkpg);
			vm_page_remove(nkpg);
			pmap_zero_page(VM_PAGE_TO_PHYS(nkpg));
		}
		pdir_pde(PTD, kernel_vm_end) = (pd_entry_t) (VM_PAGE_TO_PHYS(nkpg) | PG_V | PG_RW);
		nkpg = NULL;

		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			if (p->p_vmspace) {
				pmap = &p->p_vmspace->vm_pmap;
				*pmap_pde(pmap, kernel_vm_end) = pdir_pde(PTD, kernel_vm_end);
			}
		}
		*pmap_pde(kernel_pmap, kernel_vm_end) = pdir_pde(PTD, kernel_vm_end);
		kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	}
	splx(s);
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

	count = --pmap->pm_count;
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t) pmap, M_VMPMAP);
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t pmap;
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
free_pv_entry(pv)
	pv_entry_t pv;
{
	++pv_freelistcnt;
	TAILQ_INSERT_HEAD(&pv_freelist, pv, pv_list);
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static PMAP_INLINE pv_entry_t
get_pv_entry()
{
	pv_entry_t tmp;

	/*
	 * get more pv_entry pages if needed
	 */
	if (pv_freelistcnt < PV_FREELIST_MIN) {
		pmap_alloc_pv_entry();
	}

	/*
	 * get a pv_entry off of the free list
	 */
	--pv_freelistcnt;
	tmp = TAILQ_FIRST(&pv_freelist);
	TAILQ_REMOVE(&pv_freelist, tmp, pv_list);
	return tmp;
}

/*
 * This *strange* allocation routine eliminates the possibility of a malloc
 * failure (*FATAL*) for a pv_entry_t data structure.
 * also -- this code is MUCH MUCH faster than the malloc equiv...
 * We really need to do the slab allocator thingie here.
 */
static void
pmap_alloc_pv_entry()
{
	/*
	 * do we have any pre-allocated map-pages left?
	 */
	if (npvvapg) {
		vm_page_t m;

		/*
		 * allocate a physical page out of the vm system
		 */
		m = vm_page_alloc(kernel_object,
		    OFF_TO_IDX(pvva - vm_map_min(kernel_map)),
		    VM_ALLOC_INTERRUPT);
		if (m) {
			int newentries;
			int i;
			pv_entry_t entry;

			newentries = (PAGE_SIZE / sizeof(struct pv_entry));
			/*
			 * wire the page
			 */
			vm_page_wire(m);
			m->flags &= ~PG_BUSY;
			/*
			 * let the kernel see it
			 */
			pmap_kenter(pvva, VM_PAGE_TO_PHYS(m));

			entry = (pv_entry_t) pvva;
			/*
			 * update the allocation pointers
			 */
			pvva += PAGE_SIZE;
			--npvvapg;

			/*
			 * free the entries into the free list
			 */
			for (i = 0; i < newentries; i++) {
				free_pv_entry(entry);
				entry++;
			}
		}
	}
	if (TAILQ_FIRST(&pv_freelist) == NULL)
		panic("get_pv_entry: cannot get a pv_entry_t");
}

/*
 * init the pv_entry allocation system
 */
#define PVSPERPAGE 64
void
init_pv_entries(npg)
	int npg;
{
	/*
	 * allocate enough kvm space for PVSPERPAGE entries per page (lots)
	 * kvm space is fairly cheap, be generous!!!  (the system can panic if
	 * this is too small.)
	 */
	npvvapg = ((npg * PVSPERPAGE) * sizeof(struct pv_entry)
		+ PAGE_SIZE - 1) / PAGE_SIZE;
	pvva = kmem_alloc_pageable(kernel_map, npvvapg * PAGE_SIZE);
	/*
	 * get the first batch of entries
	 */
	pmap_alloc_pv_entry();
}

/*
 * This routine unholds page table pages, and if the hold count
 * drops to zero, then it decrements the wire count.
 */
static int
pmap_unwire_pte_hold(pmap_t pmap, vm_page_t m) {
	vm_page_unhold(m);
	if (m->hold_count == 0) {
		vm_offset_t pteva;
		/*
		 * unmap the page table page
		 */
		pmap->pm_pdir[m->pindex] = 0;
		--pmap->pm_stats.resident_count;
		if ((((unsigned)pmap->pm_pdir[PTDPTDI]) & PG_FRAME) ==
			(((unsigned) PTDpde) & PG_FRAME)) {
			/*
			 * Do a pmap_update to make the invalidated mapping
			 * take effect immediately.
			 */
			pteva = UPT_MIN_ADDRESS + i386_ptob(m->pindex);
			pmap_update_1pg(pteva);
		}
		/*
		 * If the page is finally unwired, simply free it.
		 */
		--m->wire_count;
		if (m->wire_count == 0) {
			if (pmap->pm_ptphint == m)
				pmap->pm_ptphint = NULL;
			vm_page_free_zero(m);
			--cnt.v_wire_count;
		}
		return 1;
	}
	return 0;
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
PMAP_INLINE int
pmap_unuse_pt(pmap, va, mpte)
	pmap_t pmap;
	vm_offset_t va;
	vm_page_t mpte;
{
	int ptepindex;
	if (va >= UPT_MIN_ADDRESS)
		return 0;

	if (mpte == NULL) {
		ptepindex = (va >> PDRSHIFT);
		if (pmap->pm_ptphint &&
			pmap->pm_ptphint->pindex == ptepindex) {
			mpte = pmap->pm_ptphint;
		} else {
			mpte = vm_page_lookup( pmap->pm_pteobj, ptepindex);
			pmap->pm_ptphint = mpte;
		}
	}

	return pmap_unwire_pte_hold(pmap, mpte);
}

/*
 * If it is the first entry on the list, it is actually
 * in the header and we must copy the following entry up
 * to the header.  Otherwise we must search the list for
 * the entry.  In either case we free the now unused entry.
 */
static int
pmap_remove_entry(pmap, ppv, va)
	struct pmap *pmap;
	pv_table_t *ppv;
	vm_offset_t va;
{
	pv_entry_t pv;
	int rtval;
	int s;

	s = splvm();
	if (ppv->pv_list_count < pmap->pm_stats.resident_count) {
		for (pv = TAILQ_FIRST(&ppv->pv_list);
			pv;
			pv = TAILQ_NEXT(pv, pv_list)) {
			if (pmap == pv->pv_pmap && va == pv->pv_va) 
				break;
		}
	} else {
		for (pv = TAILQ_FIRST(&pmap->pm_pvlist.pv_list);
			pv;
			pv = TAILQ_NEXT(pv, pv_plist)) {
			if (va == pv->pv_va) 
				break;
		}
	}

	rtval = 0;
	if (pv) {
		rtval = pmap_unuse_pt(pmap, va, pv->pv_ptem);
		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		--ppv->pv_list_count;
		TAILQ_REMOVE(&pmap->pm_pvlist.pv_list, pv, pv_plist);
		free_pv_entry(pv);
	}
			
	splx(s);
	return rtval;
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap, ptq, va)
	struct pmap *pmap;
	unsigned *ptq;
	vm_offset_t va;
{
	unsigned oldpte;

	oldpte = *ptq;
	*ptq = 0;
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	pmap->pm_stats.resident_count -= 1;
	if (oldpte & PG_MANAGED) {
		if (oldpte & PG_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified((pt_entry_t) oldpte)) {
				printf("pmap_remove: modified page not writable: va: 0x%lx, pte: 0x%lx\n", va, (int) oldpte);
			}
#endif
			if (pmap_track_modified(va))
				PHYS_TO_VM_PAGE(oldpte)->dirty = VM_PAGE_BITS_ALL;
		}
		return pmap_remove_entry(pmap, pa_to_pvh(oldpte), va);
	} else {
		return pmap_unuse_pt(pmap, va, NULL);
	}

	return 0;
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap, va)
	struct pmap *pmap;
	register vm_offset_t va;
{
	register unsigned *ptq;

	/*
	 * if there is no pte for this address, just skip it!!!
	 */
	if (*pmap_pde(pmap, va) == 0) {
		return;
	}

	/*
	 * get a local va for mappings for this pmap.
	 */
	ptq = get_ptbase(pmap) + i386_btop(va);
	if (*ptq & PG_V) {
		(void) pmap_remove_pte(pmap, ptq, va);
		if ((((unsigned)pmap->pm_pdir[PTDPTDI]) & PG_FRAME) == (((unsigned) PTDpde) & PG_FRAME)) {
			pmap_update_1pg(va);
		}
	}
	return;
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
	register unsigned *ptbase;
	vm_offset_t pdnxt;
	vm_offset_t ptpaddr;
	vm_offset_t sindex, eindex;
	vm_page_t mpte;
	int anyvalid;

	if (pmap == NULL)
		return;

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if ((sva + PAGE_SIZE) == eva) {
		pmap_remove_page(pmap, sva);
		return;
	}

	anyvalid = 0;

	/*
	 * Get a local virtual address for the mappings that are being
	 * worked with.
	 */
	ptbase = get_ptbase(pmap);

	sindex = i386_btop(sva);
	eindex = i386_btop(eva);
	mpte = NULL;

	for (; sindex < eindex; sindex = pdnxt) {

		/*
		 * Calculate index for next page table.
		 */
		pdnxt = ((sindex + NPTEPG) & ~(NPTEPG - 1));
		ptpaddr = (vm_offset_t) *pmap_pde(pmap, i386_ptob(sindex));

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (pdnxt > eindex) {
			pdnxt = eindex;
		}

		for ( ;sindex != pdnxt; sindex++) {
			vm_offset_t va;
			if ((ptbase[sindex] & PG_V) == 0) {
				continue;
			}
			va = i386_ptob(sindex);
			
			anyvalid++;
			if (pmap_remove_pte(pmap,
				ptbase + sindex, va))
				break;
		}
	}

	if (anyvalid) {
		pmap_update();
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
static int
pmap_remove_all(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv, npv;
	pv_table_t *ppv;
	register unsigned *pte, tpte;
	vm_page_t m;
	int nmodify;
	int s;

	nmodify = 0;
#if defined(PMAP_DIAGNOSTIC)
	/*
	 * XXX this makes pmap_page_protect(NONE) illegal for non-managed
	 * pages!
	 */
	if (!pmap_is_managed(pa)) {
		panic("pmap_page_protect: illegal for unmanaged page, va: 0x%lx", pa);
	}
#endif

	s = splvm();
	m = NULL;
	ppv = pa_to_pvh(pa);
	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = npv) {
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
		if ((tpte = *pte) & PG_V) {
			pv->pv_pmap->pm_stats.resident_count--;
			*pte = 0;
			if (tpte & PG_W)
				pv->pv_pmap->pm_stats.wired_count--;
			/*
			 * Update the vm_page_t clean and reference bits.
			 */
			if ((tpte & (PG_M|PG_MANAGED)) == (PG_M|PG_MANAGED)) {
#if defined(PMAP_DIAGNOSTIC)
				if (pmap_nw_modified((pt_entry_t) tpte)) {
					printf("pmap_remove_all: modified page not writable: va: 0x%lx, pte: 0x%lx\n", pv->pv_va, tpte);
				}
#endif
				if (pmap_track_modified(pv->pv_va))
					nmodify += 1;
			}
		}
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist.pv_list, pv, pv_plist);

		npv = TAILQ_NEXT(pv, pv_list);
		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		--ppv->pv_list_count;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}

	splx(s);
	return nmodify;
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t pmap;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register unsigned *ptbase;
	vm_offset_t pdnxt;
	vm_offset_t ptpaddr;
	vm_offset_t sindex, eindex;
	vm_page_t mpte;
	int anyvalid;


	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	anyvalid = 0;

	ptbase = get_ptbase(pmap);

	sindex = i386_btop(sva);
	eindex = i386_btop(eva);

	mpte = NULL;
	for (; sindex < eindex; sindex = pdnxt) {

		pdnxt = ((sindex + NPTEPG) & ~(NPTEPG - 1));
		ptpaddr = (vm_offset_t) *pmap_pde(pmap, i386_ptob(sindex));

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		if (pdnxt > eindex) {
			pdnxt = eindex;
		}

		for (; sindex != pdnxt; sindex++) {

			unsigned pbits = ptbase[sindex];

			if ((pbits & (PG_RW|PG_V)) == (PG_RW|PG_V)) {
				if ((pbits & (PG_M|PG_MANAGED)) == (PG_M|PG_MANAGED)) {
					vm_offset_t sva = i386_ptob(sindex);
					if (pmap_track_modified(sva)) {
						vm_page_t m = PHYS_TO_VM_PAGE(pbits);
						m->dirty = VM_PAGE_BITS_ALL;
					}
				}
				ptbase[sindex] = pbits & ~(PG_M|PG_RW);
				anyvalid = 1;
			}
		}
	}
	if (anyvalid)
		pmap_update();
}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap, va, mpte, pa)
	pmap_t pmap;
	vm_offset_t va;
	vm_page_t mpte;
	vm_offset_t pa;
{

	int s;
	pv_entry_t pv;
	pv_table_t *ppv;

	s = splvm();
	pv = get_pv_entry();
	pv->pv_va = va;
	pv->pv_pmap = pmap;
	pv->pv_ptem = mpte;

	TAILQ_INSERT_TAIL(&pmap->pm_pvlist.pv_list, pv, pv_plist);

	ppv = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
	++ppv->pv_list_count;

	splx(s);
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
	register unsigned *pte;
	vm_offset_t opa;
	vm_offset_t origpte, newpte;
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
	if (va < UPT_MIN_ADDRESS)
		mpte = pmap_allocpte(pmap, va);

	pte = pmap_pte(pmap, va);
	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL) {
		panic("pmap_enter: invalid page directory, pdir=%p, va=0x%lx\n",
			pmap->pm_pdir[PTDPTDI], va);
	}

	origpte = *(vm_offset_t *)pte;
	pa &= PG_FRAME;
	opa = origpte & PG_FRAME;

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if ((origpte & PG_V) && (opa == pa)) {
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
			printf("pmap_enter: modified page not writable: va: 0x%lx, pte: 0x%lx\n", va, origpte);
		}
#endif

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (origpte & PG_MANAGED) {
			if ((origpte & PG_M) && pmap_track_modified(va)) {
				vm_page_t m;
				m = PHYS_TO_VM_PAGE(pa);
				m->dirty = VM_PAGE_BITS_ALL;
			}
			pa |= PG_MANAGED;
		}

		if (mpte)
			--mpte->hold_count;

		goto validate;
	} 
	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (origpte & PG_V) {
		int err;
		err = pmap_remove_pte(pmap, pte, va);
		if (err)
			panic("pmap_enter: pte vanished, va: 0x%x", va);
	}

	/*
	 * Enter on the PV list if part of our managed memory Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if (pmap_is_managed(pa)) {
		pmap_insert_entry(pmap, va, mpte, pa);
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
	newpte = (vm_offset_t) (pa | pte_prot(pmap, prot) | PG_V);

	if (wired)
		newpte |= PG_W;
	if (va < UPT_MIN_ADDRESS)
		newpte |= PG_U;

	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if ((origpte & ~(PG_M|PG_A)) != newpte) {
		*pte = newpte;
		if (origpte)
			pmap_update_1pg(va);
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
pmap_enter_quick(pmap, va, pa, mpte)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_page_t mpte;
{
	register unsigned *pte;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < UPT_MIN_ADDRESS) {
		int ptepindex;
		vm_offset_t ptepa;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = va >> PDRSHIFT;
		if (mpte && (mpte->pindex == ptepindex)) {
			++mpte->hold_count;
		} else {
			/*
			 * Get the page directory entry
			 */
			ptepa = (vm_offset_t) pmap->pm_pdir[ptepindex];

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (ptepa) {
				if (pmap->pm_ptphint &&
					pmap->pm_ptphint->pindex == ptepindex) {
					mpte = pmap->pm_ptphint;
				} else {
					mpte = vm_page_lookup( pmap->pm_pteobj, ptepindex);
					pmap->pm_ptphint = mpte;
				}
				++mpte->hold_count;
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
	pte = (unsigned *)vtopte(va);
	if (*pte & PG_V) {
		if (mpte)
			pmap_unwire_pte_hold(pmap, mpte);
		return NULL;
	}

	/*
	 * Enter on the PV list if part of our managed memory Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	pmap_insert_entry(pmap, va, mpte, pa);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	/*
	 * Now validate mapping with RO protection
	 */
	*pte = pa | PG_V | PG_U | PG_MANAGED;

	return mpte;
}

#define MAX_INIT_PT (96)
/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap, addr, object, pindex, size, limit)
	pmap_t pmap;
	vm_offset_t addr;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_size_t size;
	int limit;
{
	vm_offset_t tmpidx;
	int psize;
	vm_page_t p, mpte;
	int objpgs;

	psize = i386_btop(size);

	if (!pmap || (object->type != OBJT_VNODE) ||
		(limit && (psize > MAX_INIT_PT) &&
			(object->resident_page_count > MAX_INIT_PT))) {
		return;
	}

	if (psize + pindex > object->size)
		psize = object->size - pindex;

	mpte = NULL;
	/*
	 * if we are processing a major portion of the object, then scan the
	 * entire thing.
	 */
	if (psize > (object->size >> 2)) {
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
			    (p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if (p->queue == PQ_CACHE)
					vm_page_deactivate(p);
				p->flags |= PG_BUSY;
				mpte = pmap_enter_quick(pmap, 
					addr + i386_ptob(tmpidx),
					VM_PAGE_TO_PHYS(p), mpte);
				p->flags |= PG_MAPPED;
				PAGE_WAKEUP(p);
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
			    (p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if (p->queue == PQ_CACHE)
					vm_page_deactivate(p);
				p->flags |= PG_BUSY;
				mpte = pmap_enter_quick(pmap, 
					addr + i386_ptob(tmpidx),
					VM_PAGE_TO_PHYS(p), mpte);
				p->flags |= PG_MAPPED;
				PAGE_WAKEUP(p);
			}
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
#define PFBAK 2
#define PFFOR 2
#define PAGEORDER_SIZE (PFBAK+PFFOR)

static int pmap_prefault_pageorder[] = {
	-PAGE_SIZE, PAGE_SIZE, -2 * PAGE_SIZE, 2 * PAGE_SIZE
};

void
pmap_prefault(pmap, addra, entry, object)
	pmap_t pmap;
	vm_offset_t addra;
	vm_map_entry_t entry;
	vm_object_t object;
{
	int i;
	vm_offset_t starta;
	vm_offset_t addr;
	vm_pindex_t pindex;
	vm_page_t m, mpte;

	if (entry->object.vm_object != object)
		return;

	if (!curproc || (pmap != &curproc->p_vmspace->vm_pmap))
		return;

	starta = addra - PFBAK * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	mpte = NULL;
	for (i = 0; i < PAGEORDER_SIZE; i++) {
		vm_object_t lobject;
		unsigned *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr < starta || addr >= entry->end)
			continue;

		if ((*pmap_pde(pmap, addr)) == NULL) 
			continue;

		pte = (unsigned *) vtopte(addr);
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

		if (((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
		    (m->busy == 0) &&
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if (m->queue == PQ_CACHE) {
				vm_page_deactivate(m);
			}
			m->flags |= PG_BUSY;
			mpte = pmap_enter_quick(pmap, addr,
				VM_PAGE_TO_PHYS(m), mpte);
			m->flags |= PG_MAPPED;
			PAGE_WAKEUP(m);
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
	register unsigned *pte;

	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);

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
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap, src_pmap;
	vm_offset_t dst_addr;
	vm_size_t len;
	vm_offset_t src_addr;
{
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t pdnxt;
	unsigned src_frame, dst_frame;

	if (dst_addr != src_addr)
		return;

	src_frame = ((unsigned) src_pmap->pm_pdir[PTDPTDI]) & PG_FRAME;
	if (src_frame != (((unsigned) PTDpde) & PG_FRAME))
		return;

	dst_frame = ((unsigned) dst_pmap->pm_pdir[PTDPTDI]) & PG_FRAME;
	if (dst_frame != (((unsigned) APTDpde) & PG_FRAME)) {
		APTDpde = (pd_entry_t) (dst_frame | PG_RW | PG_V);
		pmap_update();
	}

	for(addr = src_addr; addr < end_addr; addr = pdnxt) {
		unsigned *src_pte, *dst_pte;
		vm_page_t dstmpte, srcmpte;
		vm_offset_t srcptepaddr;
		int ptepindex;

		if (addr >= UPT_MIN_ADDRESS)
			panic("pmap_copy: invalid to pmap_copy page tables\n");

		pdnxt = ((addr + PAGE_SIZE*NPTEPG) & ~(PAGE_SIZE*NPTEPG - 1));
		ptepindex = addr >> PDRSHIFT;

		srcptepaddr = (vm_offset_t) src_pmap->pm_pdir[ptepindex];
		if (srcptepaddr == 0)
			continue;

		srcmpte = vm_page_lookup(src_pmap->pm_pteobj, ptepindex);
		if (srcmpte->hold_count == 0)
			continue;

		if (pdnxt > end_addr)
			pdnxt = end_addr;

		src_pte = (unsigned *) vtopte(addr);
		dst_pte = (unsigned *) avtopte(addr);
		while (addr < pdnxt) {
			unsigned ptetemp;
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
				if ((*dst_pte == 0) && (ptetemp = *src_pte)) {
					/*
					 * Clear the modified and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					*dst_pte = ptetemp & ~(PG_M|PG_A);
					dst_pmap->pm_stats.resident_count++;
					pmap_insert_entry(dst_pmap, addr,
						dstmpte,
						(ptetemp & PG_FRAME));
	 			} else {
					pmap_unwire_pte_hold(dst_pmap, dstmpte);
				}
				if (dstmpte->hold_count >= srcmpte->hold_count)
					break;
			}
			addr += PAGE_SIZE;
			++src_pte;
			++dst_pte;
		}
	}
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
	if (*(int *) CMAP2)
		panic("pmap_zero_page: CMAP busy");

	*(int *) CMAP2 = PG_V | PG_RW | (phys & PG_FRAME);
	bzero(CADDR2, PAGE_SIZE);
	*(int *) CMAP2 = 0;
	pmap_update_1pg((vm_offset_t) CADDR2);
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
	if (*(int *) CMAP1 || *(int *) CMAP2)
		panic("pmap_copy_page: CMAP busy");

	*(int *) CMAP1 = PG_V | PG_RW | (src & PG_FRAME);
	*(int *) CMAP2 = PG_V | PG_RW | (dst & PG_FRAME);

	bcopy(CADDR1, CADDR2, PAGE_SIZE);

	*(int *) CMAP1 = 0;
	*(int *) CMAP2 = 0;
	pmap_update_2pg( (vm_offset_t) CADDR1, (vm_offset_t) CADDR2);
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
pmap_page_exists(pmap, pa)
	pmap_t pmap;
	vm_offset_t pa;
{
	register pv_entry_t pv;
	pv_table_t *ppv;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	s = splvm();

	ppv = pa_to_pvh(pa);
	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	for (pv = TAILQ_FIRST(&ppv->pv_list);
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
 * is special cased for current process only.
 */
void
pmap_remove_pages(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	unsigned *pte, tpte;
	pv_table_t *ppv;
	pv_entry_t pv, npv;
	int s;

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (pmap != &curproc->p_vmspace->vm_pmap) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
#endif

	s = splvm();

	for(pv = TAILQ_FIRST(&pmap->pm_pvlist.pv_list);
		pv;
		pv = npv) {

		if (pv->pv_va >= eva || pv->pv_va < sva) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
		pte = (unsigned *)vtopte(pv->pv_va);
#else
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
#endif
		tpte = *pte;
		*pte = 0;

		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist.pv_list, pv, pv_plist);

		if (tpte & PG_V) {
			pv->pv_pmap->pm_stats.resident_count--;
			if (tpte & PG_W)
				pv->pv_pmap->pm_stats.wired_count--;
			/*
			 * Update the vm_page_t clean and reference bits.
			 */
			if (tpte & PG_M) {
				PHYS_TO_VM_PAGE(tpte)->dirty = VM_PAGE_BITS_ALL;
			}
		}

		npv = TAILQ_NEXT(pv, pv_plist);

		ppv = pa_to_pvh(tpte);
		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		--ppv->pv_list_count;

		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}
	pmap_update();
	splx(s);
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return (i386_ptob(ppn));
}

/*
 *	pmap_tcbit:
 *
 *	Return the count of bits for a page, clearing all of them.
 *	
 */
int
pmap_tcbit(vm_offset_t pa, int bit)
{
	register pv_entry_t pv, npv;
	pv_table_t *ppv;
	unsigned *pte;
	int s;
	int rtval = 0;

	s = splvm();

	ppv = pa_to_pvh(pa);
	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = npv) {
		npv = TAILQ_NEXT(pv, pv_list);
		/*
		 * if the bit being tested is the modified bit, then
		 * mark clean_map and ptes as never
		 * modified.
		 */
		if (((bit & PG_M) != 0)
			&& !pmap_track_modified(pv->pv_va))
			continue;

		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
		if (pte == NULL)
			continue;

		if ((rtval == 0) && (*pte & bit)) {
			rtval = 1;
			*pte &= ~bit;
		} else {
			*pte &= ~bit;
		}
	}
	splx(s);
	if (rtval) {
		if (curproc != pageproc)
			pmap_update();
	}
	return (rtval);
}

/*
 *	pmap_tc_modified:
 *
 *	Return the count of modified bits for a page, clearing all of them.
 *	
 */
int
pmap_tc_modified(vm_page_t m)
{
	int rtval;

	rtval = pmap_tcbit(VM_PAGE_TO_PHYS(m), PG_M);
	if (rtval)
		m->dirty = VM_PAGE_BITS_ALL;

	return rtval;
}

/*
 *	pmap_tc_referenced:
 *
 *	Return the count of referenced bits for a page, clearing all of them.
 *	
 */
int
pmap_tc_referenced(vm_offset_t pa)
{
	if (!pmap_is_managed(pa))
		return 0;
	return pmap_tcbit(pa, PG_A);
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(m, prot)
	vm_page_t m;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
			if ((m->flags & PG_FICTITIOUS) == 0)
				pmap_tcbit(VM_PAGE_TO_PHYS(m), PG_RW);
		} else {
			if (pmap_remove_all(VM_PAGE_TO_PHYS(m))) {
				m->dirty = VM_PAGE_BITS_ALL;
			}
			pmap_update();
		}
	}
}

/*
 * Miscellaneous support routines follow
 */

static void
i386_protection_init()
{
	register int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			/*
			 * Read access is also 0. There isn't any execute bit,
			 * so just make it readable.
			 */
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = 0;
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

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory. The non-cacheable bits are set on each
 * mapped page.
 */
void *
pmap_mapdev(pa, size)
	vm_offset_t pa;
	vm_size_t size;
{
	vm_offset_t va, tmpva;
	unsigned *pte;

	size = roundup(size, PAGE_SIZE);

	va = kmem_alloc_pageable(kernel_map, size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	pa = pa & PG_FRAME;
	for (tmpva = va; size > 0;) {
		pte = (unsigned *)vtopte(tmpva);
		*pte = pa | PG_RW | PG_V | PG_N;
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update();

	return ((void *) va);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap, addr)
	pmap_t pmap;
	vm_offset_t addr;
{
	
	unsigned *ptep, pte;
	int val = 0;
	
	ptep = pmap_pte_quick(pmap, addr);
	if (ptep == 0) {
		return 0;
	}

	if (pte = *ptep) {
		vm_offset_t pa;
		val = MINCORE_INCORE;
		pa = pte & PG_FRAME;

		/*
		 * Modified by us
		 */
		if (pte & PG_M)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else if (PHYS_TO_VM_PAGE(pa)->dirty ||
			pmap_tcbit(pa, PG_M)) {
			val |= MINCORE_MODIFIED_OTHER;
			PHYS_TO_VM_PAGE(pa)->dirty = VM_PAGE_BITS_ALL;
		}
		/*
		 * Referenced by us
		 */
		if (pte & PG_U)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;

		/*
		 * Referenced by someone
		 */
		else if ((PHYS_TO_VM_PAGE(pa)->flags & PG_REFERENCED) ||
			pmap_tcbit(pa, PG_A))
			val |= MINCORE_REFERENCED_OTHER;
	} 
	return val;
}

#if defined(PMAP_DEBUG)
pmap_pid_dump(int pid) {
	pmap_t pmap;
	struct proc *p;
	int npte = 0;
	int index;
	for (p = allproc.lh_first; p != NULL; p = p->p_list.le_next) {
		if (p->p_pid != pid)
			continue;

		if (p->p_vmspace) {
			int i,j;
			index = 0;
			pmap = &p->p_vmspace->vm_pmap;
			for(i=0;i<1024;i++) {
				pd_entry_t *pde;
				unsigned *pte;
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
							m = PHYS_TO_VM_PAGE((pa & PG_FRAME));
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
static void	pmap_pvdump __P((vm_offset_t pa));

/* print address space of pmap*/
static void
pads(pm)
	pmap_t pm;
{
	unsigned va, i, j;
	unsigned *ptep;

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
	register pv_entry_t pv;

	printf("pa %x", pa);
	for (pv = TAILQ_FIRST(pa_to_pvh(pa));
		pv;
		pv = TAILQ_NEXT(pv->pv_list)) {
#ifdef used_to_be
		printf(" -> pmap %x, va %x, flags %x",
		    pv->pv_pmap, pv->pv_va, pv->pv_flags);
#endif
		printf(" -> pmap %x, va %x",
		    pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}
#endif
