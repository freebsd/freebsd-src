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
 *	$Id: pmap.c,v 1.189 1998/03/09 22:09:13 eivind Exp $
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

#include "opt_disable_pse.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
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

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#if defined(SMP) || defined(APIC_IO)
#include <machine/smp.h>
#include <machine/apic.h>
#endif /* SMP || APIC_IO */

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

#define	pa_index(pa)		atop((pa) - vm_first_phys)
#define	pa_to_pvh(pa)		(&pv_table[pa_index(pa)])

static struct pmap kernel_pmap_store;
pmap_t kernel_pmap;
extern pd_entry_t my_idlePTD;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */
static vm_offset_t vm_first_phys;
static int pgeflag;		/* PG_G or-in */
static int pseflag;		/* PG_PS or-in */
static int pv_npg;

static vm_object_t kptobj;

static int nkpt;
vm_offset_t kernel_vm_end;

/*
 * Data for the pv entry allocation mechanism
 */
static vm_zone_t pvzone;
static struct vm_zone pvzone_store;
static struct vm_object pvzone_obj;
static int pv_entry_count=0, pv_entry_max=0, pv_entry_high_water=0;
static int pmap_pagedaemon_waken = 0;
static struct pv_entry *pvinit;

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1 = 0;
static pt_entry_t *CMAP2, *ptmmap;
static pv_table_t *pv_table;
caddr_t CADDR1 = 0, ptvmmap = 0;
static caddr_t CADDR2;
static pt_entry_t *msgbufmap;
struct msgbuf *msgbufp=0;

#ifdef SMP
extern char prv_CPAGE1[], prv_CPAGE2[], prv_CPAGE3[];
extern pt_entry_t *prv_CMAP1, *prv_CMAP2, *prv_CMAP3;
extern pd_entry_t *IdlePTDS[];
extern pt_entry_t SMP_prvpt[];
#endif

static pt_entry_t *PMAP1 = 0;
static unsigned *PADDR1 = 0;

static PMAP_INLINE void	free_pv_entry __P((pv_entry_t pv));
static unsigned * get_ptbase __P((pmap_t pmap));
static pv_entry_t get_pv_entry __P((void));
static void	i386_protection_init __P((void));
static void	pmap_changebit __P((vm_offset_t pa, int bit, boolean_t setem));

static PMAP_INLINE int	pmap_is_managed __P((vm_offset_t pa));
static void	pmap_remove_all __P((vm_offset_t pa));
static vm_page_t pmap_enter_quick __P((pmap_t pmap, vm_offset_t va,
				      vm_offset_t pa, vm_page_t mpte));
static int pmap_remove_pte __P((struct pmap *pmap, unsigned *ptq,
					vm_offset_t sva));
static void pmap_remove_page __P((struct pmap *pmap, vm_offset_t va));
static int pmap_remove_entry __P((struct pmap *pmap, pv_table_t *pv,
					vm_offset_t va));
static boolean_t pmap_testbit __P((vm_offset_t pa, int bit));
static void pmap_insert_entry __P((pmap_t pmap, vm_offset_t va,
		vm_page_t mpte, vm_offset_t pa));

static vm_page_t pmap_allocpte __P((pmap_t pmap, vm_offset_t va));

static int pmap_release_free_page __P((pmap_t pmap, vm_page_t p));
static vm_page_t _pmap_allocpte __P((pmap_t pmap, unsigned ptepindex));
static unsigned * pmap_pte_quick __P((pmap_t pmap, vm_offset_t va));
static vm_page_t pmap_page_lookup __P((vm_object_t object, vm_pindex_t pindex));
static int pmap_unuse_pt __P((pmap_t, vm_offset_t, vm_page_t));
static vm_offset_t pmap_kmem_choose(vm_offset_t addr) ;
void pmap_collect(void);

static unsigned pdir4mb;

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 */

PMAP_INLINE unsigned *
pmap_pte(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	unsigned *pdeaddr;

	if (pmap) {
		pdeaddr = (unsigned *) pmap_pde(pmap, va);
		if (*pdeaddr & PG_PS)
			return pdeaddr;
		if (*pdeaddr) {
			return get_ptbase(pmap) + i386_btop(va);
		}
	}
	return (0);
}

/*
 * Move the kernel virtual free pointer to the next
 * 4MB.  This is used to help improve performance
 * by using a large (4MB) page for much of the kernel
 * (.text, .data, .bss)
 */
static vm_offset_t
pmap_kmem_choose(vm_offset_t addr) {
	vm_offset_t newaddr = addr;
#ifndef DISABLE_PSE
	if (cpu_feature & CPUID_PSE) {
		newaddr = (addr + (NBPDR - 1)) & ~(NBPDR - 1);
	}
#endif
	return newaddr;
}

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
	int i, j;

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
	virtual_avail = pmap_kmem_choose(virtual_avail);

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

	kernel_pmap->pm_pdir = (pd_entry_t *) (KERNBASE + (u_int)IdlePTD);

	kernel_pmap->pm_count = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
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
	 * ptvmmap is used for reading arbitrary physical pages via /dev/mem.
	 * XXX ptmmap is not used.
	 */
	SYSMAP(caddr_t, ptmmap, ptvmmap, 1)

	/*
	 * msgbufp is used to map the system message buffer.
	 * XXX msgbufmap is not used.
	 */
	SYSMAP(struct msgbuf *, msgbufmap, msgbufp,
	       atop(round_page(sizeof(struct msgbuf))))

	/*
	 * ptemap is used for pmap_pte_quick
	 */
	SYSMAP(unsigned *, PMAP1, PADDR1, 1);

	virtual_avail = va;

	*(int *) CMAP1 = *(int *) CMAP2 = 0;
	*(int *) PTD = 0;


	pgeflag = 0;
#if !defined(SMP)
	if (cpu_feature & CPUID_PGE) {
		pgeflag = PG_G;
	}
#endif
	
/*
 * Initialize the 4MB page size flag
 */
	pseflag = 0;
/*
 * The 4MB page version of the initial
 * kernel page mapping.
 */
	pdir4mb = 0;

