/*-
 *
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2005 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 * Copyright (c) 2012 Spectra Logic Corporation
 * All rights reserved.
 * Copyright (c) 2012 Citrix Systems
 * All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Portions of this software were developed by
 * Cherry G. Mathew <cherry.g.mathew@gmail.com> under sponsorship
 * from Spectra Logic Corporation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"
#include "opt_pmap.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>

#ifdef SMP
#include <sys/smp.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/uma.h>

#include <machine/md_var.h>

#include <xen/hypervisor.h>
#include <machine/xen/xenvar.h>

#include <amd64/xen/mmu_map.h>

extern vm_offset_t pa_index; /* from machdep.c */
extern unsigned long physfree; /* from machdep.c */

struct pmap kernel_pmap_store;

uintptr_t virtual_avail;	/* VA of first avail page (after kernel bss) */
uintptr_t virtual_end;	/* VA of last avail page (end of kernel AS) */

#ifdef SUPERPAGESUPPORT
static int ndmpdp;
static vm_paddr_t dmaplimit;
#endif /* SUPERPAGESUPPORT */

uintptr_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;
pt_entry_t pg_nx; /* XXX: do we need this ? */

struct msgbuf *msgbufp = 0;

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
u_int64_t		KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */

#ifdef SUPERPAGESUPPORT
static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */
#endif /* SUPERPAGESUPPORT */

static vm_paddr_t	boot_ptphys;	/* phys addr of start of
					 * kernel bootstrap tables
					 */
static vm_paddr_t	boot_ptendphys;	/* phys addr of end of kernel
					 * bootstrap page tables
					 */

static uma_zone_t xen_pagezone;
static size_t tsz; /* mmu_map.h opaque cookie size */
static uintptr_t (*ptmb_mappedalloc)(void) = NULL;
static void (*ptmb_mappedfree)(uintptr_t) = NULL;
static uintptr_t ptmb_ptov(vm_paddr_t p)
{
	return PTOV(p);
}
static vm_paddr_t ptmb_vtop(uintptr_t v)
{
	return VTOP(v);
}

extern uint64_t xenstack; /* The stack Xen gives us at boot */
extern char *console_page; /* The shared ring for console i/o */

/* return kernel virtual address of  'n' claimed physical pages at boot. */
static uintptr_t
vallocpages(vm_paddr_t *firstaddr, int n)
{
	uintptr_t ret = *firstaddr + KERNBASE;
	bzero((void *)ret, n * PAGE_SIZE);
	*firstaddr += n * PAGE_SIZE;

	/* Make sure we are still inside of available mapped va. */
	if (PTOV(*firstaddr) > (xenstack + 512 * 1024)) {
		printk("Attempt to use unmapped va\n");
	}
	KASSERT(PTOV(*firstaddr) <= (xenstack + 512 * 1024), 
		("Attempt to use unmapped va\n"));
	return (ret);
}

/* 
 * At boot, xen guarantees us a 512kB padding area that is passed to
 * us. We must be careful not to spill the tables we create here
 * beyond this.
 */

/* Set page addressed by va to r/o */
static void
pmap_xen_setpages_ro(uintptr_t va, vm_size_t npages)
{
	vm_size_t i;
	for (i = 0; i < npages; i++) {
		PT_SET_MA(va + PAGE_SIZE * i, 
			  phystomach(VTOP(va + PAGE_SIZE * i)) | PG_U | PG_V);
	}
}

/* Set page addressed by va to r/w */
static void
pmap_xen_setpages_rw(uintptr_t va, vm_size_t npages)
{
	vm_size_t i;
	for (i = 0; i < npages; i++) {
		PT_SET_MA(va + PAGE_SIZE * i, 
			  phystomach(VTOP(va + PAGE_SIZE * i)) | PG_U | PG_V | PG_RW);
	}
}

extern int etext;	/* End of kernel text (virtual address) */
extern int end;		/* End of kernel binary (virtual address) */
/* Return pte flags according to kernel va access restrictions */
static pt_entry_t
pmap_xen_kernel_vaflags(uintptr_t va)
{
	if ((va > (uintptr_t) &etext && /* .data, .bss et. al */
	     (va < (uintptr_t) &end))
	    ||
	    ((va > (uintptr_t)(xen_start_info->pt_base +
	    			xen_start_info->nr_pt_frames * PAGE_SIZE)) &&
	     va < PTOV(boot_ptphys))
	    ||
	    va > PTOV(boot_ptendphys)) {
		return PG_RW;
	}

	return 0;
}

