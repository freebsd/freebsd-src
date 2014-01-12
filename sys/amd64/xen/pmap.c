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
 * Copyright (c) 2012 Citrix Systems
 * All rights reserved.
 * Copyright (c) 2012, 2013 Spectra Logic Corporation
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
#include <sys/mman.h>
#include <sys/rwlock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>

#ifdef SMP
#include <sys/smp.h>
#endif

#include <sys/proc.h>
#include <sys/sched.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/uma.h>

#include <machine/md_var.h>
#include <machine/pcb.h>

#include <xen/hypervisor.h>
#include <machine/xen/xenvar.h>

#include <amd64/xen/mmu_map.h>
#include <amd64/xen/pmap_pv.h>

extern vm_offset_t pa_index; /* from machdep.c */
extern unsigned long physfree; /* from machdep.c */

struct pmap kernel_pmap_store;

#define ISDMAPVA(va) ((va) >= DMAP_MIN_ADDRESS && \
		      (va) <= DMAP_MAX_ADDRESS)
#define ISKERNELVA(va) ((va) >= VM_MIN_KERNEL_ADDRESS && \
			(va) <= VM_MAX_KERNEL_ADDRESS)
#define ISBOOTVA(va) ((va) >= KERNBASE && (va) <= virtual_avail) /* XXX: keep an eye on virtual_avail */

uintptr_t virtual_avail;	/* VA of first avail page (after kernel bss) */
uintptr_t virtual_end;	/* VA of last avail page (end of kernel AS) */

int nkpt;

/* 
 * VA for temp mapping to zero.
 * We need this because on xen, the DMAP is R/O
 */
const uintptr_t zerova = VM_MAX_KERNEL_ADDRESS;

#define DMAP4KSUPPORT /* Temporary 4K based DMAP support */
#ifdef DMAPSUPPORT
static int ndmpdp;
static vm_paddr_t dmaplimit;
#endif

uintptr_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;
pt_entry_t pg_nx = 0; /* XXX: probe for this ? */

struct msgbuf *msgbufp = 0;

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
u_int64_t		KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */

#if defined(DMAPSUPPORT) || defined(DMAP4KSUPPORT)
#ifdef DMAP4KSUPPORT
static u_int64_t	DMPTphys;	/* phys addr of direct mapped level 1 */
#endif
static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */
#endif /* DMAPSUPPORT || DMAP4KSUPPORT */

static vm_paddr_t	boot_ptphys;	/* phys addr of start of
					 * kernel bootstrap tables
					 */
static vm_paddr_t	boot_ptendphys;	/* phys addr of end of kernel
					 * bootstrap page tables
					 */

static size_t tsz; /* mmu_map.h opaque cookie size */
static uintptr_t (*ptmb_mappedalloc)(void) = NULL;
static void (*ptmb_mappedfree)(uintptr_t) = NULL;
static uintptr_t (*ptmb_ptov)(vm_paddr_t) = NULL;
static vm_paddr_t (*ptmb_vtop)(uintptr_t) = NULL;

extern int gdtset;
extern uint64_t xenstack; /* The stack Xen gives us at boot */
extern char *console_page; /* The shared ring for console i/o */
extern struct xenstore_domain_interface *xen_store; /* xenstore page */

extern vm_map_t pv_map;

/* Index offset into a pagetable, for a given va */
static int
pt_index(uintptr_t va)
{
	return ((va & PDRMASK) >> PAGE_SHIFT);
}


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
			  phystomach(ptmb_vtop(va + PAGE_SIZE * i)) | PG_U | PG_V);
	}
}