#if !defined(DISABLE_PSE)
	if (cpu_feature & CPUID_PSE) {
		unsigned ptditmp;
		/*
		 * Enable the PSE mode
		 */
		load_cr4(rcr4() | CR4_PSE);

		/*
		 * Note that we have enabled PSE mode
		 */
		pseflag = PG_PS;
		ptditmp = *((unsigned *)PTmap + i386_btop(KERNBASE));
		ptditmp &= ~(NBPDR - 1);
		ptditmp |= PG_V | PG_RW | PG_PS | PG_U | pgeflag;
		pdir4mb = ptditmp;
		/*
		 * We can do the mapping here for the single processor
		 * case.  We simply ignore the old page table page from
		 * now on.
		 */
#if !defined(SMP)
		PTD[KPTDI] = (pd_entry_t) ptditmp;
		kernel_pmap->pm_pdir[KPTDI] = (pd_entry_t) ptditmp;
		invltlb();
#endif
	}
#endif

#ifdef SMP
	if (cpu_apic_address == 0)
		panic("pmap_bootstrap: no local apic!");

	/* 0 = private page */
	/* 1 = page table page */
	/* 2 = local apic */
	/* 16-31 = io apics */
	SMP_prvpt[2] = (pt_entry_t)(PG_V | PG_RW | pgeflag | ((u_long)cpu_apic_address & PG_FRAME));

	for (i = 0; i < mp_napics; i++) {
		for (j = 0; j < 16; j++) {
			/* same page frame as a previous IO apic? */
			if (((u_long)SMP_prvpt[j + 16] & PG_FRAME) ==
			    ((u_long)io_apic_address[0] & PG_FRAME)) {
				ioapic[i] = (ioapic_t *)&SMP_ioapic[j * PAGE_SIZE];
				break;
			}
			/* use this slot if available */
			if (((u_long)SMP_prvpt[j + 16] & PG_FRAME) == 0) {
				SMP_prvpt[j + 16] = (pt_entry_t)(PG_V | PG_RW | pgeflag |
				    ((u_long)io_apic_address[i] & PG_FRAME));
				ioapic[i] = (ioapic_t *)&SMP_ioapic[j * PAGE_SIZE];
				break;
			}
		}
		if (j == 16)
			panic("no space to map IO apic %d!", i);
	}

	/* BSP does this itself, AP's get it pre-set */
	prv_CMAP1 = (pt_entry_t *)&SMP_prvpt[3 + UPAGES];
	prv_CMAP2 = (pt_entry_t *)&SMP_prvpt[4 + UPAGES];
	prv_CMAP3 = (pt_entry_t *)&SMP_prvpt[5 + UPAGES];
#endif

	invltlb();

}

/*
 * Set 4mb pdir for mp startup, and global flags
 */
void
pmap_set_opt(unsigned *pdir) {
	int i;

	if (pseflag && (cpu_feature & CPUID_PSE)) {
		load_cr4(rcr4() | CR4_PSE);
		if (pdir4mb) {
			(unsigned) pdir[KPTDI] = pdir4mb;
		}
	}

	if (pgeflag && (cpu_feature & CPUID_PGE)) {
		load_cr4(rcr4() | CR4_PGE);
		for(i = KPTDI; i < KPTDI + nkpt; i++) {
			if (pdir[i]) {
				pdir[i] |= PG_G;
			}
		}
	}
}

/*
 * Setup the PTD for the boot processor
 */
void
pmap_set_opt_bsp(void)
{
	pmap_set_opt((unsigned *)kernel_pmap->pm_pdir);
	pmap_set_opt((unsigned *)PTD);
	invltlb();
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
	vm_size_t s;
	int i;
	int initial_pvs;

	/*
	 * calculate the number of pv_entries needed
	 */
	vm_first_phys = phys_avail[0];
	for (i = 0; phys_avail[i + 1]; i += 2);
	pv_npg = (phys_avail[(i - 2) + 1] - vm_first_phys) / PAGE_SIZE;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */
	s = (vm_size_t) (sizeof(pv_table_t) * pv_npg);
	s = round_page(s);

	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	pv_table = (pv_table_t *) addr;
	for(i = 0; i < pv_npg; i++) {
		vm_offset_t pa;
		TAILQ_INIT(&pv_table[i].pv_list);
		pv_table[i].pv_list_count = 0;
		pa = vm_first_phys + i * PAGE_SIZE;
		pv_table[i].pv_vm_page = PHYS_TO_VM_PAGE(pa);
	}

	/*
	 * init the pv free list
	 */
	initial_pvs = pv_npg;
	if (initial_pvs < MINPV)
		initial_pvs = MINPV;
	pvzone = &pvzone_store;
	pvinit = (struct pv_entry *) kmem_alloc(kernel_map,
		initial_pvs * sizeof (struct pv_entry));
	zbootinit(pvzone, "PV ENTRY", sizeof (struct pv_entry), pvinit, pv_npg);
	/*
	 * object for kernel page table pages
	 */
	kptobj = vm_object_allocate(OBJT_DEFAULT, NKPDE);

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
pmap_init2() {
	pv_entry_max = PMAP_SHPGPERPROC * maxproc + pv_npg;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	zinitna(pvzone, &pvzone_obj, NULL, 0, pv_entry_max, ZONE_INTERRUPT, 1);
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

static PMAP_INLINE void
invltlb_1pg( vm_offset_t va) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386) {
		invltlb();
	} else
#endif
	{
		invlpg(va);
	}
}

static PMAP_INLINE void
invltlb_2pg( vm_offset_t va1, vm_offset_t va2) {
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386) {
		invltlb();
	} else
#endif
	{
		invlpg(va1);
		invlpg(va2);
	}
}

static unsigned *
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
		invltlb();
	}
	return (unsigned *) APTmap;
}

/*
 * Super fast pmap_pte routine best used when scanning
 * the pv lists.  This eliminates many coarse-grained
 * invltlb calls.  Note that many of the pv list
 * scans are across different pmaps.  It is very wasteful
 * to do an entire invltlb for checking a single mapping.
 */

static unsigned * 
pmap_pte_quick(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	unsigned pde, newpf;
	if (pde = (unsigned) pmap->pm_pdir[va >> PDRSHIFT]) {
		unsigned frame = (unsigned) pmap->pm_pdir[PTDPTDI] & PG_FRAME;
		unsigned index = i386_btop(va);
		/* are we current address space or kernel? */
		if ((pmap == kernel_pmap) ||
			(frame == (((unsigned) PTDpde) & PG_FRAME))) {
			return (unsigned *) PTmap + index;
		}
		newpf = pde & PG_FRAME;
		if ( ((* (unsigned *) PMAP1) & PG_FRAME) != newpf) {
			* (unsigned *) PMAP1 = newpf | PG_RW | PG_V;
			invltlb_1pg((vm_offset_t) PADDR1);
		}
		return PADDR1 + ((unsigned) index & (NPTEPG - 1));
	}
	return (0);
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
	vm_offset_t rtval;
	vm_offset_t pdirindex;
	pdirindex = va >> PDRSHIFT;
	if (pmap && (rtval = (unsigned) pmap->pm_pdir[pdirindex])) {
		unsigned *pte;
		if ((rtval & PG_PS) != 0) {
			rtval &= ~(NBPDR - 1);
			rtval |= va & (NBPDR - 1);
			return rtval;
		}
		pte = get_ptbase(pmap) + i386_btop(va);
		rtval = ((*pte & PG_FRAME) | (va & PAGE_MASK));
		return rtval;
	}
	return 0;

}