static void
create_boot_pagetables(vm_paddr_t *firstaddr)
{
	int i;
	int nkpt, nkpdpe;
	int nkmapped = atop(VTOP(xenstack + 512 * 1024 + PAGE_SIZE));

	kernel_vm_end = PTOV(ptoa(nkmapped - 1));

	boot_ptphys = *firstaddr; /* lowest available r/w area */

	/* Allocate pseudo-physical pages for kernel page tables. */
	nkpt = howmany(nkmapped, NPTEPG);
	nkpdpe = howmany(nkpt, NPDEPG);
	KPML4phys = vallocpages(firstaddr, 1);
	KPDPphys = vallocpages(firstaddr, NKPML4E);
	KPDphys = vallocpages(firstaddr, nkpdpe);
	KPTphys = vallocpages(firstaddr, nkpt);

#ifdef SUPERPAGESUPPORT
	int ndm1g;
	ndmpdp = (ptoa(Maxmem) + NBPDP - 1) >> PDPSHIFT;
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	DMPDPphys = vallocpages(firstaddr, NDMPML4E, VALLOC_MAPPED);
	ndm1g = 0;
	if ((amd_feature & AMDID_PAGE1GB) != 0)
		ndm1g = ptoa(Maxmem) >> PDPSHIFT;

	if (ndm1g < ndmpdp)
		DMPDphys = vallocpages(firstaddr, ndmpdp - ndm1g, 
				       VALLOC_MAPPED);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;
#endif /* SUPERPAGESUPPORT */


	boot_ptendphys = *firstaddr - 1;

	/* We can't spill over beyond the 512kB padding */
	KASSERT(((boot_ptendphys - boot_ptphys) / 1024) <= 512,
		("bootstrap mapped memory insufficient.\n"));

	/* Fill in the underlying page table pages */
	for (i = 0; ptoa(i) < ptoa(nkmapped); i++) {
		((pt_entry_t *)KPTphys)[i] = phystomach(i << PAGE_SHIFT);
		((pt_entry_t *)KPTphys)[i] |= PG_V | PG_U;
		((pt_entry_t *)KPTphys)[i] |= 
			pmap_xen_kernel_vaflags(PTOV(i << PAGE_SHIFT));	
	}
	
	pmap_xen_setpages_ro(KPTphys, (i - 1)/ NPTEPG + 1);

	/* Now map the page tables at their location within PTmap */
	for (i = 0; i < nkpt; i++) {
		((pd_entry_t *)KPDphys)[i] = phystomach(VTOP(KPTphys) +
							(i << PAGE_SHIFT));
		((pd_entry_t *)KPDphys)[i] |= PG_RW | PG_V | PG_U;

	}

	pmap_xen_setpages_ro(KPDphys, (nkpt - 1) / NPDEPG + 1);

#ifdef SUPERPAGESUPPORT /* XXX: work out r/o overlaps and 2M machine pages*/
	/* Map from zero to end of allocations under 2M pages */
	/* This replaces some of the KPTphys entries above */
	for (i = 0; (i << PDRSHIFT) < *firstaddr; i++) {
		((pd_entry_t *)KPDphys)[i] = phystomach(i << PDRSHIFT);
		((pd_entry_t *)KPDphys)[i] |= PG_U | PG_RW | PG_V | PG_PS | PG_G;
	}
#endif

	/* And connect up the PD to the PDP */
	for (i = 0; i < nkpdpe; i++) {
		((pdp_entry_t *)KPDPphys)[i + KPDPI] = phystomach(VTOP(KPDphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPDPphys)[i + KPDPI] |= PG_RW | PG_V | PG_U;
	}

	pmap_xen_setpages_ro(KPDPphys, (nkpdpe - 1) / NPDPEPG + 1);

#ifdef SUPERPAGESUPPORT
	int j;

	/*
	 * Now, set up the direct map region using 2MB and/or 1GB pages.  If
	 * the end of physical memory is not aligned to a 1GB page boundary,
	 * then the residual physical memory is mapped with 2MB pages.  Later,
	 * if pmap_mapdev{_attr}() uses the direct map for non-write-back
	 * memory, pmap_change_attr() will demote any 2MB or 1GB page mappings
	 * that are partially used. 
	 */

	for (i = NPDEPG * ndm1g, j = 0; i < NPDEPG * ndmpdp; i++, j++) {
		if ((i << PDRSHIFT) > ptoa(Maxmem)) {
			/* 
			 * Since the page is zeroed out at
			 * vallocpages(), the remaining ptes will be
			 * invalid.
			 */
			 
			break;
		}
		((pd_entry_t *)DMPDphys)[j] = (vm_paddr_t)(phystomach(i << PDRSHIFT));
		/* Preset PG_M and PG_A because demotion expects it. */
		((pd_entry_t *)DMPDphys)[j] |= PG_U | PG_V | PG_PS /* | PG_G */ |
		    PG_M | PG_A;
	}
	/* Mark pages R/O */
	pmap_xen_setpages_ro(DMPDphys, ndmpdp - ndm1g);

	/* Setup 1G pages, if available */
	for (i = 0; i < ndm1g; i++) {
		if ((i << PDPSHIFT) > ptoa(Maxmem)) {
			/* 
			 * Since the page is zeroed out at
			 * vallocpages(), the remaining ptes will be
			 * invalid.
			 */
			 
			break;
		}

		((pdp_entry_t *)DMPDPphys)[i] = (vm_paddr_t)phystomach(i << PDPSHIFT);
		/* Preset PG_M and PG_A because demotion expects it. */
		((pdp_entry_t *)DMPDPphys)[i] |= PG_U | PG_V | PG_PS | PG_G |
		    PG_M | PG_A;
	}

	for (j = 0; i < ndmpdp; i++, j++) {
		((pdp_entry_t *)DMPDPphys)[i] = phystomach(VTOP(DMPDphys) + (j << PAGE_SHIFT));
		((pdp_entry_t *)DMPDPphys)[i] |= PG_V | PG_U;
	}

	pmap_xen_setpages_ro(DMPDPphys, NDMPML4E);

	/* Connect the Direct Map slot(s) up to the PML4. */
	for (i = 0; i < NDMPML4E; i++) {
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] = phystomach(VTOP(DMPDPphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] |= PG_V | PG_U;
	}
#endif /* SUPERPAGESUPPORT */

	/* And recursively map PML4 to itself in order to get PTmap */
	((pdp_entry_t *)KPML4phys)[PML4PML4I] = phystomach(VTOP(KPML4phys));
	((pdp_entry_t *)KPML4phys)[PML4PML4I] |= PG_V | PG_U;

	/* Connect the KVA slot up to the PML4 */
	((pdp_entry_t *)KPML4phys)[KPML4I] = phystomach(VTOP(KPDPphys));
	((pdp_entry_t *)KPML4phys)[KPML4I] |= PG_RW | PG_V | PG_U;

	pmap_xen_setpages_ro(KPML4phys, 1);

	xen_pgdir_pin(phystomach(VTOP(KPML4phys)));
}

/* 
 *
 * Map in the xen provided share page. Note: The console page is
 * mapped in later in the boot process, when kmem_alloc*() is
 * available. 
 */

static void
pmap_xen_bootpages(vm_paddr_t *firstaddr)
{
	uintptr_t va;
	vm_paddr_t ma;

	/* Share info */
	ma = xen_start_info->shared_info;

	/* This is a bit of a hack right now - we waste a physical
	 * page by overwriting its original mapping to point to
	 * the page we want ( thereby losing access to the
	 * original page ).
	 *
	 * The clean solution would have been to map it in at 
	 * KERNBASE + pa, where pa is the "pseudo-physical" address of
	 * the shared page that xen gives us. We can't seem to be able
	 * to use the pseudo-physical address in this way because the
	 * linear mapped virtual address seems to be outside of the
	 * range of PTEs that we have available during bootup (ptes
	 * take virtual address space which is limited to under 
	 * (512KB - (kernal binaries, stack et al.)) during xen
	 * bootup).
	 */

	va = vallocpages(firstaddr, 1);
	PT_SET_MA(va, ma | PG_RW | PG_V | PG_U);


	HYPERVISOR_shared_info = (void *) va;
}

/* alloc from linear mapped boot time virtual address space */
static uintptr_t
mmu_alloc(void)
{
	uintptr_t va;

	KASSERT(physfree != 0,
		("physfree must have been set before using mmu_alloc"));
				
	va = vallocpages(&physfree, atop(PAGE_SIZE));

	/* 
	 * Xen requires the page table hierarchy to be R/O.
	 */

	pmap_xen_setpages_ro(va, atop(PAGE_SIZE));

	return va;
}

void
pmap_bootstrap(vm_paddr_t *firstaddr)
{

	/* setup mmu_map backend function pointers for boot */
	ptmb_mappedalloc = mmu_alloc;
	ptmb_mappedfree = NULL;

	create_boot_pagetables(firstaddr);

	/* Switch to the new kernel tables */
	xen_pt_switch(VTOP(KPML4phys));

	/* Unpin old page table hierarchy, and mark all its pages r/w */
	xen_pgdir_unpin(phystomach(VTOP(xen_start_info->pt_base)));
	pmap_xen_setpages_rw(xen_start_info->pt_base,
			     xen_start_info->nr_pt_frames);

	/* 
	 * gc newly free pages (bootstrap PTs and bootstrap stack,
	 * mostly, I think.).
	 * Record the pages as available to the VM via phys_avail[] 
	 */

	/* This is the first free phys segment. see: xen.h */
	KASSERT(pa_index == 0, 
		("reclaimed page table pages are not the lowest available!"));

	dump_avail[pa_index + 1] = phys_avail[pa_index] = VTOP(xen_start_info->pt_base);
	dump_avail[pa_index + 2] = phys_avail[pa_index + 1] = phys_avail[pa_index] +
		ptoa(xen_start_info->nr_pt_frames - 1);
	pa_index += 2;

	/* Map in Xen related pages into VA space */
	pmap_xen_bootpages(firstaddr);

	/*
	 * Xen guarantees mapped virtual addresses at boot time upto
	 * xenstack + 512KB. We want to use these for vallocpages()
	 * and therefore don't want to touch these mappings since
	 * they're scarce resources. Move along to the end of
	 * guaranteed mapping.
	 *
	 * Note: Xen *may* provide mappings upto xenstack + 2MB, but
	 * this is not guaranteed. We therefore assum that only 512KB
	 * is available.
	 */

	virtual_avail = (uintptr_t) xenstack + 512 * 1024;
	/* XXX: Check we don't overlap xen pgdir entries. */
	virtual_end = VM_MAX_KERNEL_ADDRESS; 

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_pml4 = (pdp_entry_t *)KPML4phys;
	kernel_pmap->pm_root = NULL;
	CPU_FILL(&kernel_pmap->pm_active);	/* don't allow deactivation */
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);

	tsz = mmu_map_t_size();

	/* Steal some memory (backing physical pages, and kva) */
	physmem -= atop(round_page(msgbufsize));

	msgbufp = (void *) pmap_map(&virtual_avail,
				    ptoa(physmem), ptoa(physmem) + round_page(msgbufsize),
				    VM_PROT_READ | VM_PROT_WRITE);

	bzero(msgbufp, round_page(msgbufsize));
}