/* Set page addressed by va to r/w */
static void
pmap_xen_setpages_rw(uintptr_t va, vm_size_t npages)
{
	vm_size_t i;
	for (i = 0; i < npages; i++) {
		PT_SET_MA(va + PAGE_SIZE * i, 
			  phystomach(ptmb_vtop(va + PAGE_SIZE * i)) | PG_U | PG_V | PG_RW);
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
uintptr_t tmpva;

static void
create_boot_pagetables(vm_paddr_t *firstaddr)
{
	int i;
	int nkpdpe;
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

#ifdef DMAPSUPPORT
	int ndm1g;
	ndmpdp = (ptoa(Maxmem) + NBPDP - 1) >> PDPSHIFT;
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	DMPDPphys = vallocpages(firstaddr, NDMPML4E);
	ndm1g = 0;
	if ((amd_feature & AMDID_PAGE1GB) != 0)
		ndm1g = ptoa(Maxmem) >> PDPSHIFT;

	if (ndm1g < ndmpdp)
		DMPDphys = vallocpages(firstaddr, ndmpdp - ndm1g);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;
#endif /* DMAPSUPPORT */
#ifdef DMAP4KSUPPORT
	int ndmapped = physmem;
	int ndmpt = howmany(ndmapped, NPTEPG);
	int ndmpdpe = howmany(ndmpt, NPDEPG);
	tmpva = ptoa(ndmapped);

	DMPDPphys = vallocpages(firstaddr, NDMPML4E);
	DMPDphys = vallocpages(firstaddr, ndmpdpe);
	DMPTphys = vallocpages(firstaddr, ndmpt);

	for (i = 0;ptoa(i) < ptoa(ndmapped); i++) {
		((pt_entry_t *)DMPTphys)[i] = phystomach(i << PAGE_SHIFT);
		((pt_entry_t *)DMPTphys)[i] |= PG_V | PG_U;
	}

	pmap_xen_setpages_ro(DMPTphys, (i - 1) / NPTEPG + 1);

	for (i = 0; i < ndmpt; i ++) {
		((pd_entry_t *)DMPDphys)[i] = phystomach(VTOP(DMPTphys) +
							 (i << PAGE_SHIFT));
		((pd_entry_t *)DMPDphys)[i] |= PG_V | PG_U;

	}

	pmap_xen_setpages_ro(DMPDphys, (ndmpt - 1) / NPDEPG + 1);

	for (i = 0; i < ndmpdpe; i++) {
		((pdp_entry_t *)DMPDPphys)[i] = phystomach(VTOP(DMPDphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)DMPDPphys)[i] |= PG_V | PG_U;
	}

	pmap_xen_setpages_ro(DMPDPphys, (ndmpdpe - 1) / NPDPEPG + 1);

	/* Connect the Direct Map slot(s) up to the PML4. */
	for (i = 0; i < NDMPML4E - 1; i++) {
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] = phystomach(VTOP(DMPDPphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] |= PG_V | PG_U;
	}
#endif /* DMAP4KSUPPORT */	

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

#ifdef SUPERPAGESUPPORT /* XXX: work out r/o overlaps and 2M machine pages*/
	/* Map from zero to end of allocations under 2M pages */
	/* This replaces some of the KPTphys entries above */
	for (i = 0; (i << PDRSHIFT) < *firstaddr; i++) {
		((pd_entry_t *)KPDphys)[i] = phystomach(i << PDRSHIFT);
		((pd_entry_t *)KPDphys)[i] |= PG_U | PG_RW | PG_V | PG_PS | PG_G;
	}
#endif

	pmap_xen_setpages_ro(KPDphys, (nkpt - 1) / NPDEPG + 1);

	/* And connect up the PD to the PDP */
	for (i = 0; i < nkpdpe; i++) {
		((pdp_entry_t *)KPDPphys)[i + KPDPI] = phystomach(VTOP(KPDphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPDPphys)[i + KPDPI] |= PG_RW | PG_V | PG_U;
	}

	pmap_xen_setpages_ro(KPDPphys, (nkpdpe - 1) / NPDPEPG + 1);

#ifdef DMAPSUPPORT
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
#endif /* DMAPSUPPORT */

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

	/* i) Share info */
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

#if 0
	/* ii) Userland page table base */
	va = vallocpages(firstaddr, 1);
	bzero((void *)va, PAGE_SIZE);

	/* 
	 * x86_64 has 2 privilege rings and Xen keeps separate pml4
	 * pointers for each, which are sanity checked on every
	 * exit via hypervisor_iret. We therefore set up a zeroed out
	 * user page table pml4 to satisfy/fool xen.
	 */
	
	/* Mark the page r/o before pinning */
	pmap_xen_setpages_ro(va, 1);

	/* Pin the table */
	xen_pgdir_pin(phystomach(VTOP(va)));

	/* Register user page table with Xen */
	xen_pt_user_switch(xpmap_ptom(VTOP(va)));
#endif
}

/* Boot time ptov - xen guarantees bootpages to be offset */
static uintptr_t boot_ptov(vm_paddr_t p)
{
	return PTOV(p);
}

/* Boot time vtop - xen guarantees bootpages to be offset */
static vm_paddr_t boot_vtop(uintptr_t v)
{
	return VTOP(v);
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
	ptmb_ptov = boot_ptov;
	ptmb_vtop = boot_vtop;

	create_boot_pagetables(firstaddr);

	/* Switch to the new kernel tables */
	xen_pt_switch(xpmap_ptom(VTOP(KPML4phys)));

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
		ptoa(xen_start_info->nr_pt_frames);
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
	virtual_end = VM_MAX_KERNEL_ADDRESS - PAGE_SIZE; 

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_pml4 = (pdp_entry_t *)KPML4phys;
	kernel_pmap->pm_cr3 = pmap_kextract_ma((vm_offset_t) KPML4phys);
	kernel_pmap->pm_root.rt_root = 0;
	CPU_FILL(&kernel_pmap->pm_active);	/* don't allow deactivation */
	pmap_pv_init();
	pmap_pv_pmap_init(kernel_pmap);

	tsz = mmu_map_t_size();

	/* Steal some memory (backing physical pages, and kva) */
	physmem -= atop(round_page(msgbufsize));

	msgbufp = (void *) pmap_map(&virtual_avail,
				    ptoa(physmem), ptoa(physmem) + round_page(msgbufsize),
				    VM_PROT_READ | VM_PROT_WRITE);

	bzero(msgbufp, round_page(msgbufsize));
}

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{
	pmap_pv_vm_page_init(m);
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

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */

void
pmap_init(void)
{
	uintptr_t va;

	/* XXX: review the use of gdtset for the purpose below */

	/* Get a va for console and map the console mfn into it */
	vm_paddr_t ma = xen_start_info->console.domU.mfn << PAGE_SHIFT;

	va = kva_alloc(PAGE_SIZE);
	KASSERT(va != 0, ("Could not allocate KVA for console page!\n"));

	pmap_kenter_ma(va, ma);
	console_page = (void *)va;

	/* Get a va for the xenstore shared page */
	ma = xen_start_info->store_mfn << PAGE_SHIFT;

	va = kva_alloc(PAGE_SIZE);
	KASSERT(va != 0, ("Could not allocate KVA for xenstore page!\n"));

	pmap_kenter_ma(va, ma);
	xen_store = (void *)va;

	/* Reserve pv VA space by allocating a submap */
	KASSERT(kernel_map != 0, ("Initialising kernel submap before kernel_map!"));

	gdtset = 1; /* xpq may assert for locking sanity from this point onwards */

}

void
pmap_pinit0(pmap_t pmap)
{
	PMAP_LOCK_INIT(pmap);
	pmap->pm_pml4 = (void *) KPML4phys;
	pmap->pm_cr3 = pmap_kextract_ma((vm_offset_t) KPML4phys);
	pmap->pm_root.rt_root = 0;
	CPU_ZERO(&pmap->pm_active);
	CPU_ZERO(&pmap->pm_save);
	PCPU_SET(curpmap, pmap);
	pmap_pv_pmap_init(pmap);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_pcid = -1;
}

int
pmap_pinit(pmap_t pmap)
{

	KASSERT(pmap != kernel_pmap, 
		("%s: kernel map re-initialised!", __func__));

	/*
	 * allocate the page directory page
	 */
	pmap->pm_pml4 = (void *) kmem_malloc(kernel_arena, PAGE_SIZE, M_ZERO);
	if (pmap->pm_pml4 == NULL) return 0;

	pmap->pm_cr3 = pmap_kextract_ma((vm_offset_t)pmap->pm_pml4);

	/* 
	 * We do not wire in kernel space, or the self-referencial
	 * entry in userspace pmaps becase both kernel and userland
	 * share ring3 privilege. The user/kernel context switch is
	 * arbitrated by the hypervisor by means of pre-loaded values
	 * for kernel and user %cr3. The userland parts of kernel VA
	 * may be conditionally overlaid with the VA of curthread,
	 * since the kernel occasionally needs to access userland
	 * process VA space.
	 */

	pmap_xen_setpages_ro((uintptr_t)pmap->pm_pml4, 1);

	xen_pgdir_pin(pmap->pm_cr3);

	pmap->pm_root.rt_root = 0;
	CPU_ZERO(&pmap->pm_active);
	pmap_pv_pmap_init(pmap);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_pcid = -1; /* No pcid for now */
	CPU_ZERO(&pmap->pm_save);

	return 1;
}

void pmap_xen_userload(pmap_t pmap)
{
	KASSERT(pmap != kernel_pmap, 
		("Kernel pmap requested on user load.\n"));

	int i;
	for (i = 0; i < NUPML4E; i++) {
		pml4_entry_t pml4e;
		pml4e = (pmap->pm_pml4[i]);
		PT_SET_VA_MA((pml4_entry_t *)KPML4phys + i, pml4e, false);
	}
	PT_UPDATES_FLUSH();
	invltlb();

	/* Tell xen about user pmap switch */
	xen_pt_user_switch(pmap->pm_cr3);
}

void
pmap_release(pmap_t pmap)
{
	KASSERT(pmap != kernel_pmap,
		("%s: kernel pmap released", __func__));

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	KASSERT(vm_radix_is_empty(&pmap->pm_root),
	    ("pmap_release: pmap has reserved page table page(s)"));

	xen_pgdir_unpin(pmap->pm_cr3);
	pmap_xen_setpages_rw((uintptr_t)pmap->pm_pml4, 1);

	bzero(pmap->pm_pml4, PAGE_SIZE);
	kmem_free(kernel_arena, (vm_offset_t)pmap->pm_pml4, PAGE_SIZE);
}

static pt_entry_t *
pmap_vtopte_inspect(pmap_t pmap, uintptr_t va, void *addr)
{
	KASSERT(addr != NULL, ("addr == NULL"));

	mmu_map_t tptr = *(mmu_map_t *)addr;

	pd_entry_t *pte; /* PTE address to return */

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};

	mmu_map_t_init(tptr, &mb);

	if (!mmu_map_inspect_va(pmap, tptr, va)) {
		return NULL; /* XXX: fix api, return some kind of #define */
	}

	pte = mmu_map_pt(tptr); /* Read out PT from mmu state */

	/* add VA offset */
	pte += pt_index(va);

	return pte;
}

static pt_entry_t *
vtopte_inspect(uintptr_t va, void *addr)
{
	return pmap_vtopte_inspect(kernel_pmap, va, addr);
}

static pt_entry_t *
pmap_vtopte_hold(pmap_t pmap, uintptr_t va, void *addr)
{
	KASSERT(addr != NULL, ("addr == NULL"));

	mmu_map_t tptr = *(mmu_map_t *)addr;

	pd_entry_t *pte; /* PTE address to return */

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};

	mmu_map_t_init(tptr, &mb);

	if (!mmu_map_inspect_va(pmap, tptr, va)) {
		mmu_map_hold_va(pmap, tptr, va); /* PT hierarchy */
		xen_flush_queue();
	}

	pte = mmu_map_pt(tptr); /* Read out PT from mmu state */

	/* add VA offset */
	pte += pt_index(va);

	return pte;
}

pt_entry_t *
vtopte_hold(uintptr_t va, void *addr)
{
	return pmap_vtopte_hold(kernel_pmap, va, addr);
}

static void
pmap_vtopte_release(pmap_t pmap, uintptr_t va, void *addr)
{
	mmu_map_t tptr = *(mmu_map_t *)addr;

	mmu_map_release_va(pmap, tptr, va);
	mmu_map_t_fini(tptr);

}

void
vtopte_release(uintptr_t va, void *addr)
{
	pmap_vtopte_release(kernel_pmap, va, addr);
}

#ifdef SMP
void pmap_lazyfix_action(void);

void
pmap_lazyfix_action(void)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}
#endif /* SMP */

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
	pmap->pm_stats.resident_count -= count;
}

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