/*
 * determine if a page is managed (memory vs. device)
 */
static PMAP_INLINE int
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
		unsigned npte = VM_PAGE_TO_PHYS(m[i]) | PG_RW | PG_V | pgeflag;
		unsigned opte;
		pte = (unsigned *)vtopte(tva);
		opte = *pte;
		*pte = npte;
		if (opte)
			invltlb_1pg(tva);
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
		invltlb_1pg(va);
		va += PAGE_SIZE;
	}
}

/*
 * add a wired page to the kva
 * note that in order for the mapping to take effect -- you
 * should do a invltlb after doing the pmap_kenter...
 */
PMAP_INLINE void 
pmap_kenter(va, pa)
	vm_offset_t va;
	register vm_offset_t pa;
{
	register unsigned *pte;
	unsigned npte, opte;

	npte = pa | PG_RW | PG_V | pgeflag;
	pte = (unsigned *)vtopte(va);
	opte = *pte;
	*pte = npte;
	if (opte)
		invltlb_1pg(va);
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
	invltlb_1pg(va);
}

static vm_page_t
pmap_page_lookup(object, pindex)
	vm_object_t object;
	vm_pindex_t pindex;
{
	vm_page_t m;
retry:
	m = vm_page_lookup(object, pindex);
	if (m && vm_page_sleep(m, "pplookp", NULL))
		goto retry;
	return m;
}

/*
 * Create the UPAGES for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
pmap_new_proc(p)
	struct proc *p;
{
	int i, updateneeded;
	vm_object_t upobj;
	vm_page_t m;
	struct user *up;
	unsigned *ptek, oldpte;

	/*
	 * allocate object for the upages
	 */
	if ((upobj = p->p_upages_obj) == NULL) {
		upobj = vm_object_allocate( OBJT_DEFAULT, UPAGES);
		p->p_upages_obj = upobj;
	}

	/* get a kernel virtual address for the UPAGES for this proc */
	if ((up = p->p_addr) == NULL) {
		up = (struct user *) kmem_alloc_pageable(kernel_map,
				UPAGES * PAGE_SIZE);
#if !defined(MAX_PERF)
		if (up == NULL)
			panic("pmap_new_proc: u_map allocation failed");
#endif
		p->p_addr = up;
	}

	ptek = (unsigned *) vtopte((vm_offset_t) up);

	updateneeded = 0;
	for(i=0;i<UPAGES;i++) {
		/*
		 * Get a kernel stack page
		 */
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page
		 */
		m->wire_count++;
		cnt.v_wire_count++;

		oldpte = *(ptek + i);
		/*
		 * Enter the page into the kernel address space.
		 */
		*(ptek + i) = VM_PAGE_TO_PHYS(m) | PG_RW | PG_V | pgeflag;
		if (oldpte) {
			if ((oldpte & PG_G) || (cpu_class > CPUCLASS_386)) {
				invlpg((vm_offset_t) up + i * PAGE_SIZE);
			} else {
				updateneeded = 1;
			}
		}

		PAGE_WAKEUP(m);
		m->flags &= ~PG_ZERO;
		m->flags |= PG_MAPPED | PG_WRITEABLE;
		m->valid = VM_PAGE_BITS_ALL;
	}
	if (updateneeded)
		invltlb();
}

/*
 * Dispose the UPAGES for a process that has exited.
 * This routine directly impacts the exit perf of a process.
 */
void
pmap_dispose_proc(p)
	struct proc *p;
{
	int i;
	vm_object_t upobj;
	vm_page_t m;
	unsigned *ptek, oldpte;

	upobj = p->p_upages_obj;

	ptek = (unsigned *) vtopte((vm_offset_t) p->p_addr);
	for(i=0;i<UPAGES;i++) {

		if ((m = vm_page_lookup(upobj, i)) == NULL)
			panic("pmap_dispose_proc: upage already missing???");

		m->flags |= PG_BUSY;

		oldpte = *(ptek + i);
		*(ptek + i) = 0;
		if ((oldpte & PG_G) || (cpu_class > CPUCLASS_386))
			invlpg((vm_offset_t) p->p_addr + i * PAGE_SIZE);
		vm_page_unwire(m);
		vm_page_free(m);
	}

	if (cpu_class <= CPUCLASS_386)
		invltlb();
}

/*
 * Allow the UPAGES for a process to be prejudicially paged out.
 */
void
pmap_swapout_proc(p)
	struct proc *p;
{
	int i;
	vm_object_t upobj;
	vm_page_t m;

	upobj = p->p_upages_obj;
	/*
	 * let the upages be paged
	 */
	for(i=0;i<UPAGES;i++) {
		if ((m = vm_page_lookup(upobj, i)) == NULL)
			panic("pmap_swapout_proc: upage already missing???");
		m->dirty = VM_PAGE_BITS_ALL;
		vm_page_unwire(m);
		vm_page_deactivate(m);
		pmap_kremove( (vm_offset_t) p->p_addr + PAGE_SIZE * i);
	}
}

/*
 * Bring the UPAGES for a specified process back in.
 */
void
pmap_swapin_proc(p)
	struct proc *p;
{
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
#if !defined(MAX_PERF)
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_proc: cannot get upages for proc: %d\n", p->p_pid);
#endif
			m = vm_page_lookup(upobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}

		vm_page_wire(m);
		PAGE_WAKEUP(m);
		m->flags |= PG_MAPPED | PG_WRITEABLE;
	}
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

/*
 * This routine unholds page table pages, and if the hold count
 * drops to zero, then it decrements the wire count.
 */