void
pmap_page_init(vm_page_t m)
{
	/* XXX: TODO - pv_lists */

}

/* 
 * Map in backing memory from kernel_vm_end to addr, 
 * and update kernel_vm_end.
 */
void
pmap_growkernel(uintptr_t addr)
{
	KASSERT(kernel_vm_end < addr, ("trying to shrink kernel VA!"));

	addr = trunc_page(addr);

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};

	mmu_map_t_init(tptr, &mb);

	for (;addr <= kernel_vm_end;addr += PAGE_SIZE) {
		
		if (mmu_map_inspect_va(kernel_pmap, tptr, addr)) {
			continue;
		}
		int pflags = VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED;
		vm_page_t m = vm_page_alloc(NULL, 0, pflags);
		KASSERT(m != NULL, ("Backing page alloc failed!"));
		vm_paddr_t pa = VM_PAGE_TO_PHYS(m);

		pmap_kenter(addr, pa);
	}

	mmu_map_t_fini(tptr);
}

void
pmap_init(void)
{
	uintptr_t va;

	/* XXX: review the use of gdtset for the purpose below */
	gdtset = 1; /* xpq may assert for locking sanity from this point onwards */

	/* XXX: switch the mmu_map.c backend to something more sane */

	/* Get a va for console and map the console mfn into it */
	vm_paddr_t console_ma = xen_start_info->console.domU.mfn << PAGE_SHIFT;

	va = kmem_alloc_nofault(kernel_map, PAGE_SIZE);
	KASSERT(va != 0, ("Could not allocate KVA for console page!\n"));

	pmap_kenter(va, xpmap_mtop(console_ma));
	console_page = (void *)va;
}