/*
 * This routine tears out page mappings from the
 * kernel -- it is meant only for temporary mappings.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	KASSERT(count > 0, ("count > 0"));
	KASSERT(sva == trunc_page(sva),
		("sva not page aligned"));

	vm_offset_t va;

	va = sva;
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	xen_flush_queue();
	// XXX: TODO: pmap_invalidate_range(kernel_pmap, sva, va);
}

static void
pmap_enter_locked(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	pt_entry_t *pte;
	pt_entry_t newpte, origpte;
	vm_paddr_t opa, pa;
	vm_page_t om;

	va = trunc_page(va);

	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%lx)",
	    va));

	KASSERT(VM_PAGE_TO_PHYS(m) != 0,
		("VM_PAGE_TO_PHYS(m) == 0x%lx\n", VM_PAGE_TO_PHYS(m)));

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	pa = VM_PAGE_TO_PHYS(m);
	newpte = (pt_entry_t)(xpmap_ptom(pa) | PG_A | PG_V | PG_U);
	if ((access & VM_PROT_WRITE) != 0)
		newpte |= PG_M;
	if ((prot & VM_PROT_WRITE) != 0)
		newpte |= PG_RW;
	KASSERT((newpte & (PG_M | PG_RW)) != PG_M,
	    ("pmap_enter: access includes VM_PROT_WRITE but prot doesn't"));
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;
	if (wired)
		newpte |= PG_W;

	/* newpte |= pmap_cache_bits(m->md.pat_mode, 0); XXX */

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */

	KASSERT(tsz != 0, ("tsz != 0"));

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	pte = pmap_vtopte_hold(pmap, va, &tptr);

	origpte = *pte;

	/*
	 * Is the specified virtual address already mapped?
	 */
	if ((origpte & PG_V) != 0) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if ((newpte & PG_W) != 0 && (origpte & PG_W) == 0)
			pmap->pm_stats.wired_count++;
		else if ((newpte & PG_W) == 0 && (origpte & PG_W) != 0)
			pmap->pm_stats.wired_count--;
		
		/*
		 * Has the physical page changed?
		 */
		opa = xpmap_mtop(origpte & PG_FRAME);
		if (opa == pa) {
			/*
			 * No, might be a protection or wiring change.
			 */
			if ((origpte & PG_MANAGED) != 0) {
				newpte |= PG_MANAGED;
				if ((newpte & PG_RW) != 0)
					vm_page_aflag_set(m, PGA_WRITEABLE);
			}
			if (((origpte ^ newpte) & ~(PG_M | PG_A)) == 0)
				goto unchanged;
			goto validate;
		}
	} else {
		/*
		 * Increment the counters.
		 */
		if ((newpte & PG_W) != 0)
			pmap->pm_stats.wired_count++;
		pmap_resident_count_inc(pmap, 1);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		bool pvunmanaged = false;
		newpte |= PG_MANAGED;
		pvunmanaged = pmap_put_pv_entry(pmap, va, m);

		KASSERT(pvunmanaged == true,
		    ("VPO_UNMANAGED flag set on existing pv entry for m == %p\n", m));

		if ((newpte & PG_RW) != 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}

	/*
	 * Update the PTE.
	 */
	if ((origpte & PG_V) != 0) {
validate:
		{
			/* XXX: This is not atomic */
			origpte = *pte;
			PT_SET_VA_MA(pte, newpte, true);
			/* Sync the kernel's view of the pmap */
			if (pmap != kernel_pmap && PCPU_GET(curpmap) == pmap) {
				/* XXX: this can be optimised to a single entry update */
				pmap_xen_userload(pmap);
			}

		}
		opa = xpmap_mtop(origpte & PG_FRAME);
		if (opa != pa) {
			if ((origpte & PG_MANAGED) != 0) {
				om = PHYS_TO_VM_PAGE(opa);
				if ((origpte & (PG_M | PG_RW)) == (PG_M |
				    PG_RW))
					vm_page_dirty(om);
				if ((origpte & PG_A) != 0)
					vm_page_aflag_set(om, PGA_REFERENCED);
				if (!pmap_free_pv_entry(pmap, va, om)) {
					panic("Unable to free pv entry!");
				}

				if ((om->aflags & PGA_WRITEABLE) != 0 &&
				    !pmap_page_is_mapped(om) &&
				    (om->flags & PG_FICTITIOUS) != 0)
					vm_page_aflag_clear(om, PGA_WRITEABLE);
			}
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
		}

		if ((origpte & PG_A) != 0)
			pmap_invalidate_page(pmap, va);

	} else {
		PT_SET_VA_MA(pte, newpte, true);

		/* Sync the kernel's view of the pmap */
		if (pmap != kernel_pmap && PCPU_GET(curpmap) == pmap) {
			/* XXX: this can be optimised to a single entry update */
			pmap_xen_userload(pmap);
		}
	}


	if (pmap != kernel_pmap) pmap_xen_userload(pmap);


unchanged:
	pmap_vtopte_release(pmap, va, &tptr);

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

	if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
		VM_OBJECT_ASSERT_WLOCKED(m->object);

	KASSERT(VM_PAGE_TO_PHYS(m) != 0,
		("VM_PAGE_TO_PHYS(m) == 0x%lx\n", VM_PAGE_TO_PHYS(m)));


	PMAP_LOCK(pmap);

	pmap_enter_locked(pmap, va, access, m, prot, wired);

	PMAP_UNLOCK(pmap);
}