static int 
_pmap_unwire_pte_hold(pmap_t pmap, vm_page_t m) {
	int s;

	while (vm_page_sleep(m, "pmuwpt", NULL));

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
			 * Do a invltlb to make the invalidated mapping
			 * take effect immediately.
			 */
			pteva = UPT_MIN_ADDRESS + i386_ptob(m->pindex);
			invltlb_1pg(pteva);
		}

		if (pmap->pm_ptphint == m)
			pmap->pm_ptphint = NULL;

		/*
		 * If the page is finally unwired, simply free it.
		 */
		--m->wire_count;
		if (m->wire_count == 0) {

			if (m->flags & PG_WANTED) {
				m->flags &= ~PG_WANTED;
				wakeup(m);
			}

			m->flags |= PG_BUSY;
			vm_page_free_zero(m);
			--cnt.v_wire_count;
		}
		return 1;
	}
	return 0;
}

__inline static int
pmap_unwire_pte_hold(pmap_t pmap, vm_page_t m) {
	vm_page_unhold(m);
	if (m->hold_count == 0)
		return _pmap_unwire_pte_hold(pmap, m);
	else
		return 0;
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap, va, mpte)
	pmap_t pmap;
	vm_offset_t va;
	vm_page_t mpte;
{
	unsigned ptepindex;
	if (va >= UPT_MIN_ADDRESS)
		return 0;

	if (mpte == NULL) {
		ptepindex = (va >> PDRSHIFT);
		if (pmap->pm_ptphint &&
			(pmap->pm_ptphint->pindex == ptepindex)) {
			mpte = pmap->pm_ptphint;
		} else {
			mpte = pmap_page_lookup( pmap->pm_pteobj, ptepindex);
			pmap->pm_ptphint = mpte;
		}
	}

	return pmap_unwire_pte_hold(pmap, mpte);
}

#if !defined(SMP)
void
pmap_pinit0(pmap)
	struct pmap *pmap;
{
	pmap->pm_pdir =
		(pd_entry_t *)kmem_alloc_pageable(kernel_map, PAGE_SIZE);
	pmap_kenter((vm_offset_t) pmap->pm_pdir, (vm_offset_t) IdlePTD);
	pmap->pm_flags = 0;
	pmap->pm_count = 1;
	pmap->pm_ptphint = NULL;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}
#else
void
pmap_pinit0(pmap)
	struct pmap *pmap;
{
	pmap_pinit(pmap);
}
#endif

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
	if (pmap->pm_pdir == NULL)
		pmap->pm_pdir =
			(pd_entry_t *)kmem_alloc_pageable(kernel_map, PAGE_SIZE);

	/*
	 * allocate object for the ptes
	 */
	if (pmap->pm_pteobj == NULL)
		pmap->pm_pteobj = vm_object_allocate( OBJT_DEFAULT, PTDPTDI + 1);

	/*
	 * allocate the page directory page
	 */
retry:
	ptdpg = vm_page_grab( pmap->pm_pteobj, PTDPTDI,
			VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

	ptdpg->wire_count = 1;
	++cnt.v_wire_count;

	ptdpg->flags &= ~(PG_MAPPED | PG_BUSY);	/* not mapped normally */
	ptdpg->valid = VM_PAGE_BITS_ALL;

	pmap_kenter((vm_offset_t) pmap->pm_pdir, VM_PAGE_TO_PHYS(ptdpg));
	if ((ptdpg->flags & PG_ZERO) == 0)
		bzero(pmap->pm_pdir, PAGE_SIZE);

	/* wire in kernel global address entries */
	/* XXX copies current process, does not fill in MPPTDI */
	bcopy(PTD + KPTDI, pmap->pm_pdir + KPTDI, nkpt * PTESIZE);

	/* install self-referential address mapping entry */
	*(unsigned *) (pmap->pm_pdir + PTDPTDI) =
		VM_PAGE_TO_PHYS(ptdpg) | PG_V | PG_RW | PG_A | PG_M;

	pmap->pm_flags = 0;
	pmap->pm_count = 1;
	pmap->pm_ptphint = NULL;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
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
	if (vm_page_sleep(p, "pmaprl", NULL))
		return 0;

	p->flags |= PG_BUSY;

	/*
	 * Remove the page table page from the processes address space.
	 */
	pde[p->pindex] = 0;
	pmap->pm_stats.resident_count--;

#if !defined(MAX_PERF)
	if (p->hold_count)  {
		panic("pmap_release: freeing held page table page");
	}
#endif
	/*
	 * Page directory pages need to have the kernel
	 * stuff cleared, so they can go into the zero queue also.
	 */
	if (p->pindex == PTDPTDI) {
		bzero(pde + KPTDI, nkpt * PTESIZE);
#ifdef SMP
		pde[MPPTDI] = 0;
#endif
		pde[APTDPTDI] = 0;
		pmap_kremove((vm_offset_t) pmap->pm_pdir);
	}

	if (pmap->pm_ptphint && (pmap->pm_ptphint->pindex == p->pindex))
		pmap->pm_ptphint = NULL;

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
	vm_offset_t pteva, ptepa;
	vm_page_t m;

	/*
	 * Find or fabricate a new pagetable page
	 */
	m = vm_page_grab(pmap->pm_pteobj, ptepindex,
			VM_ALLOC_ZERO | VM_ALLOC_RETRY);

	if (m->queue != PQ_NONE) {
		int s = splvm();
		vm_page_unqueue(m);
		splx(s);
	}

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
	pmap->pm_pdir[ptepindex] =
		(pd_entry_t) (ptepa | PG_U | PG_RW | PG_V | PG_A | PG_M);

	/*
	 * Set the page table hint
	 */
	pmap->pm_ptphint = m;

	/*
	 * Try to use the new mapping, but if we cannot, then
	 * do it with the routine that maps the page explicitly.
	 */
	if ((m->flags & PG_ZERO) == 0) {
		if ((((unsigned)pmap->pm_pdir[PTDPTDI]) & PG_FRAME) ==
			(((unsigned) PTDpde) & PG_FRAME)) {
			pteva = UPT_MIN_ADDRESS + i386_ptob(ptepindex);
			bzero((caddr_t) pteva, PAGE_SIZE);
		} else {
			pmap_zero_page(ptepa);
		}
	}

	m->valid = VM_PAGE_BITS_ALL;
	m->flags &= ~(PG_ZERO | PG_BUSY);
	m->flags |= PG_MAPPED;

	return m;
}