void
pmap_pinit0(pmap_t pmap)
{
	PMAP_LOCK_INIT(pmap);
	pmap->pm_pml4 = (void *) KPML4phys;
	pmap->pm_root = NULL;
	CPU_ZERO(&pmap->pm_active);
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

int
pmap_pinit(pmap_t pmap)
{

	KASSERT(pmap != kernel_pmap, 
		("kernel map re-initialised!"));

	PMAP_LOCK_INIT(pmap);

	/*
	 * allocate the page directory page
	 */
	pmap->pm_pml4 = (void *) kmem_alloc(kernel_map, PAGE_SIZE);
	bzero(pmap->pm_pml4, PAGE_SIZE);

	/* 
	 * We do not wire in kernel space, or the self-referencial
	 * entry in userspace pmaps for two reasons:
	 * i)  both kernel and userland run in ring3 (same CPU
	 *     privilege level). This means that userland that has kernel
	 *     address space mapped in, can access kernel memory!
	 *     Instead, we make the kernel pmap is exclusive and
	 *     unshared, and we switch to it on *every* kernel
	 *     entry. This is facilitated by the hypervisor.
	 * ii) we access the user pmap from within kernel VA. The
	 *     self-referencing entry is useful if we access the pmap
	 *     from the *user* VA.
	 * XXX: review this when userland is up.
	 */

	pmap->pm_root = NULL;
	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

	return 1;
}

void
pmap_release(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
}

__inline pt_entry_t *
vtopte(uintptr_t va)
{
	KASSERT(0, ("XXX: REVIEW\n"));
	u_int64_t mask = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT + NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1);

	return (PTmap + ((va >> PAGE_SHIFT) & mask));
}