static void
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{

	pt_entry_t *pte;
	pt_entry_t newpte, origpte;
	vm_paddr_t pa;

	va = trunc_page(va);

	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%lx)",
	    va));

	KASSERT(VM_PAGE_TO_PHYS(m) != 0,
		("VM_PAGE_TO_PHYS(m) == 0x%lx\n", VM_PAGE_TO_PHYS(m)));

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	pa = VM_PAGE_TO_PHYS(m);
	newpte = (pt_entry_t)(xpmap_ptom(pa) | PG_V | PG_U);

	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */

	KASSERT(tsz != 0, ("tsz != 0"));

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	pte = pmap_vtopte_hold(pmap, va, &tptr);

	/* XXX: if (pte == NULL) bail; */

	origpte = *pte;

	/*
	 * Is the specified virtual address already mapped? We're done.
	 */
	if ((origpte & PG_V) != 0) {
		goto done;
				       
	}	

	/*
	 * Enter on the PV list if part of our managed memory.
	 */

	if ((m->oflags & VPO_UNMANAGED) == 0) {
		bool pvunmanaged = false;
		newpte |= PG_MANAGED;
		pvunmanaged = pmap_put_pv_entry(pmap, va, m);

		if (pvunmanaged != true) {
			/* already managed entry - we're done */
			goto done;
		}
	}

	/*
	 * Increment the counters.
	 */
	pmap_resident_count_inc(pmap, 1);

	/*
	 * Update the PTE.
	 */
	PT_SET_VA_MA(pte, newpte, true);

	/* Sync the kernel's view of the pmap */
	if (pmap != kernel_pmap && 
	    PCPU_GET(curpmap) == pmap) {
	  /* XXX: this can be optimised to a single entry update */
	  pmap_xen_userload(pmap);
	}

done:
	pmap_vtopte_release(pmap, va, &tptr);
}

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	/* RO and unwired */
	prot = (prot & ~VM_PROT_WRITE) | VM_PROT_READ;

	va = trunc_page(va);

	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("%s: toobig", __func__));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
		("%s: invalid to pmap_enter page table pages (va: 0x%lx)", __func__, va));

	KASSERT(VM_PAGE_TO_PHYS(m) != 0,
		("VM_PAGE_TO_PHYS(m) == 0x%lx\n", VM_PAGE_TO_PHYS(m)));

	PMAP_LOCK(pmap);

	pmap_enter_quick_locked(pmap, va, m, prot);

	PMAP_UNLOCK(pmap);
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
	vm_pindex_t diff, psize;
	vm_page_t m;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	m = m_start;

	PMAP_LOCK(pmap);

	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		pmap_enter_quick_locked(pmap, va, m, prot);
		m = TAILQ_NEXT(m, listq);
	}

	PMAP_UNLOCK(pmap);
}