static vm_page_t
pmap_allocpte(pmap, va)
	pmap_t	pmap;
	vm_offset_t va;
{
	unsigned ptepindex;
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
	 * This supports switching from a 4MB page to a
	 * normal 4K page.
	 */
	if (ptepa & PG_PS) {
		pmap->pm_pdir[ptepindex] = 0;
		ptepa = 0;
		invltlb();
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (ptepa) {
		/*
		 * In order to get the page table page, try the
		 * hint first.
		 */
		if (pmap->pm_ptphint &&
			(pmap->pm_ptphint->pindex == ptepindex)) {
			m = pmap->pm_ptphint;
		} else {
			m = pmap_page_lookup( pmap->pm_pteobj, ptepindex);
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
pmap_release(pmap)
	register struct pmap *pmap;
{
	vm_page_t p,n,ptdpg;
	vm_object_t object = pmap->pm_pteobj;
	int curgeneration;

#if defined(DIAGNOSTIC)
	if (object->ref_count != 1)
		panic("pmap_release: pteobj reference count != 1");
#endif
	
	ptdpg = NULL;
retry:
	curgeneration = object->generation;
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = n) {
		n = TAILQ_NEXT(p, listq);
		if (p->pindex == PTDPTDI) {
			ptdpg = p;
			continue;
		}
		while (1) {
			if (!pmap_release_free_page(pmap, p) &&
				(object->generation != curgeneration))
				goto retry;
		}
	}

	if (ptdpg && !pmap_release_free_page(pmap, ptdpg))
		goto retry;
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
	vm_offset_t ptppaddr;
	vm_page_t nkpg;
#ifdef SMP
	int i;
#endif
	pd_entry_t newpdir;

	s = splhigh();
	if (kernel_vm_end == 0) {
		kernel_vm_end = KERNBASE;
		nkpt = 0;
		while (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			nkpt++;
		}
	}
	addr = (addr + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	while (kernel_vm_end < addr) {
		if (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			continue;
		}

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = vm_page_alloc(kptobj, nkpt, VM_ALLOC_SYSTEM);
#if !defined(MAX_PERF)
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");
#endif

		nkpt++;

		vm_page_wire(nkpg);
		ptppaddr = VM_PAGE_TO_PHYS(nkpg);
		pmap_zero_page(ptppaddr);
		newpdir = (pd_entry_t) (ptppaddr | PG_V | PG_RW | PG_A | PG_M);
		pdir_pde(PTD, kernel_vm_end) = newpdir;

#ifdef SMP
		for (i = 0; i < mp_ncpus; i++) {
			if (IdlePTDS[i])
				pdir_pde(IdlePTDS[i], kernel_vm_end) = newpdir;
		}
#endif

		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			if (p->p_vmspace) {
				pmap = &p->p_vmspace->vm_pmap;
				*pmap_pde(pmap, kernel_vm_end) = newpdir;
			}
		}
		*pmap_pde(kernel_pmap, kernel_vm_end) = newpdir;
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
#if !defined(MAX_PERF)
		panic("destroying a pmap is not yet implemented");
#endif
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
static inline void
free_pv_entry(pv)
	pv_entry_t pv;
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
	pv_entry_count++;
	if (pv_entry_high_water &&
		(pv_entry_count > pv_entry_high_water) &&
		(pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup (&vm_pages_needed);
	}
	return zalloci(pvzone);
}

/*
 * This routine is very drastic, but can save the system
 * in a pinch.
 */
void
pmap_collect() {
	pv_table_t *ppv;
	int i;
	vm_offset_t pa;
	vm_page_t m;
	static int warningdone=0;

	if (pmap_pagedaemon_waken == 0)
		return;

	if (warningdone < 5) {
		printf("pmap_collect: collecting pv entries -- suggest increasing PMAP_SHPGPERPROC\n");
		warningdone++;
	}

	for(i = 0; i < pv_npg; i++) {
		if ((ppv = &pv_table[i]) == 0)
			continue;
		m = ppv->pv_vm_page;
		if ((pa = VM_PAGE_TO_PHYS(m)) == 0)
			continue;
		if (m->wire_count || m->hold_count || m->busy ||
			(m->flags & PG_BUSY))
			continue;
		pmap_remove_all(pa);
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
		for (pv = TAILQ_FIRST(&pmap->pm_pvlist);
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
		ppv->pv_list_count--;
		if (TAILQ_FIRST(&ppv->pv_list) == NULL)
			ppv->pv_vm_page->flags &= ~(PG_MAPPED | PG_WRITEABLE);

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

	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);

	ppv = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
	ppv->pv_list_count++;

	splx(s);
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
	pv_table_t *ppv;

	oldpte = *ptq;
	*ptq = 0;
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	/*
	 * Machines that don't support invlpg, also don't support
	 * PG_G.
	 */
	if (oldpte & PG_G)
		invlpg(va);
	pmap->pm_stats.resident_count -= 1;
	if (oldpte & PG_MANAGED) {
		ppv = pa_to_pvh(oldpte);
		if (oldpte & PG_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified((pt_entry_t) oldpte)) {
				printf("pmap_remove: modified page not writable: va: 0x%lx, pte: 0x%lx\n", va, (int) oldpte);
			}
#endif
			if (pmap_track_modified(va))
				ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
		}
		if (oldpte & PG_A)
			ppv->pv_vm_page->flags |= PG_REFERENCED;
		return pmap_remove_entry(pmap, ppv, va);
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
	if (*ptq) {
		(void) pmap_remove_pte(pmap, ptq, va);
		invltlb_1pg(va);
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
	if (((sva + PAGE_SIZE) == eva) && 
		(((unsigned) pmap->pm_pdir[(sva >> PDRSHIFT)] & PG_PS) == 0)) {
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

	for (; sindex < eindex; sindex = pdnxt) {
		unsigned pdirindex;

		/*
		 * Calculate index for next page table.
		 */
		pdnxt = ((sindex + NPTEPG) & ~(NPTEPG - 1));
		if (pmap->pm_stats.resident_count == 0)
			break;

		pdirindex = sindex / NPDEPG;
		if (((ptpaddr = (unsigned) pmap->pm_pdir[pdirindex]) & PG_PS) != 0) {
			pmap->pm_pdir[pdirindex] = 0;
			pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
			anyvalid++;
			continue;
		}

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
			if (ptbase[sindex] == 0) {
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
		invltlb();
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
pmap_remove_all(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;
	pv_table_t *ppv;
	register unsigned *pte, tpte;
	int nmodify;
	int update_needed;
	int s;

	nmodify = 0;
	update_needed = 0;
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
	ppv = pa_to_pvh(pa);
	while ((pv = TAILQ_FIRST(&ppv->pv_list)) != NULL) {
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);

		pv->pv_pmap->pm_stats.resident_count--;

		tpte = *pte;
		*pte = 0;
		if (tpte & PG_W)
			pv->pv_pmap->pm_stats.wired_count--;

		if (tpte & PG_A)
			ppv->pv_vm_page->flags |= PG_REFERENCED;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PG_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified((pt_entry_t) tpte)) {
				printf("pmap_remove_all: modified page not writable: va: 0x%lx, pte: 0x%lx\n", pv->pv_va, tpte);
			}
#endif
			if (pmap_track_modified(pv->pv_va))
				ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
		}
		if (!update_needed &&
			((!curproc || (&curproc->p_vmspace->vm_pmap == pv->pv_pmap)) ||
			(pv->pv_pmap == kernel_pmap))) {
			update_needed = 1;
		}

		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		ppv->pv_list_count--;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}

	ppv->pv_vm_page->flags &= ~(PG_MAPPED | PG_WRITEABLE);

	if (update_needed)
		invltlb();

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
	register unsigned *ptbase;
	vm_offset_t pdnxt, ptpaddr;
	vm_pindex_t sindex, eindex;
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

	ptbase = get_ptbase(pmap);

	sindex = i386_btop(sva);
	eindex = i386_btop(eva);

	for (; sindex < eindex; sindex = pdnxt) {

		unsigned pdirindex;

		pdnxt = ((sindex + NPTEPG) & ~(NPTEPG - 1));

		pdirindex = sindex / NPDEPG;
		if (((ptpaddr = (unsigned) pmap->pm_pdir[pdirindex]) & PG_PS) != 0) {
			(unsigned) pmap->pm_pdir[pdirindex] &= ~(PG_M|PG_RW);
			pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
			anychanged++;
			continue;
		}

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

			unsigned pbits;
			pv_table_t *ppv;

			pbits = ptbase[sindex];

			if (pbits & PG_MANAGED) {
				ppv = NULL;
				if (pbits & PG_A) {
					ppv = pa_to_pvh(pbits);
					ppv->pv_vm_page->flags |= PG_REFERENCED;
					pbits &= ~PG_A;
				}
				if (pbits & PG_M) {
					if (pmap_track_modified(i386_ptob(sindex))) {
						if (ppv == NULL)
							ppv = pa_to_pvh(pbits);
						ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
						pbits &= ~PG_M;
					}
				}
			}

			pbits &= ~PG_RW;

			if (pbits != ptbase[sindex]) {
				ptbase[sindex] = pbits;
				anychanged = 1;
			}
		}
	}
	if (anychanged)
		invltlb();
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
pmap_enter(pmap_t pmap, vm_offset_t va, vm_offset_t pa, vm_prot_t prot,
	   boolean_t wired)
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
	if (va < UPT_MIN_ADDRESS) {
		mpte = pmap_allocpte(pmap, va);
	}
#if 0 && defined(PMAP_DIAGNOSTIC)
	else {
		vm_offset_t *pdeaddr = (vm_offset_t *)pmap_pde(pmap, va);
		if (((origpte = (vm_offset_t) *pdeaddr) & PG_V) == 0) { 
			panic("pmap_enter: invalid kernel page table page(0), pdir=%p, pde=%p, va=%p\n",
				pmap->pm_pdir[PTDPTDI], origpte, va);
		}
		if (smp_active) {
			pdeaddr = (vm_offset_t *) IdlePTDS[cpuid];
			if (((newpte = pdeaddr[va >> PDRSHIFT]) & PG_V) == 0) {
				if ((vm_offset_t) my_idlePTD != (vm_offset_t) vtophys(pdeaddr))
					printf("pde mismatch: %x, %x\n", my_idlePTD, pdeaddr);
				printf("cpuid: %d, pdeaddr: 0x%x\n", cpuid, pdeaddr);
				panic("pmap_enter: invalid kernel page table page(1), pdir=%p, npde=%p, pde=%p, va=%p\n",
					pmap->pm_pdir[PTDPTDI], newpte, origpte, va);
			}
		}
	}
#endif

	pte = pmap_pte(pmap, va);

#if !defined(MAX_PERF)
	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL) {
		panic("pmap_enter: invalid page directory, pdir=%p, va=0x%lx\n",
			pmap->pm_pdir[PTDPTDI], va);
	}
#endif

	origpte = *(vm_offset_t *)pte;
	pa &= PG_FRAME;
	opa = origpte & PG_FRAME;

#if !defined(MAX_PERF)
	if (origpte & PG_PS)
		panic("pmap_enter: attempted pmap_enter on 4MB page");
#endif

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
			printf("pmap_enter: modified page not writable: va: 0x%lx, pte: 0x%lx\n", va, origpte);
		}
#endif

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->hold_count--;

		if ((prot & VM_PROT_WRITE) && (origpte & PG_V)) {
			if ((origpte & PG_RW) == 0) {
				*pte |= PG_RW;
				invltlb_1pg(va);
			}
			return;
		}

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (origpte & PG_MANAGED) {
			if ((origpte & PG_M) && pmap_track_modified(va)) {
				pv_table_t *ppv;
				ppv = pa_to_pvh(opa);
				ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
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
		err = pmap_remove_pte(pmap, pte, va);
#if !defined(MAX_PERF)
		if (err)
			panic("pmap_enter: pte vanished, va: 0x%x", va);
#endif
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
	if (pmap == kernel_pmap)
		newpte |= pgeflag;

	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if ((origpte & ~(PG_M|PG_A)) != newpte) {
		*pte = newpte | PG_A;
		if (origpte)
			invltlb_1pg(va);
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
		unsigned ptepindex;
		vm_offset_t ptepa;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = va >> PDRSHIFT;
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->hold_count++;
		} else {
retry:
			/*
			 * Get the page directory entry
			 */
			ptepa = (vm_offset_t) pmap->pm_pdir[ptepindex];

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (ptepa) {
#if !defined(MAX_PERF)
				if (ptepa & PG_PS)
					panic("pmap_enter_quick: unexpected mapping into 4MB page");
#endif
				if (pmap->pm_ptphint &&
					(pmap->pm_ptphint->pindex == ptepindex)) {
					mpte = pmap->pm_ptphint;
				} else {
					mpte = pmap_page_lookup( pmap->pm_pteobj, ptepindex);
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
	pte = (unsigned *)vtopte(va);
	if (*pte) {
		if (mpte)
			pmap_unwire_pte_hold(pmap, mpte);
		return 0;
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

	if (!pmap)
		return;

	/*
	 * This code maps large physical mmap regions into the
	 * processor address space.  Note that some shortcuts
	 * are taken, but the code works.
	 */
	if (pseflag &&
		(object->type == OBJT_DEVICE) &&
		((addr & (NBPDR - 1)) == 0) &&
		((size & (NBPDR - 1)) == 0) ) {
		int i;
		int s;
		vm_page_t m[1];
		unsigned int ptepindex;
		int npdes;
		vm_offset_t ptepa;

		if (pmap->pm_pdir[ptepindex = (addr >> PDRSHIFT)])
			return;

retry:
		p = vm_page_lookup(object, pindex);
		if (p && vm_page_sleep(p, "init4p", NULL))
			goto retry;

		if (p == NULL) {
			p = vm_page_alloc(object, pindex, VM_ALLOC_NORMAL);
			if (p == NULL)
				return;
			m[0] = p;

			if (vm_pager_get_pages(object, m, 1, 0) != VM_PAGER_OK) {
				vm_page_free(p);
				return;
			}

			p = vm_page_lookup(object, pindex);
			PAGE_WAKEUP(p);
		}

		ptepa = (vm_offset_t) VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1)) {
			return;
		}

		p->valid = VM_PAGE_BITS_ALL;

		pmap->pm_stats.resident_count += size >> PAGE_SHIFT;
		npdes = size >> PDRSHIFT;
		for(i=0;i<npdes;i++) {
			pmap->pm_pdir[ptepindex] =
				(pd_entry_t) (ptepa | PG_U | PG_RW | PG_V | PG_PS);
			ptepa += NBPDR;
			ptepindex += 1;
		}
		p->flags |= PG_MAPPED;
		invltlb();
		return;
	}

	psize = i386_btop(size);

	if ((object->type != OBJT_VNODE) ||
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
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
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
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
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

	if (!curproc || (pmap != &curproc->p_vmspace->vm_pmap))
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
		unsigned *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

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
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if ((m->queue - m->pc) == PQ_CACHE) {
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
	if (src_frame != (((unsigned) PTDpde) & PG_FRAME)) {
		return;
	}

	dst_frame = ((unsigned) dst_pmap->pm_pdir[PTDPTDI]) & PG_FRAME;
	if (dst_frame != (((unsigned) APTDpde) & PG_FRAME)) {
		APTDpde = (pd_entry_t) (dst_frame | PG_RW | PG_V);
		invltlb();
	}

	for(addr = src_addr; addr < end_addr; addr = pdnxt) {
		unsigned *src_pte, *dst_pte;
		vm_page_t dstmpte, srcmpte;
		vm_offset_t srcptepaddr;
		unsigned ptepindex;

#if !defined(MAX_PERF)
		if (addr >= UPT_MIN_ADDRESS)
			panic("pmap_copy: invalid to pmap_copy page tables\n");
#endif

		pdnxt = ((addr + PAGE_SIZE*NPTEPG) & ~(PAGE_SIZE*NPTEPG - 1));
		ptepindex = addr >> PDRSHIFT;

		srcptepaddr = (vm_offset_t) src_pmap->pm_pdir[ptepindex];
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			if (dst_pmap->pm_pdir[ptepindex] == 0) {
				dst_pmap->pm_pdir[ptepindex] = (pd_entry_t) srcptepaddr;
				dst_pmap->pm_stats.resident_count += NBPDR;
			}
			continue;
		}

		srcmpte = vm_page_lookup(src_pmap->pm_pteobj, ptepindex);
		if ((srcmpte == NULL) ||
			(srcmpte->hold_count == 0) || (srcmpte->flags & PG_BUSY))
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
					*dst_pte = ptetemp & ~(PG_M | PG_A);
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
			src_pte++;
			dst_pte++;
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
#ifdef SMP
#if !defined(MAX_PERF)
	if (*(int *) prv_CMAP3)
		panic("pmap_zero_page: prv_CMAP3 busy");
#endif

	*(int *) prv_CMAP3 = PG_V | PG_RW | (phys & PG_FRAME) | PG_A | PG_M;
	invltlb_1pg((vm_offset_t) &prv_CPAGE3);

	bzero(&prv_CPAGE3, PAGE_SIZE);

	*(int *) prv_CMAP3 = 0;
	invltlb_1pg((vm_offset_t) &prv_CPAGE3);
#else
#if !defined(MAX_PERF)
	if (*(int *) CMAP2)
		panic("pmap_zero_page: CMAP busy");
#endif

	*(int *) CMAP2 = PG_V | PG_RW | (phys & PG_FRAME) | PG_A | PG_M;
	bzero(CADDR2, PAGE_SIZE);
	*(int *) CMAP2 = 0;
	invltlb_1pg((vm_offset_t) CADDR2);
#endif
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
#ifdef SMP
#if !defined(MAX_PERF)
	if (*(int *) prv_CMAP1)
		panic("pmap_copy_page: prv_CMAP1 busy");
	if (*(int *) prv_CMAP2)
		panic("pmap_copy_page: prv_CMAP2 busy");
#endif

	*(int *) prv_CMAP1 = PG_V | (src & PG_FRAME) | PG_A;
	*(int *) prv_CMAP2 = PG_V | PG_RW | (dst & PG_FRAME) | PG_A | PG_M;

	invltlb_2pg( (vm_offset_t) &prv_CPAGE1, (vm_offset_t) &prv_CPAGE2);

	bcopy(&prv_CPAGE1, &prv_CPAGE2, PAGE_SIZE);

	*(int *) prv_CMAP1 = 0;
	*(int *) prv_CMAP2 = 0;
	invltlb_2pg( (vm_offset_t) &prv_CPAGE1, (vm_offset_t) &prv_CPAGE2);
#else
#if !defined(MAX_PERF)
	if (*(int *) CMAP1 || *(int *) CMAP2)
		panic("pmap_copy_page: CMAP busy");
#endif

	*(int *) CMAP1 = PG_V | (src & PG_FRAME) | PG_A;
	*(int *) CMAP2 = PG_V | PG_RW | (dst & PG_FRAME) | PG_A | PG_M;

	bcopy(CADDR1, CADDR2, PAGE_SIZE);

	*(int *) CMAP1 = 0;
	*(int *) CMAP2 = 0;
	invltlb_2pg( (vm_offset_t) CADDR1, (vm_offset_t) CADDR2);
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
	unsigned *pte, tpte;
	pv_table_t *ppv;
	pv_entry_t pv, npv;
	int s;

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (!curproc || (pmap != &curproc->p_vmspace->vm_pmap)) {
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
		pte = (unsigned *)vtopte(pv->pv_va);
#else
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
#endif
		tpte = *pte;

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (tpte & PG_W) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}
		*pte = 0;

		ppv = pa_to_pvh(tpte);

		pv->pv_pmap->pm_stats.resident_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PG_M) {
			ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
		}


		npv = TAILQ_NEXT(pv, pv_plist);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);

		ppv->pv_list_count--;
		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		if (TAILQ_FIRST(&ppv->pv_list) == NULL) {
			ppv->pv_vm_page->flags &= ~(PG_MAPPED | PG_WRITEABLE);
		}

		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		free_pv_entry(pv);
	}
	splx(s);
	invltlb();
}

/*
 * pmap_testbit tests bits in pte's
 * note that the testbit/changebit routines are inline,
 * and a lot of things compile-time evaluate.
 */
static boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register pv_entry_t pv;
	pv_table_t *ppv;
	unsigned *pte;
	int s;

	if (!pmap_is_managed(pa))
		return FALSE;

	ppv = pa_to_pvh(pa);
	if (TAILQ_FIRST(&ppv->pv_list) == NULL)
		return FALSE;

	s = splvm();

	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {

		/*
		 * if the bit being tested is the modified bit, then
		 * mark clean_map and ptes as never
		 * modified.
		 */
		if (bit & (PG_A|PG_M)) {
			if (!pmap_track_modified(pv->pv_va))
				continue;
		}

#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (tb) at va: 0x%lx\n", pv->pv_va);
			continue;
		}
#endif
		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
		if (*pte & bit) {
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
static void
pmap_changebit(pa, bit, setem)
	vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	pv_table_t *ppv;
	register unsigned *pte;
	int changed;
	int s;

	if (!pmap_is_managed(pa))
		return;

	s = splvm();
	changed = 0;
	ppv = pa_to_pvh(pa);

	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {

		/*
		 * don't write protect pager mappings
		 */
		if (!setem && (bit == PG_RW)) {
			if (!pmap_track_modified(pv->pv_va))
				continue;
		}

#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (cb) at va: 0x%lx\n", pv->pv_va);
			continue;
		}
#endif

		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);

		if (setem) {
			*(int *)pte |= bit;
			changed = 1;
		} else {
			vm_offset_t pbits = *(vm_offset_t *)pte;
			if (pbits & bit) {
				changed = 1;
				if (bit == PG_RW) {
					if (pbits & PG_M) {
						ppv->pv_vm_page->dirty = VM_PAGE_BITS_ALL;
					}
					*(int *)pte = pbits & ~(PG_M|PG_RW);
				} else {
					*(int *)pte = pbits & ~bit;
				}
			}
		}
	}
	splx(s);
	if (changed)
		invltlb();
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_offset_t phys, vm_prot_t prot)
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
			pmap_changebit(phys, PG_RW, FALSE);
		} else {
			pmap_remove_all(phys);
		}
	}
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return (i386_ptob(ppn));
}

/*
 *	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 *	
 */
int
pmap_ts_referenced(vm_offset_t pa)
{
	register pv_entry_t pv;
	pv_table_t *ppv;
	unsigned *pte;
	int s;
	int rtval = 0;

	if (!pmap_is_managed(pa))
		return FALSE;

	s = splvm();

	ppv = pa_to_pvh(pa);

	if (TAILQ_FIRST(&ppv->pv_list) == NULL) {
		splx(s);
		return 0;
	}
		
	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {

		TAILQ_REMOVE(&ppv->pv_list, pv, pv_list);
		/*
		 * if the bit being tested is the modified bit, then
		 * mark clean_map and ptes as never
		 * modified.
		 */
		if (!pmap_track_modified(pv->pv_va)) {
			TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
			continue;
		}

		pte = pmap_pte_quick(pv->pv_pmap, pv->pv_va);
		if (pte == NULL) {
			TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
			continue;
		}

		if (*pte & PG_A) {
			rtval++;
			*pte &= ~PG_A;
			if (rtval > 4) {
				TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
				break;
			}
		}
		TAILQ_INSERT_TAIL(&ppv->pv_list, pv, pv_list);
	}

	splx(s);
	if (rtval) {
		invltlb();
	}
	return (rtval);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_offset_t pa)
{
	return pmap_testbit((pa), PG_M);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_offset_t pa)
{
	pmap_changebit((pa), PG_M, FALSE);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_offset_t pa)
{
	pmap_changebit((pa), PG_A, FALSE);
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
 * NOT real memory.
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
#if !defined(MAX_PERF)
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");
#endif

	pa = pa & PG_FRAME;
	for (tmpva = va; size > 0;) {
		pte = (unsigned *)vtopte(tmpva);
		*pte = pa | PG_RW | PG_V | pgeflag;
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	invltlb();

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
	vm_page_t m;
	int val = 0;
	
	ptep = pmap_pte(pmap, addr);
	if (ptep == 0) {
		return 0;
	}

	if (pte = *ptep) {
		pv_table_t *ppv;
		vm_offset_t pa;

		val = MINCORE_INCORE;
		if ((pte & PG_MANAGED) == 0)
			return val;

		pa = pte & PG_FRAME;

		ppv = pa_to_pvh((pa & PG_FRAME));
		m = ppv->pv_vm_page;

		/*
		 * Modified by us
		 */
		if (pte & PG_M)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else if (m->dirty || pmap_is_modified(pa))
			val |= MINCORE_MODIFIED_OTHER;
		/*
		 * Referenced by us
		 */
		if (pte & PG_A)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;

		/*
		 * Referenced by someone
		 */
		else if ((m->flags & PG_REFERENCED) || pmap_ts_referenced(pa)) {
			val |= MINCORE_REFERENCED_OTHER;
			m->flags |= PG_REFERENCED;
		}
	} 
	return val;
}

void
pmap_activate(struct proc *p)
{
#if defined(SWTCH_OPTIM_STATS)
	tlb_flush_count++;
#endif
	load_cr3(p->p_addr->u_pcb.pcb_cr3 =
		vtophys(p->p_vmspace->vm_pmap.pm_pdir));
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size) {

	if ((obj == NULL) || (size < NBPDR) || (obj->type != OBJT_DEVICE)) {
		return addr;
	}

	addr = (addr + (NBPDR - 1)) & ~(NBPDR - 1);
	return addr;
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
	pv_table_t *ppv;
	register pv_entry_t pv;

	printf("pa %x", pa);
	ppv = pa_to_pvh(pa);
	for (pv = TAILQ_FIRST(&ppv->pv_list);
		pv;
		pv = TAILQ_NEXT(pv, pv_list)) {
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