#ifdef SMP
void pmap_lazyfix_action(void);

void
pmap_lazyfix_action(void)
{
	KASSERT(0, ("XXX: TODO\n"));
}
#endif /* SMP */

/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 * XXX: TODO SMP.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */

void
pmap_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{
	KASSERT(count > 0, ("count > 0"));
	KASSERT(sva == trunc_page(sva),
		("sva not page aligned"));
	KASSERT(ma != NULL, ("ma != NULL"));
	vm_page_t m;
	vm_paddr_t pa;

	while (count--) {
		m = *ma++;
		pa = VM_PAGE_TO_PHYS(m);
		pmap_kenter(sva, pa);
		sva += PAGE_SIZE;
	}
	// XXX: TODO: pmap_invalidate_range(kernel_pmap, sva, sva + count *
	//	      PAGE_SIZE);

}

void
pmap_qremove(vm_offset_t sva, int count)
{
	KASSERT(0, ("XXX: TODO\n"));
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
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	va = trunc_page(va);
	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%lx)",
	    va));
	KASSERT((m->oflags & (VPO_UNMANAGED | VPO_BUSY)) != 0 ||
	    VM_OBJECT_LOCKED(m->object),
	    ("pmap_enter: page %p is not busy", m));

	pmap_kenter(va, VM_PAGE_TO_PHYS(m)); /* Shim to keep bootup
					      * happy for now */

	/* XXX: TODO: */
}