void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return NULL;
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, vm_offset_t va, pt_entry_t *ptq)
{
	pt_entry_t oldpte;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	{ /* XXX: there's no way to make this atomic ? */
		oldpte = *ptq;
		KASSERT(oldpte & PG_V, ("Invalid pte\n"));
		if (oldpte & PG_FRAME) { /* Optimise */
			PT_CLEAR_VA(ptq, TRUE);
		}
	}
	
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	pmap_resident_count_dec(pmap, 1);

	if (oldpte & PG_MANAGED) {
		m = MACH_TO_VM_PAGE(oldpte & PG_FRAME);

		if ((oldpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);

		if (oldpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);

		if (!pmap_free_pv_entry(pmap, va, m)) {
			panic("%s: pv 0x%lx: 0x%lx, unknown on managed page!", 
			      __func__, VM_PAGE_TO_PHYS(m), va);
		}

		if (!pmap_page_is_mapped(m) &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
	}
	/* 
	 * We never remove the backing pages - that's the job of
	 * mmu_map.[ch]
	 */
	return false; 
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va, pt_entry_t *pte)
{
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((*pte & PG_V) == 0)
		return;

	pmap_remove_pte(pmap, va, pte);

	pmap_invalidate_page(pmap, va);
}

void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(eva >= sva, ("End VA is lower than Start VA"));
	vm_offset_t va, va_next;
	pt_entry_t *pte;
	int anyvalid;

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = 0;

	KASSERT(tsz != 0, ("tsz != 0"));

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


	PMAP_LOCK(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */

	if (sva + PAGE_SIZE == eva) {
		if (!mmu_map_inspect_va(pmap, tptr, sva)) {
			goto out;
		}
		      
		pte = mmu_map_pt(tptr) + pt_index(sva);

		pmap_remove_page(pmap, sva, pte);
		mmu_map_release_va(pmap, tptr, sva);
		goto out;
	}

	for (; sva < eva; sva = va_next) {
		if (pmap->pm_stats.resident_count == 0)
			break;

		if (!mmu_map_inspect_va(pmap, tptr, sva)) {
			if (mmu_map_pdpt(tptr) == NULL) {
				va_next = (sva + NBPML4) & ~PML4MASK;
				if (va_next < sva) /* Overflow */
					va_next = eva;
				continue;
			}

			if (mmu_map_pdt(tptr) == NULL) {
				va_next = (sva + NBPDP) & ~PDPMASK;
				if (va_next < sva) /* Overflow */
					va_next = eva;
				continue;
			}

			if (mmu_map_pt(tptr) == NULL) {
				va_next = (sva + NBPDR) & ~PDRMASK;
				if (va_next < sva) /* Overflow */
					va_next = eva;
				continue;
			}

			panic("%s: All backing tables non-NULL,"
			      "yet hierarchy can't be inspected at va = 0x%lx\n", 
			      __func__, sva);
		}

		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		va = va_next;

		for (pte = (mmu_map_pt(tptr) + pt_index(sva)); 
		     sva != va_next;pte++, sva += PAGE_SIZE) {
			if ((*pte & PG_V) == 0) {
				if (va != va_next) {
					pmap_invalidate_range(pmap, sva, va);
					va = va_next;
				}
				continue;
			}
			
			/*
			 * XXX: PG_G is set on *user* entries unlike
			 * native, where it is set on kernel entries 
			 */ 
			if ((*pte & PG_G) != 0) 
				anyvalid = 1;
			else if (va == va_next)
				va = sva;

			pmap_remove_pte(pmap, sva, pte);
			mmu_map_release_va(pmap, tptr, sva);
		}
		if (va != va_next) {
			pmap_invalidate_range(pmap, sva, va);
		}
	}
out:
	if (anyvalid)
		pmap_invalidate_all(pmap);

	PMAP_UNLOCK(pmap);
	mmu_map_t_fini(tptr);
}

static bool
pv_remove(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pt_entry_t *pte, tpte;

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	KASSERT(m != NULL, ("%s: passed NULL page!", __func__));

	PMAP_LOCK(pmap);
	pte = pmap_vtopte_inspect(pmap, va, &tptr);

	KASSERT(pte != NULL, ("pte has no backing page tables!"));

	tpte = *pte;
	PT_CLEAR_VA(pte, TRUE);
	if (tpte & PG_A)
		vm_page_aflag_set(m, PGA_REFERENCED);

	/*
	 * Update the vm_page_t clean and reference bits.
	 */
	if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
		vm_page_dirty(m);

	/* XXX: Tell mmu_xxx about backing page */
	pmap_vtopte_release(pmap, va, &tptr);

	pmap_resident_count_dec(pmap, 1);

	pmap_invalidate_page(pmap, va);
	PMAP_UNLOCK(pmap);

	return false;
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 */
void
pmap_remove_all(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));

	pmap_pv_iterate(m, pv_remove, PV_RW_ITERATE);

	/* free pv entry from all pmaps */
	pmap_pv_page_unmap(m);
}

vm_paddr_t 
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	KASSERT(pmap == kernel_pmap, ("XXX: %s: TODO\n", __func__));

	return 	pmap_kextract(va);
}

vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	PMAP_LOCK(pmap);

	pt_entry_t *pte;
	vm_page_t m;
	vm_paddr_t ma, pa;

	m = NULL;
	pa = ma = 0;

	/* Walk the PT hierarchy to get the ma */
	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;
retry:
	pte = pmap_vtopte_inspect(pmap, va, &tptr);

	if (pte == NULL) {
		goto nomapping;
	}

	if ((*pte & PG_V) == 0) {
		goto nomapping;
	}

	ma = *pte & PG_FRAME;

	if (ma == 0) {
		goto nomapping;
	}

	if (((*pte & PG_RW) ||
	     prot & VM_PROT_WRITE) == 0) {
		if (vm_page_pa_tryrelock(pmap, ma, &pa)) goto retry;

		m = MACH_TO_VM_PAGE(ma);
		KASSERT(m != NULL, ("Invalid ma from pte"));

		vm_page_hold(m);
	}

nomapping:	
	pmap_vtopte_release(pmap, va, &tptr);

	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);

	return m;
}

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	vm_paddr_t ma;
	ma = pmap_kextract_ma(va);

	KASSERT((ma & ~PAGE_MASK) != 0, ("%s: Unmapped va: 0x%lx \n", __func__, va));

	return xpmap_mtop(ma);
}

vm_paddr_t
pmap_kextract_ma(vm_offset_t va)
{

	if (ISDMAPVA(va)) {
		return xpmap_ptom(DMAP_TO_PHYS(va));
	}

	vm_paddr_t ma;

	/* Walk the PT hierarchy to get the ma */
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
		ma = 0;
		goto nomapping;
	}

	ma = mmu_map_pt(tptr)[pt_index(va)];

	mmu_map_t_fini(tptr);

nomapping:
	return (ma & PG_FRAME) | (va & PAGE_MASK);
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

	KASSERT(tsz != 0, ("tsz != 0"));

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
		xen_flush_queue(); /* XXX: cleanup */
	}

	/* Backing page tables are in place, let xen do the maths */
	PT_SET_MA(va, ma | PG_RW | PG_V | PG_U);
	PT_UPDATES_FLUSH();

	mmu_map_t_fini(tptr);
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */

void
pmap_kremove(vm_offset_t va)
{

	pt_entry_t *pte;
	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	pte = vtopte_inspect(va, &tptr);
	if (pte == NULL) { /* Mapping doesn't exist */
		goto notmapped;
	}

	PT_CLEAR_VA(pte, TRUE);
	PT_UPDATES_FLUSH();

	pmap_invalidate_page(kernel_pmap, va);
notmapped:
//	XXX: vtopte_release(va, &tptr);
	mmu_map_t_fini(tptr);
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

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */


void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t addr;
	vm_offset_t va_next;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;

	PMAP_LOCK(pmap);

	/* XXX: unify va range operations over ptes across functions */

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

	for (addr = sva; addr < eva; addr = va_next) {
		pt_entry_t *pte;

		if (!mmu_map_inspect_va(pmap, tptr, addr)) {
			if (mmu_map_pdpt(tptr) == NULL) {
				va_next = (addr + NBPML4) & ~PML4MASK;
				if (va_next < addr) /* Overflow */
					va_next = eva;
				continue;
			}

			if (mmu_map_pdt(tptr) == NULL) {
				va_next = (addr + NBPDP) & ~PDPMASK;
				if (va_next < addr) /* Overflow */
					va_next = eva;
				continue;
			}


			if (mmu_map_pt(tptr) == NULL) {
				va_next = (addr + NBPDR) & ~PDRMASK;
				if (va_next < addr)
					va_next = eva;
				continue;
			}

			panic("%s: All backing tables non-NULL,"
			      "yet hierarchy can't be inspected at va = 0x%lx\n", 
			      __func__, addr);
		}

		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next < addr)
			va_next = eva;
	
		pte = mmu_map_pt(tptr) + pt_index(addr);

		while (addr < va_next) {
			pt_entry_t ptetemp;
			ptetemp = *pte;

			if ((ptetemp & PG_V) == 0) {
				goto nextpte; /* continue */
			}

			if ((prot & VM_PROT_WRITE) == 0) {
				if ((ptetemp & (PG_MANAGED | PG_M | PG_RW)) ==
				    (PG_MANAGED | PG_M | PG_RW)) {
					vm_page_t m = MACH_TO_VM_PAGE(ptetemp & PG_FRAME);
					vm_page_dirty(m);
				}
				ptetemp &= ~(PG_RW | PG_M);
			}

			if ((prot & VM_PROT_EXECUTE) == 0)
				ptetemp |= pg_nx;

			if (ptetemp != *pte) {
				PT_SET_VA_MA(pte, ptetemp, true);
			}

		nextpte:
			addr += PAGE_SIZE;
			pte++;
		}
	}

	mmu_map_t_fini(tptr);

	PMAP_UNLOCK(pmap);

}