void
pmap_enter_object(pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	KASSERT(0, ("XXX: TODO\n"));
	return NULL;
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_all(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

vm_paddr_t 
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	return xpmap_mtop(pmap_kextract_ma(va));
}

vm_paddr_t
pmap_kextract_ma(vm_offset_t va)
{
	vm_paddr_t ma;

	/* Walk the PT hierarchy to get the ma */
	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	const uint64_t SIGNMASK = (1UL << 48) - 1;
	va &= SIGNMASK; /* Remove sign extension */

	mmu_map_t tptr = tbuf;

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};
	mmu_map_t_init(tptr, &mb);

	if (!mmu_map_inspect_va(kernel_pmap, tptr, va)) {
		ma = 0;
		goto nomapping;
	}

	ma = mmu_map_pt(tptr)[(PDRMASK & va) >> PAGE_SHIFT];

	mmu_map_t_fini(tptr);

nomapping:
	return ma;
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Add a wired page to the kva.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */

void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pmap_kenter_ma(va, xpmap_ptom(pa));
}

void 
pmap_kenter_ma(vm_offset_t va, vm_paddr_t ma)
{

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};
	mmu_map_t_init(tptr, &mb);

	if (!mmu_map_inspect_va(kernel_pmap, tptr, va)) {
		mmu_map_hold_va(kernel_pmap, tptr, va); /* PT hierarchy */
		xen_flush_queue();
	}

	/* Backing page tables are in place, let xen do the maths */
	PT_SET_MA(va, ma | PG_RW | PG_V | PG_U);
	PT_UPDATES_FLUSH();

	mmu_map_release_va(kernel_pmap, tptr, va);
	mmu_map_t_fini(tptr);
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */

__inline void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	PT_CLEAR_VA(pte, FALSE);
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
	vm_offset_t va, sva;

	va = sva = *virt;

	CTR4(KTR_PMAP, "pmap_map: va=0x%x start=0x%jx end=0x%jx prot=0x%x",
	    va, start, end, prot);

	while (start < end) {
		pmap_kenter(va, start);
		va += PAGE_SIZE;
		start += PAGE_SIZE;
	}

	// XXX: pmap_invalidate_range(kernel_pmap, sva, va);
	*virt = va;

	return (sva);
}

void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	/* 
	 * XXX: TODO - ignore for now - we need to revisit this as
	 * soon as  kdb is up.
	 */
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr, 
	  vm_size_t len, vm_offset_t src_addr)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	/* XXX: temp fix, dmap not yet implemented. */
	pmap_kenter(va, VM_PAGE_TO_PHYS(m));

	pagezero((void *)va);
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_zero_page_idle(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_activate(struct thread *td)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_pages(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	KASSERT(0, ("XXX: TODO\n"));
}

boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

int
pmap_page_wired_mappings(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_is_referenced(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

void
pmap_clear_modify(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_clear_reference(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_write(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_ts_referenced(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	KASSERT(0, ("XXX: TODO\n"));
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

void
pmap_suspend()
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_resume()
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{	
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
  	KASSERT(0, ("XXX: TODO\n"));
	return NULL;
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
		KASSERT(0, ("XXX: TODO\n"));
		return -1;
}

static uintptr_t
xen_pagezone_alloc(void)
{
	uintptr_t ret;

	ret = (uintptr_t)uma_zalloc(xen_pagezone, M_NOWAIT | M_ZERO);
	if (ret == 0)
		panic("%s: failed allocation\n", __func__);
	return (ret);
}

static void
xen_pagezone_free(vm_offset_t page)
{

	uma_zfree(xen_pagezone, (void *)page);
}

static int
xen_pagezone_init(void *mem, int size, int flags)
{
	uintptr_t va;

	va = (uintptr_t)mem;

	/* Xen requires the page table hierarchy to be R/O. */
	pmap_xen_setpages_ro(va, atop(size));
	return (0);
}

/*
 * Replace the custom mmu_alloc(), backed by vallocpages(), with an
 * uma backed allocator, as soon as it is possible.
 */ 
static void
setup_xen_pagezone(void *dummy __unused)
{

	xen_pagezone = uma_zcreate("XEN PAGEZONE", PAGE_SIZE, NULL, NULL,
	    xen_pagezone_init, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	ptmb_mappedalloc = xen_pagezone_alloc;
	ptmb_mappedfree = xen_pagezone_free;
}
SYSINIT(setup_xen_pagezone, SI_SUB_VM_CONF, SI_ORDER_ANY, setup_xen_pagezone,
    NULL);