void
pmap_invalidate_all(pmap_t pmap)
{

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		invltlb();
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{

	vm_offset_t addr;
	/* XXX: TODO SMP */
	sched_pin();

	for (addr = sva; addr < eva; addr += PAGE_SIZE)
		invlpg(addr);

	sched_unpin();
}

void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	pmap_invalidate_range(pmap, va, va + PAGE_SIZE);
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	/* 
	 * Nothing to do - page table backing pages are the only pages
	 * that are implicitly "wired". These are managed by uma(9).
	 */
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
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t va_next;

	if (dst_addr != src_addr)
		return;

	if (dst_pmap->pm_type != src_pmap->pm_type)
		return;

	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}


	KASSERT(tsz != 0, ("tsz != 0"));

	char stbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	char dtbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t stptr = stbuf;
	mmu_map_t dtptr = dtbuf;

	struct mmu_map_mbackend mb = {
		ptmb_mappedalloc,
		ptmb_mappedfree,
		ptmb_ptov,
		ptmb_vtop
	};
	mmu_map_t_init(stptr, &mb);
	mmu_map_t_init(dtptr, &mb);

	for (addr = src_addr; addr < end_addr; addr = va_next) {
		pt_entry_t *src_pte, *dst_pte;

		KASSERT(addr < UPT_MIN_ADDRESS,
		    ("pmap_copy: invalid to pmap_copy page tables"));

		if (!mmu_map_inspect_va(src_pmap, stptr, addr)) {
			if (mmu_map_pdpt(stptr) == NULL) {
				va_next = (addr + NBPML4) & ~PML4MASK;
				if (va_next < addr) /* Overflow */
					va_next = end_addr;
				continue;
			}

			if (mmu_map_pdt(stptr) == NULL) {
				va_next = (addr + NBPDP) & ~PDPMASK;
				if (va_next < addr) /* Overflow */
					va_next = end_addr;
				continue;
			}


			if (mmu_map_pt(stptr) == NULL) {
				va_next = (addr + NBPDR) & ~PDRMASK;
				if (va_next < addr)
					va_next = end_addr;
				continue;
			}

			panic("%s: All backing tables non-NULL,"
			      "yet hierarchy can't be inspected at va = 0x%lx\n", 
			      __func__, addr);
		}

		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next < addr)
			va_next = end_addr;
	
		src_pte = mmu_map_pt(stptr) + pt_index(addr);

		while (addr < va_next) {
			pt_entry_t ptetemp;
			ptetemp = *src_pte;
			/*
			 * we only virtual copy managed pages
			 */
			if ((ptetemp & PG_MANAGED) != 0) {
				if (!mmu_map_hold_va(dst_pmap, dtptr, addr)) {
					goto out;
				}

				dst_pte = mmu_map_pt(dtptr) + pt_index(addr);
				if ((*dst_pte & PG_V) == 0 &&
 				    pmap_put_pv_entry(dst_pmap, addr,
					MACH_TO_VM_PAGE(ptetemp & PG_FRAME))) {
					/*
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					PT_SET_VA_MA(dst_pte, ptetemp & 
						     ~(PG_W | PG_M | PG_A), true);
					pmap_resident_count_inc(dst_pmap, 1);
	 			} else {
					goto out;
				}
			}
			addr += PAGE_SIZE;
			src_pte++;
		}
	}
out:
	mmu_map_t_fini(dtptr);
	mmu_map_t_fini(stptr);

	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
}

void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t ma_src, ma_dst;
	vm_offset_t va_src, va_dst;

	KASSERT(msrc != NULL && mdst != NULL,
		("Invalid source or destination page!"));

	va_src = kva_alloc(PAGE_SIZE * 2);
	va_dst = va_src + PAGE_SIZE;

	KASSERT(va_src != 0,
		("Out of kernel virtual space!"));

	ma_src = VM_PAGE_TO_MACH(msrc);
	ma_dst = VM_PAGE_TO_MACH(mdst);

	pmap_kenter_ma(va_src, ma_src);
	pmap_kenter_ma(va_dst, ma_dst);

	pagecopy((void *)va_src, (void *)va_dst);

	pmap_kremove(va_src);
	pmap_kremove(va_dst);

	kva_free(va_src, PAGE_SIZE * 2);
}

int unmapped_buf_allowed = 1;

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
	void *a_cp, *b_cp;
	vm_offset_t a_pg, b_pg;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;

	a_pg = kva_alloc(PAGE_SIZE * 2);
	b_pg = a_pg + PAGE_SIZE;

	KASSERT(a_pg != 0,
		("Out of kernel virtual space!"));

	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		pmap_kenter_ma(a_pg, 
			VM_PAGE_TO_MACH(ma[a_offset >> PAGE_SHIFT]));
		a_cp = (char *)a_pg + a_pg_offset;

		b_pg_offset = b_offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		pmap_kenter_ma(b_pg,
			VM_PAGE_TO_MACH(mb[b_offset >> PAGE_SHIFT]));
		b_cp = (char *)b_pg + b_pg_offset;
		bcopy(a_cp, b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}

	pmap_kremove(a_pg);
	pmap_kremove(b_pg);

	kva_free(a_pg, PAGE_SIZE * 2);
}

void
pmap_zero_page(vm_page_t m)
{
	pmap_kenter(zerova, VM_PAGE_TO_PHYS(m));
	pagezero((void *)zerova);
	pmap_kremove(zerova);
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
	if (off == 0 && size == PAGE_SIZE)
		pmap_zero_page(m);
	else {
		pmap_kenter(zerova, VM_PAGE_TO_PHYS(m));
		bzero((char *)zerova + off, size);
		pmap_kremove(zerova);
	}
}

void
pmap_zero_page_idle(vm_page_t m)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

void
pmap_activate(struct thread *td)
{
	pmap_t	pmap, oldpmap;
	u_int	cpuid;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
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
	td->td_pcb->pcb_cr3 = pmap->pm_cr3;
	if (__predict_false(pmap == kernel_pmap)) {
		load_cr3(pmap->pm_cr3);
	}
	else {
		pmap_xen_userload(pmap);
	}

	PCPU_SET(curpmap, pmap);
	critical_exit();
}

/* This is subtly different from pv_remove(). We don't defer removal
 * of pv entries until the end.
 * XXX: the pmap_pv.[ch] needs review for performance and use cases.
 */
static bool
pv_map_remove(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pt_entry_t *pte, tpte;

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	pte = pmap_vtopte_inspect(pmap, va, &tptr);

	KASSERT(pte != NULL, ("pte has no backing page tables!"));

	tpte = *pte;

	if ((tpte & PG_V) == 0) {
		panic("bad pte va %lx pte %lx", va, tpte);
	}

	/*
	 * We cannot remove wired pages from a process' mapping at this time
	 */
	if (tpte & PG_W) {
		return false; /* Continue iteration */
	}

	if (m == NULL) {
		m = MACH_TO_VM_PAGE(tpte & PG_FRAME);
	}

	PT_CLEAR_VA(pte, TRUE);

	/*
	 * Update the vm_page_t clean/reference bits.
	 */
	if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
		vm_page_dirty(m);
	}

	/* XXX: Tell mmu_xxx about backing page */
	pmap_vtopte_release(pmap, va, &tptr);

	pmap_resident_count_dec(pmap, 1);

	if (!pmap_free_pv_entry(pmap, va, m)) { /* XXX: This is very slow */
		panic("%s: pv 0x%lx: 0x%lx, unknown on managed page!", 
		      __func__, VM_PAGE_TO_PHYS(m), va);
	}

	return false;
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
	KASSERT(pmap != kernel_pmap, 
		("Trying to destroy kernel_pmap pv mappings!"));
	if (pmap != PCPU_GET(curpmap)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
	PMAP_LOCK(pmap);

	pmap_pv_iterate_map(pmap, pv_map_remove);

	pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

static bool
pv_dummy(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	return true; /* stop at the first iteration */
}

boolean_t
pmap_page_is_mapped(vm_page_t m)
{

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);

	return pmap_pv_iterate(m, pv_dummy, PV_RO_ITERATE);
}

boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return 0;
}

static bool
pv_page_is_wired(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pt_entry_t *pte, tpte;

	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	pte = pmap_vtopte_inspect(pmap, va, &tptr);

	KASSERT(pte != NULL, ("pte has no backing page tables!"));

	tpte = *pte;

	if ((tpte & PG_V) == 0) {
		panic("bad pte va %lx pte %lx", va, tpte);
	}

	/*
	 * We cannot remove wired pages from a process' mapping at this time
	 */
	if (tpte & PG_W) {
		return false; /* Continue iteration */
	}

	pmap_vtopte_release(pmap, va, &tptr);

	return true; /* stop iteration */
}

int
pmap_page_wired_mappings(vm_page_t m)
{
	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (0);

	return pmap_pv_iterate(m, pv_page_is_wired, PV_RW_ITERATE);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return 0;
}

boolean_t
pmap_is_referenced(vm_page_t m)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return 0;
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */

/* 
 * XXX: I've just duplicated what native does here. I *think*, with
 * mmu_map.[ch] (which native doesn't have), addr is always
 * prefaultable. Research this.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	boolean_t prefaultable = false;

	KASSERT(tsz != 0, ("tsz != 0"));

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

	PMAP_LOCK(pmap);
	prefaultable = mmu_map_inspect_va(pmap, tptr, addr);
	PMAP_UNLOCK(pmap);

	mmu_map_t_fini(tptr);

	return prefaultable;
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	vm_offset_t addr;
	vm_offset_t va_next;
	vm_page_t m;
	boolean_t anychanged, pv_lists_locked;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;
	pv_lists_locked = FALSE;

	anychanged = FALSE;
	PMAP_LOCK(pmap);
	/* XXX: unify va range operations over ptes across functions */

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

	for (addr = sva; addr < eva; addr = va_next) {
		pt_entry_t *pte;

		if (!mmu_map_inspect_va(pmap, tptr, addr)) {
			if (mmu_map_pdpt(tptr) == NULL) {
				va_next = (addr + NBPML4) & ~PML4MASK;
				if (va_next < addr) /* Overflow */
					va_next = eva;
				continue;
			}

			if (mmu_map_pdt(tptr) == NULL) {
				va_next = (addr + NBPDP) & ~PDPMASK;
				if (va_next < addr) /* Overflow */
					va_next = eva;
				continue;
			}


			if (mmu_map_pt(tptr) == NULL) {
				va_next = (addr + NBPDR) & ~PDRMASK;
				if (va_next < addr)
					va_next = eva;
				continue;

			}
		}
		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next > eva)
			va_next = eva;

		for (pte = mmu_map_pt(tptr) + pt_index(sva); sva != va_next; pte++,
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
				/* XXX: This is not atomic */
				PT_SET_VA_MA(pte, *pte & ~(PG_M | PG_A), true);
			} else if ((*pte & PG_A) != 0)
				/* XXX: This is not atomic */
				PT_SET_VA_MA(pte, *pte & ~(PG_A), true);
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
	PMAP_UNLOCK(pmap);
	mmu_map_t_fini(tptr);
}

void
pmap_clear_modify(vm_page_t m)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

void *
pmap_mapbios(vm_paddr_t pa, vm_size_t size)
{

	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return NULL;
}

/* Callback to remove write access on given va and pmap */
static bool
pv_remove_write(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pt_entry_t oldpte, *pte;
	char tbuf[tsz]; /* Safe to do this on the stack since tsz is
			 * effectively const.
			 */

	mmu_map_t tptr = tbuf;

	KASSERT(m != NULL, ("%s: passed NULL page!", __func__));
	PMAP_LOCK(pmap);
	pte = pmap_vtopte_inspect(pmap, va, &tptr);

	KASSERT(pte != NULL, ("pte has no backing page tables!"));

	oldpte = *pte;
	if (oldpte & PG_RW) {
		PT_SET_MA(va, oldpte & ~(PG_RW | PG_M));
		if ((oldpte & PG_M) != 0)
			vm_page_dirty(m);
		pmap_invalidate_page(pmap, va);
	}
	pmap_vtopte_release(pmap, va, &tptr);
	PMAP_UNLOCK(pmap);

	return false; /* Iterate through every mapping */
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
	 * If the page is not VPO_BUSY, then PGA_WRITEABLE cannot be set by
	 * another thread while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no page table entries need updating.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
 		return;

	pmap_pv_iterate(m, pv_remove_write, PV_RW_ITERATE);
	vm_page_aflag_clear(m, PGA_WRITEABLE);
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 */

int
pmap_ts_referenced(vm_page_t m)
{
	/*
	 * XXX: we don't clear refs yet. We just return non-zero if at
	 * least one reference exists.
	 * This obeys the required semantics - but only just.
	 */
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));

	return pmap_pv_iterate(m, pv_dummy, PV_RO_ITERATE);
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
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
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

void
pmap_resume()
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{	
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return -1;
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
  	KASSERT(0, ("XXX: %s: TODO\n", __func__));
	return NULL;
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	KASSERT(0, ("XXX: %s: TODO\n", __func__));
}

int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
		KASSERT(0, ("XXX: %s: TODO\n", __func__));
		return -1;
}

static uintptr_t
xen_vm_ptov(vm_paddr_t pa)
{
	/* Assert for valid PA *after* the VM has been init-ed */
	KASSERT((gdtset == 1 || pa < physfree), ("Stray PA 0x%lx passed\n", pa));
	return PHYS_TO_DMAP(pa);
}

static vm_paddr_t
xen_vm_vtop(uintptr_t va)
{
	if (ISBOOTVA(va) && gdtset != 1) { /* 
					    * Boot time page
					    */
		return VTOP(va);
	}

	if (ISDMAPVA(va)) {
		return DMAP_TO_PHYS(va);
	}

	if (ISKERNELVA(va)) {
		return pmap_kextract(va);
	}

	panic("Unknown VA 0x%lxpassed to %s\n", va, __func__);

	return 0;
}

static uintptr_t
xen_pagezone_alloc(void)
{
	vm_page_t m = vm_page_alloc(NULL, 0, VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);

	return PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
}

static void
xen_pagezone_free(vm_offset_t va)
{

	vm_paddr_t pa;
	vm_page_t m;

	/* We only free va obtained from xen_pagezone_alloc() */
	KASSERT(!ISKERNELVA(va), ("Trying to free unknown va: 0x%lx ", va));

	if (ISBOOTVA(va)) {
		/* We don't manage this range */
		return;
	}

	if (ISDMAPVA(va)) {
		pa = DMAP_TO_PHYS(va);
	}
	else {
		panic("Unknown va: 0x%lx\n", va);
	}

	m = PHYS_TO_VM_PAGE(pa);

	KASSERT(m != NULL, ("Trying to free unknown page"));

	vm_page_unwire(m, 0);
	vm_page_free(m);
	return;
}

/*
 * Replace the custom mmu_alloc(), backed by vallocpages(), with an
 * uma backed allocator, as soon as it is possible.
 */ 
static void
setup_xen_pagezone(void *dummy __unused)
{
	ptmb_mappedalloc = xen_pagezone_alloc;
	ptmb_mappedfree = xen_pagezone_free;
	ptmb_vtop = xen_vm_vtop;
	ptmb_ptov = xen_vm_ptov;
}
SYSINIT(setup_xen_pagezone, SI_SUB_VM_CONF, SI_ORDER_ANY, setup_xen_pagezone,
    NULL);
