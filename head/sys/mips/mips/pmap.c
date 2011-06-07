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
 *	from: src/sys/i386/i386/pmap.c,v 1.250.2.8 2000/11/21 00:09:14 ps
 *	JNPR: pmap.c,v 1.11.2.1 2007/08/16 11:51:06 girish
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.	These pseudo-maps are
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/smp.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
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
#include <sys/pcpu.h>
#include <sys/sched.h>
#ifdef SMP
#include <sys/smp.h>
#endif

#include <machine/cache.h>
#include <machine/md_var.h>
#include <machine/tlb.h>

#undef PMAP_DEBUG

#ifndef PMAP_SHPGPERPROC
#define	PMAP_SHPGPERPROC 200
#endif

#if !defined(DIAGNOSTIC)
#define	PMAP_INLINE __inline
#else
#define	PMAP_INLINE
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_seg_index(v)	(((v) >> SEGSHIFT) & (NPDEPG - 1))
#define	pmap_pde_index(v)	(((v) >> PDRSHIFT) & (NPDEPG - 1))
#define	pmap_pte_index(v)	(((v) >> PAGE_SHIFT) & (NPTEPG - 1))
#define	pmap_pde_pindex(v)	((v) >> PDRSHIFT)

#ifdef __mips_n64
#define	NUPDE			(NPDEPG * NPDEPG)
#define	NUSERPGTBLS		(NUPDE + NPDEPG)
#else
#define	NUPDE			(NPDEPG)
#define	NUSERPGTBLS		(NUPDE)
#endif

#define	is_kernel_pmap(x)	((x) == kernel_pmap)

struct pmap kernel_pmap_store;
pd_entry_t *kernel_segmap;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

static int nkpt;
unsigned pmap_max_asid;		/* max ASID supported by the system */

#define	PMAP_ASID_RESERVED	0

vm_offset_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;

static void pmap_asid_alloc(pmap_t pmap);

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static struct vm_object pvzone_obj;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;

static PMAP_INLINE void free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t locked_pmap);
static void pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
    vm_offset_t va);
static __inline void pmap_changebit(vm_page_t m, int bit, boolean_t setem);
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte);
static int pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va);
static void pmap_remove_entry(struct pmap *pmap, vm_page_t m, vm_offset_t va);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_page_t mpte,
    vm_offset_t va, vm_page_t m);
static void pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte);
static void pmap_invalidate_all(pmap_t pmap);
static void pmap_invalidate_page(pmap_t pmap, vm_offset_t va);
static int _pmap_unwire_pte_hold(pmap_t pmap, vm_offset_t va, vm_page_t m);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags);
static vm_page_t _pmap_allocpte(pmap_t pmap, unsigned ptepindex, int flags);
static int pmap_unuse_pt(pmap_t, vm_offset_t, vm_page_t);
static pt_entry_t init_pte_prot(vm_offset_t va, vm_page_t m, vm_prot_t prot);

#ifdef SMP
static void pmap_invalidate_page_action(void *arg);
static void pmap_invalidate_all_action(void *arg);
static void pmap_update_page_action(void *arg);
#endif

#ifndef __mips_n64
/*
 * This structure is for high memory (memory above 512Meg in 32 bit) support.
 * The highmem area does not have a KSEG0 mapping, and we need a mechanism to
 * do temporary per-CPU mappings for pmap_zero_page, pmap_copy_page etc.
 *
 * At bootup, we reserve 2 virtual pages per CPU for mapping highmem pages. To 
 * access a highmem physical address on a CPU, we map the physical address to
 * the reserved virtual address for the CPU in the kernel pagetable.  This is 
 * done with interrupts disabled(although a spinlock and sched_pin would be 
 * sufficient).
 */
struct local_sysmaps {
	vm_offset_t	base;
	uint32_t	saved_intr;
	uint16_t	valid1, valid2;
};
static struct local_sysmaps sysmap_lmem[MAXCPU];

static __inline void
pmap_alloc_lmem_map(void)
{
	int i;

	for (i = 0; i < MAXCPU; i++) {
		sysmap_lmem[i].base = virtual_avail;
		virtual_avail += PAGE_SIZE * 2;
		sysmap_lmem[i].valid1 = sysmap_lmem[i].valid2 = 0;
	}
}

static __inline vm_offset_t
pmap_lmem_map1(vm_paddr_t phys)
{
	struct local_sysmaps *sysm;
	pt_entry_t *pte, npte;
	vm_offset_t va;
	uint32_t intr;
	int cpu;

	intr = intr_disable();
	cpu = PCPU_GET(cpuid);
	sysm = &sysmap_lmem[cpu];
	sysm->saved_intr = intr;
	va = sysm->base;
	npte = TLBLO_PA_TO_PFN(phys) |
	    PTE_D | PTE_V | PTE_G | PTE_W | PTE_C_CACHE;
	pte = pmap_pte(kernel_pmap, va);
	*pte = npte;
	sysm->valid1 = 1;
	return (va);
}

static __inline vm_offset_t
pmap_lmem_map2(vm_paddr_t phys1, vm_paddr_t phys2)
{
	struct local_sysmaps *sysm;
	pt_entry_t *pte, npte;
	vm_offset_t va1, va2;
	uint32_t intr;
	int cpu;

	intr = intr_disable();
	cpu = PCPU_GET(cpuid);
	sysm = &sysmap_lmem[cpu];
	sysm->saved_intr = intr;
	va1 = sysm->base;
	va2 = sysm->base + PAGE_SIZE;
	npte = TLBLO_PA_TO_PFN(phys1) |
	    PTE_D | PTE_V | PTE_G | PTE_W | PTE_C_CACHE;
	pte = pmap_pte(kernel_pmap, va1);
	*pte = npte;
	npte =  TLBLO_PA_TO_PFN(phys2) |
	    PTE_D | PTE_V | PTE_G | PTE_W | PTE_C_CACHE;
	pte = pmap_pte(kernel_pmap, va2);
	*pte = npte;
	sysm->valid1 = 1;
	sysm->valid2 = 1;
	return (va1);
}

static __inline void
pmap_lmem_unmap(void)
{
	struct local_sysmaps *sysm;
	pt_entry_t *pte;
	int cpu;

	cpu = PCPU_GET(cpuid);
	sysm = &sysmap_lmem[cpu];
	pte = pmap_pte(kernel_pmap, sysm->base);
	*pte = PTE_G;
	tlb_invalidate_address(kernel_pmap, sysm->base);
	sysm->valid1 = 0;
	if (sysm->valid2) {
		pte = pmap_pte(kernel_pmap, sysm->base + PAGE_SIZE);
		*pte = PTE_G;
		tlb_invalidate_address(kernel_pmap, sysm->base + PAGE_SIZE);
		sysm->valid2 = 0;
	}
	intr_restore(sysm->saved_intr);
}
#else  /* __mips_n64 */

static __inline void
pmap_alloc_lmem_map(void)
{
}

static __inline vm_offset_t
pmap_lmem_map1(vm_paddr_t phys)
{

	return (0);
}

static __inline vm_offset_t
pmap_lmem_map2(vm_paddr_t phys1, vm_paddr_t phys2)
{

	return (0);
}

static __inline vm_offset_t 
pmap_lmem_unmap(void)
{

	return (0);
}
#endif /* !__mips_n64 */

/*
 * Page table entry lookup routines.
 */
static __inline pd_entry_t *
pmap_segmap(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_segtab[pmap_seg_index(va)]);
}

#ifdef __mips_n64
static __inline pd_entry_t *
pmap_pdpe_to_pde(pd_entry_t *pdpe, vm_offset_t va)
{
	pd_entry_t *pde;

	pde = (pd_entry_t *)*pdpe;
	return (&pde[pmap_pde_index(va)]);
}

static __inline pd_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pdpe;

	pdpe = pmap_segmap(pmap, va);
	if (pdpe == NULL || *pdpe == NULL)
		return (NULL);

	return (pmap_pdpe_to_pde(pdpe, va));
}
#else
static __inline pd_entry_t *
pmap_pdpe_to_pde(pd_entry_t *pdpe, vm_offset_t va)
{

	return (pdpe);
}

static __inline 
pd_entry_t *pmap_pde(pmap_t pmap, vm_offset_t va)
{

	return (pmap_segmap(pmap, va));
}
#endif

static __inline pt_entry_t *
pmap_pde_to_pte(pd_entry_t *pde, vm_offset_t va)
{
	pt_entry_t *pte;

	pte = (pt_entry_t *)*pde;
	return (&pte[pmap_pte_index(va)]);
}

pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (pde == NULL || *pde == NULL)
		return (NULL);

	return (pmap_pde_to_pte(pde, va));
}

vm_offset_t
pmap_steal_memory(vm_size_t size)
{
	vm_paddr_t bank_size, pa;
	vm_offset_t va;

	size = round_page(size);
	bank_size = phys_avail[1] - phys_avail[0];
	while (size > bank_size) {
		int i;

		for (i = 0; phys_avail[i + 2]; i += 2) {
			phys_avail[i] = phys_avail[i + 2];
			phys_avail[i + 1] = phys_avail[i + 3];
		}
		phys_avail[i] = 0;
		phys_avail[i + 1] = 0;
		if (!phys_avail[0])
			panic("pmap_steal_memory: out of memory");
		bank_size = phys_avail[1] - phys_avail[0];
	}

	pa = phys_avail[0];
	phys_avail[0] += size;
	if (MIPS_DIRECT_MAPPABLE(pa) == 0)
		panic("Out of memory below 512Meg?");
	va = MIPS_PHYS_TO_DIRECT(pa);
	bzero((caddr_t)va, size);
	return (va);
}

/*
 * Bootstrap the system enough to run with virtual memory.  This
 * assumes that the phys_avail array has been initialized.
 */
static void 
pmap_create_kernel_pagetable(void)
{
	int i, j;
	vm_offset_t ptaddr;
	pt_entry_t *pte;
#ifdef __mips_n64
	pd_entry_t *pde;
	vm_offset_t pdaddr;
	int npt, npde;
#endif

	/*
	 * Allocate segment table for the kernel
	 */
	kernel_segmap = (pd_entry_t *)pmap_steal_memory(PAGE_SIZE);

	/*
	 * Allocate second level page tables for the kernel
	 */
#ifdef __mips_n64
	npde = howmany(NKPT, NPDEPG);
	pdaddr = pmap_steal_memory(PAGE_SIZE * npde);
#endif
	nkpt = NKPT;
	ptaddr = pmap_steal_memory(PAGE_SIZE * nkpt);

	/*
	 * The R[4-7]?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry. Thus invalid
	 * entrys must have the Global bit set so when Entry LO and Entry HI
	 * G bits are anded together they will produce a global bit to store
	 * in the tlb.
	 */
	for (i = 0, pte = (pt_entry_t *)ptaddr; i < (nkpt * NPTEPG); i++, pte++)
		*pte = PTE_G;

#ifdef __mips_n64
	for (i = 0,  npt = nkpt; npt > 0; i++) {
		kernel_segmap[i] = (pd_entry_t)(pdaddr + i * PAGE_SIZE);
		pde = (pd_entry_t *)kernel_segmap[i];

		for (j = 0; j < NPDEPG && npt > 0; j++, npt--)
			pde[j] = (pd_entry_t)(ptaddr + (i * NPDEPG + j) * PAGE_SIZE);
	}
#else
	for (i = 0, j = pmap_seg_index(VM_MIN_KERNEL_ADDRESS); i < nkpt; i++, j++)
		kernel_segmap[j] = (pd_entry_t)(ptaddr + (i * PAGE_SIZE));
#endif

	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_segtab = kernel_segmap;
	CPU_FILL(&kernel_pmap->pm_active);
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	kernel_pmap->pm_asid[0].asid = PMAP_ASID_RESERVED;
	kernel_pmap->pm_asid[0].gen = 0;
	kernel_vm_end += nkpt * NPTEPG * PAGE_SIZE;
}

void
pmap_bootstrap(void)
{
	int i;
	int need_local_mappings = 0; 

	/* Sort. */
again:
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		/*
		 * Keep the memory aligned on page boundary.
		 */
		phys_avail[i] = round_page(phys_avail[i]);
		phys_avail[i + 1] = trunc_page(phys_avail[i + 1]);

		if (i < 2)
			continue;
		if (phys_avail[i - 2] > phys_avail[i]) {
			vm_paddr_t ptemp[2];

			ptemp[0] = phys_avail[i + 0];
			ptemp[1] = phys_avail[i + 1];

			phys_avail[i + 0] = phys_avail[i - 2];
			phys_avail[i + 1] = phys_avail[i - 1];

			phys_avail[i - 2] = ptemp[0];
			phys_avail[i - 1] = ptemp[1];
			goto again;
		}
	}

       	/*
	 * In 32 bit, we may have memory which cannot be mapped directly.
	 * This memory will need temporary mapping before it can be
	 * accessed.
	 */
	if (!MIPS_DIRECT_MAPPABLE(phys_avail[i - 1] - 1))
		need_local_mappings = 1;

	/*
	 * Copy the phys_avail[] array before we start stealing memory from it.
	 */
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		physmem_desc[i] = phys_avail[i];
		physmem_desc[i + 1] = phys_avail[i + 1];
	}

	Maxmem = atop(phys_avail[i - 1]);

	if (bootverbose) {
		printf("Physical memory chunk(s):\n");
		for (i = 0; phys_avail[i + 1] != 0; i += 2) {
			vm_paddr_t size;

			size = phys_avail[i + 1] - phys_avail[i];
			printf("%#08jx - %#08jx, %ju bytes (%ju pages)\n",
			    (uintmax_t) phys_avail[i],
			    (uintmax_t) phys_avail[i + 1] - 1,
			    (uintmax_t) size, (uintmax_t) size / PAGE_SIZE);
		}
		printf("Maxmem is 0x%0jx\n", ptoa((uintmax_t)Maxmem));
	}
	/*
	 * Steal the message buffer from the beginning of memory.
	 */
	msgbufp = (struct msgbuf *)pmap_steal_memory(msgbufsize);
	msgbufinit(msgbufp, msgbufsize);

	/*
	 * Steal thread0 kstack.
	 */
	kstack0 = pmap_steal_memory(KSTACK_PAGES << PAGE_SHIFT);

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

#ifdef SMP
	/*
	 * Steal some virtual address space to map the pcpu area.
	 */
	virtual_avail = roundup2(virtual_avail, PAGE_SIZE * 2);
	pcpup = (struct pcpu *)virtual_avail;
	virtual_avail += PAGE_SIZE * 2;

	/*
	 * Initialize the wired TLB entry mapping the pcpu region for
	 * the BSP at 'pcpup'. Up until this point we were operating
	 * with the 'pcpup' for the BSP pointing to a virtual address
	 * in KSEG0 so there was no need for a TLB mapping.
	 */
	mips_pcpu_tlb_init(PCPU_ADDR(0));

	if (bootverbose)
		printf("pcpu is available at virtual address %p.\n", pcpup);
#endif

	if (need_local_mappings)
		pmap_alloc_lmem_map();
	pmap_create_kernel_pagetable();
	pmap_max_asid = VMNUM_PIDS;
	mips_wr_entryhi(0);
	mips_wr_pagemask(0);
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_list_count = 0;
	m->md.pv_flags = 0;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 *	pmap_init has been enhanced to support in a fairly consistant
 *	way, discontiguous physical memory.
 */
void
pmap_init(void)
{

	/*
	 * Initialize the address space (zone) for the pv entries.  Set a
	 * high water mark so that the system can recover from excessive
	 * numbers of pv entries.
	 */
	pvzone = uma_zcreate("PV ENTRY", sizeof(struct pv_entry), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM | UMA_ZONE_NOFREE);
	pv_entry_max = PMAP_SHPGPERPROC * maxproc + cnt.v_page_count;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	uma_zone_set_obj(pvzone, &pvzone_obj, pv_entry_max);
}

/***************************************************
 * Low level helper routines.....
 ***************************************************/

static __inline void
pmap_invalidate_all_local(pmap_t pmap)
{

	if (pmap == kernel_pmap) {
		tlb_invalidate_all();
		return;
	}
	sched_pin();
	if (CPU_OVERLAP(&pmap->pm_active, PCPU_PTR(cpumask))) {
		sched_unpin();
		tlb_invalidate_all_user(pmap);
	} else {
		sched_unpin();
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
	}
}

#ifdef SMP
static void
pmap_invalidate_all(pmap_t pmap)
{

	smp_rendezvous(0, pmap_invalidate_all_action, 0, pmap);
}

static void
pmap_invalidate_all_action(void *arg)
{

	pmap_invalidate_all_local((pmap_t)arg);
}
#else
static void
pmap_invalidate_all(pmap_t pmap)
{

	pmap_invalidate_all_local(pmap);
}
#endif

static __inline void
pmap_invalidate_page_local(pmap_t pmap, vm_offset_t va)
{

	if (is_kernel_pmap(pmap)) {
		tlb_invalidate_address(pmap, va);
		return;
	}
	sched_pin();
	if (pmap->pm_asid[PCPU_GET(cpuid)].gen != PCPU_GET(asid_generation)) {
		sched_unpin();
		return;
	} else if (!CPU_OVERLAP(&pmap->pm_active, PCPU_PTR(cpumask))) {
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
		sched_unpin();
		return;
	}
	sched_unpin();
	tlb_invalidate_address(pmap, va);
}

#ifdef SMP
struct pmap_invalidate_page_arg {
	pmap_t pmap;
	vm_offset_t va;
};

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	struct pmap_invalidate_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	smp_rendezvous(0, pmap_invalidate_page_action, 0, &arg);
}

static void
pmap_invalidate_page_action(void *arg)
{
	struct pmap_invalidate_page_arg *p = arg;

	pmap_invalidate_page_local(p->pmap, p->va);
}
#else
static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{

	pmap_invalidate_page_local(pmap, va);
}
#endif

static __inline void
pmap_update_page_local(pmap_t pmap, vm_offset_t va, pt_entry_t pte)
{

	if (is_kernel_pmap(pmap)) {
		tlb_update(pmap, va, pte);
		return;
	}
	sched_pin();
	if (pmap->pm_asid[PCPU_GET(cpuid)].gen != PCPU_GET(asid_generation)) {
		sched_unpin();
		return;
	} else if (!CPU_OVERLAP(&pmap->pm_active, PCPU_PTR(cpumask))) {
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
		sched_unpin();
		return;
	}
	sched_unpin();
	tlb_update(pmap, va, pte);
}

#ifdef SMP
struct pmap_update_page_arg {
	pmap_t pmap;
	vm_offset_t va;
	pt_entry_t pte;
};

static void
pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte)
{
	struct pmap_update_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	arg.pte = pte;
	smp_rendezvous(0, pmap_update_page_action, 0, &arg);
}

static void
pmap_update_page_action(void *arg)
{
	struct pmap_update_page_arg *p = arg;

	pmap_update_page_local(p->pmap, p->va, p->pte);
}
#else
static void
pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte)
{

	pmap_update_page_local(pmap, va, pte);
}
#endif

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *pte;
	vm_offset_t retval = 0;

	PMAP_LOCK(pmap);
	pte = pmap_pte(pmap, va);
	if (pte) {
		retval = TLBLO_PTE_TO_PA(*pte) | (va & PAGE_MASK);
	}
	PMAP_UNLOCK(pmap);
	return (retval);
}

/*
 *	Routine:	pmap_extract_and_hold
 *	Function:
 *		Atomically extract and hold the physical page
 *		with the given pmap and virtual address pair
 *		if that mapping permits the given protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	pt_entry_t pte;
	vm_page_t m;
	vm_paddr_t pa;

	m = NULL;
	pa = 0;
	PMAP_LOCK(pmap);
retry:
	pte = *pmap_pte(pmap, va);
	if (pte != 0 && pte_test(&pte, PTE_V) &&
	    (pte_test(&pte, PTE_D) || (prot & VM_PROT_WRITE) == 0)) {
		if (vm_page_pa_tryrelock(pmap, TLBLO_PTE_TO_PA(pte), &pa))
			goto retry;

		m = PHYS_TO_VM_PAGE(TLBLO_PTE_TO_PA(pte));
		vm_page_hold(m);
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * add a wired page to the kva
 */
void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int attr)
{
	pt_entry_t *pte;
	pt_entry_t opte, npte;

#ifdef PMAP_DEBUG
	printf("pmap_kenter:  va: %p -> pa: %p\n", (void *)va, (void *)pa);
#endif
	npte = TLBLO_PA_TO_PFN(pa) | PTE_D | PTE_V | PTE_G | PTE_W | attr;

	pte = pmap_pte(kernel_pmap, va);
	opte = *pte;
	*pte = npte;
	if (pte_test(&opte, PTE_V) && opte != npte)
		pmap_update_page(kernel_pmap, va, npte);
}

void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{

	KASSERT(is_cacheable_mem(pa),
		("pmap_kenter: memory at 0x%lx is not cacheable", (u_long)pa));

	pmap_kenter_attr(va, pa, PTE_C_CACHE);
}

/*
 * remove a page from the kernel pagetables
 */
 /* PMAP_INLINE */ void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	/*
	 * Write back all caches from the page being destroyed
	 */
	mips_dcache_wbinv_range_index(va, PAGE_SIZE);

	pte = pmap_pte(kernel_pmap, va);
	*pte = PTE_G;
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
 *
 *	Use XKPHYS for 64 bit, and KSEG0 where possible for 32 bit.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	vm_offset_t va, sva;

	if (MIPS_DIRECT_MAPPABLE(end - 1))
		return (MIPS_PHYS_TO_DIRECT(start));

	va = sva = *virt;
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
	vm_offset_t origva = va;

	for (i = 0; i < count; i++) {
		pmap_flush_pvcache(m[i]);
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
		va += PAGE_SIZE;
	}

	mips_dcache_wbinv_range_index(origva, PAGE_SIZE*count);
}

/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	/*
	 * No need to wb/inv caches here, 
	 *   pmap_kremove will do it for us
	 */

	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

/*  Revision 1.507
 *
 * Simplify the reference counting of page table pages.	 Specifically, use
 * the page table page's wired count rather than its hold count to contain
 * the reference count.
 */

/*
 * This routine unholds page table pages, and if the hold count
 * drops to zero, then it decrements the wire count.
 */
static PMAP_INLINE int
pmap_unwire_pte_hold(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	--m->wire_count;
	if (m->wire_count == 0)
		return (_pmap_unwire_pte_hold(pmap, va, m));
	else
		return (0);
}

static int
_pmap_unwire_pte_hold(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pd_entry_t *pde;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * unmap the page table page
	 */
#ifdef __mips_n64
	if (m->pindex < NUPDE)
		pde = pmap_pde(pmap, va);
	else
		pde = pmap_segmap(pmap, va);
#else
	pde = pmap_pde(pmap, va);
#endif
	*pde = 0;
	pmap->pm_stats.resident_count--;

#ifdef __mips_n64
	if (m->pindex < NUPDE) {
		pd_entry_t *pdp;
		vm_page_t pdpg;

		/*
		 * Recursively decrement next level pagetable refcount
		 */
		pdp = (pd_entry_t *)*pmap_segmap(pmap, va);
		pdpg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(pdp));
		pmap_unwire_pte_hold(pmap, va, pdpg);
	}
#endif
	if (pmap->pm_ptphint == m)
		pmap->pm_ptphint = NULL;

	/*
	 * If the page is finally unwired, simply free it.
	 */
	vm_page_free_zero(m);
	atomic_subtract_int(&cnt.v_wire_count, 1);
	return (1);
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, vm_page_t mpte)
{
	unsigned ptepindex;
	pd_entry_t pteva;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);

	if (mpte == NULL) {
		ptepindex = pmap_pde_pindex(va);
		if (pmap->pm_ptphint &&
		    (pmap->pm_ptphint->pindex == ptepindex)) {
			mpte = pmap->pm_ptphint;
		} else {
			pteva = *pmap_pde(pmap, va);
			mpte = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(pteva));
			pmap->pm_ptphint = mpte;
		}
	}
	return (pmap_unwire_pte_hold(pmap, va, mpte));
}

void
pmap_pinit0(pmap_t pmap)
{
	int i;

	PMAP_LOCK_INIT(pmap);
	pmap->pm_segtab = kernel_segmap;
	CPU_ZERO(&pmap->pm_active);
	pmap->pm_ptphint = NULL;
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asid[i].asid = PMAP_ASID_RESERVED;
		pmap->pm_asid[i].gen = 0;
	}
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

void
pmap_grow_direct_page_cache()
{

#ifdef __mips_n64
	vm_contig_grow_cache(3, 0, MIPS_XKPHYS_LARGEST_PHYS);
#else
	vm_contig_grow_cache(3, 0, MIPS_KSEG0_LARGEST_PHYS);
#endif
}

vm_page_t
pmap_alloc_direct_page(unsigned int index, int req)
{
	vm_page_t m;

	m = vm_page_alloc_freelist(VM_FREELIST_DIRECT, req);
	if (m == NULL)
		return (NULL);

	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	m->pindex = index;
	atomic_add_int(&cnt.v_wire_count, 1);
	m->wire_count = 1;
	return (m);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit(pmap_t pmap)
{
	vm_offset_t ptdva;
	vm_page_t ptdpg;
	int i;

	PMAP_LOCK_INIT(pmap);

	/*
	 * allocate the page directory page
	 */
	while ((ptdpg = pmap_alloc_direct_page(NUSERPGTBLS, VM_ALLOC_NORMAL)) == NULL)
	       pmap_grow_direct_page_cache();

	ptdva = MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(ptdpg));
	pmap->pm_segtab = (pd_entry_t *)ptdva;
	CPU_ZERO(&pmap->pm_active);
	pmap->pm_ptphint = NULL;
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asid[i].asid = PMAP_ASID_RESERVED;
		pmap->pm_asid[i].gen = 0;
	}
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

	return (1);
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap_t pmap, unsigned ptepindex, int flags)
{
	vm_offset_t pageva;
	vm_page_t m;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("_pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	/*
	 * Find or fabricate a new pagetable page
	 */
	if ((m = pmap_alloc_direct_page(ptepindex, VM_ALLOC_NORMAL)) == NULL) {
		if (flags & M_WAITOK) {
			PMAP_UNLOCK(pmap);
			vm_page_unlock_queues();
			pmap_grow_direct_page_cache();
			vm_page_lock_queues();
			PMAP_LOCK(pmap);
		}

		/*
		 * Indicate the need to retry.	While waiting, the page
		 * table page may have been allocated.
		 */
		return (NULL);
	}

	/*
	 * Map the pagetable page into the process address space, if it
	 * isn't already there.
	 */
	pageva = MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));

#ifdef __mips_n64
	if (ptepindex >= NUPDE) {
		pmap->pm_segtab[ptepindex - NUPDE] = (pd_entry_t)pageva;
	} else {
		pd_entry_t *pdep, *pde;
		int segindex = ptepindex >> (SEGSHIFT - PDRSHIFT);
		int pdeindex = ptepindex & (NPDEPG - 1);
		vm_page_t pg;
		
		pdep = &pmap->pm_segtab[segindex];
		if (*pdep == NULL) { 
			/* recurse for allocating page dir */
			if (_pmap_allocpte(pmap, NUPDE + segindex, 
			    flags) == NULL) {
				/* alloc failed, release current */
				--m->wire_count;
				atomic_subtract_int(&cnt.v_wire_count, 1);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			pg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(*pdep));
			pg->wire_count++;
		}
		/* Next level entry */
		pde = (pd_entry_t *)*pdep;
		pde[pdeindex] = (pd_entry_t)pageva;
		pmap->pm_ptphint = m;
	}
#else
	pmap->pm_segtab[ptepindex] = (pd_entry_t)pageva;
#endif
	pmap->pm_stats.resident_count++;

	/*
	 * Set the page table hint
	 */
	pmap->pm_ptphint = m;
	return (m);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags)
{
	unsigned ptepindex;
	pd_entry_t *pde;
	vm_page_t m;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_pde_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	pde = pmap_pde(pmap, va);

	/*
	 * If the page table page is mapped, we just increment the hold
	 * count, and activate it.
	 */
	if (pde != NULL && *pde != NULL) {
		/*
		 * In order to get the page table page, try the hint first.
		 */
		if (pmap->pm_ptphint &&
		    (pmap->pm_ptphint->pindex == ptepindex)) {
			m = pmap->pm_ptphint;
		} else {
			m = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(*pde));
			pmap->pm_ptphint = m;
		}
		m->wire_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		m = _pmap_allocpte(pmap, ptepindex, flags);
		if (m == NULL && (flags & M_WAITOK))
			goto retry;
	}
	return (m);
}


/***************************************************
* Pmap allocation/deallocation routines.
 ***************************************************/
/*
 *  Revision 1.397
 *  - Merged pmap_release and pmap_release_free_page.  When pmap_release is
 *    called only the page directory page(s) can be left in the pmap pte
 *    object, since all page table pages will have been freed by
 *    pmap_remove_pages and pmap_remove.  In addition, there can only be one
 *    reference to the pmap and the page directory is wired, so the page(s)
 *    can never be busy.  So all there is to do is clear the magic mappings
 *    from the page directory and free the page(s).
 */


/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	vm_offset_t ptdva;
	vm_page_t ptdpg;

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	ptdva = (vm_offset_t)pmap->pm_segtab;
	ptdpg = PHYS_TO_VM_PAGE(MIPS_DIRECT_TO_PHYS(ptdva));

	ptdpg->wire_count--;
	atomic_subtract_int(&cnt.v_wire_count, 1);
	vm_page_free_zero(ptdpg);
	PMAP_LOCK_DESTROY(pmap);
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_page_t nkpg;
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;
	int i;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
	addr = roundup2(addr, NBSEG);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		pdpe = pmap_segmap(kernel_pmap, kernel_vm_end);
#ifdef __mips_n64
		if (*pdpe == 0) {
			/* new intermediate page table entry */
			nkpg = pmap_alloc_direct_page(nkpt, VM_ALLOC_INTERRUPT);
			if (nkpg == NULL)
				panic("pmap_growkernel: no memory to grow kernel");
			*pdpe = (pd_entry_t)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(nkpg));
			continue; /* try again */
		}
#endif
		pde = pmap_pdpe_to_pde(pdpe, kernel_vm_end);
		if (*pde != 0) {
			kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
			continue;
		}

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = pmap_alloc_direct_page(nkpt, VM_ALLOC_INTERRUPT);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");
		nkpt++;
		*pde = (pd_entry_t)MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(nkpg));

		/*
		 * The R[4-7]?00 stores only one copy of the Global bit in
		 * the translation lookaside buffer for each 2 page entry.
		 * Thus invalid entrys must have the Global bit set so when
		 * Entry LO and Entry HI G bits are anded together they will
		 * produce a global bit to store in the tlb.
		 */
		pte = (pt_entry_t *)*pde;
		for (i = 0; i < NPTEPG; i++)
			pte[i] = PTE_G;

		kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
		if (kernel_vm_end - 1 >= kernel_map->max_offset) {
			kernel_vm_end = kernel_map->max_offset;
			break;
		}
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
	uma_zfree(pvzone, pv);
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static pv_entry_t
get_pv_entry(pmap_t locked_pmap)
{
	static const struct timeval printinterval = { 60, 0 };
	static struct timeval lastprint;
	struct vpgqueues *vpq;
	pt_entry_t *pte, oldpte;
	pmap_t pmap;
	pv_entry_t allocated_pv, next_pv, pv;
	vm_offset_t va;
	vm_page_t m;

	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	allocated_pv = uma_zalloc(pvzone, M_NOWAIT);
	if (allocated_pv != NULL) {
		pv_entry_count++;
		if (pv_entry_count > pv_entry_high_water)
			pagedaemon_wakeup();
		else
			return (allocated_pv);
	}
	/*
	 * Reclaim pv entries: At first, destroy mappings to inactive
	 * pages.  After that, if a pv entry is still needed, destroy
	 * mappings to active pages.
	 */
	if (ratecheck(&lastprint, &printinterval))
		printf("Approaching the limit on PV entries, "
		    "increase the vm.pmap.shpgperproc tunable.\n");
	vpq = &vm_page_queues[PQ_INACTIVE];
retry:
	TAILQ_FOREACH(m, &vpq->pl, pageq) {
		if (m->hold_count || m->busy)
			continue;
		TAILQ_FOREACH_SAFE(pv, &m->md.pv_list, pv_list, next_pv) {
			va = pv->pv_va;
			pmap = pv->pv_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap)
				PMAP_LOCK(pmap);
			else if (pmap != locked_pmap && !PMAP_TRYLOCK(pmap))
				continue;
			pmap->pm_stats.resident_count--;
			pte = pmap_pte(pmap, va);
			KASSERT(pte != NULL, ("pte"));
			oldpte = *pte;
			if (is_kernel_pmap(pmap))
				*pte = PTE_G;
			else
				*pte = 0;
			KASSERT(!pte_test(&oldpte, PTE_W),
			    ("wired pte for unwired page"));
			if (m->md.pv_flags & PV_TABLE_REF)
				vm_page_flag_set(m, PG_REFERENCED);
			if (pte_test(&oldpte, PTE_D))
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, va);
			TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
			m->md.pv_list_count--;
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
			pmap_unuse_pt(pmap, va, pv->pv_ptem);
			if (pmap != locked_pmap)
				PMAP_UNLOCK(pmap);
			if (allocated_pv == NULL)
				allocated_pv = pv;
			else
				free_pv_entry(pv);
		}
		if (TAILQ_EMPTY(&m->md.pv_list)) {
			vm_page_flag_clear(m, PG_WRITEABLE);
			m->md.pv_flags &= ~(PV_TABLE_REF | PV_TABLE_MOD);
		}
	}
	if (allocated_pv == NULL) {
		if (vpq == &vm_page_queues[PQ_INACTIVE]) {
			vpq = &vm_page_queues[PQ_ACTIVE];
			goto retry;
		}
		panic("get_pv_entry: increase the vm.pmap.shpgperproc tunable");
	}
	return (allocated_pv);
}

/*
 *  Revision 1.370
 *
 *  Move pmap_collect() out of the machine-dependent code, rename it
 *  to reflect its new location, and add page queue and flag locking.
 *
 *  Notes: (1) alpha, i386, and ia64 had identical implementations
 *  of pmap_collect() in terms of machine-independent interfaces;
 *  (2) sparc64 doesn't require it; (3) powerpc had it as a TODO.
 *
 *  MIPS implementation was identical to alpha [Junos 8.2]
 */

/*
 * If it is the first entry on the list, it is actually
 * in the header and we must copy the following entry up
 * to the header.  Otherwise we must search the list for
 * the entry.  In either case we free the now unused entry.
 */

static pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (pvh->pv_list_count < pmap->pm_stats.resident_count) {
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_list) {
			if (pmap == pv->pv_pmap && va == pv->pv_va)
				break;
		}
	} else {
		TAILQ_FOREACH(pv, &pmap->pm_pvlist, pv_plist) {
			if (va == pv->pv_va)
				break;
		}
	}
	if (pv != NULL) {
		TAILQ_REMOVE(&pvh->pv_list, pv, pv_list);
		pvh->pv_list_count--;
		TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
	}
	return (pv);
}

static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found, pa %lx va %lx",
	     (u_long)VM_PAGE_TO_PHYS(member2struct(vm_page, md, pvh)),
	     (u_long)va));
	free_pv_entry(pv);
}

static void
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	pmap_pvh_free(&m->md, pmap, va);
	if (TAILQ_EMPTY(&m->md.pv_list))
		vm_page_flag_clear(m, PG_WRITEABLE);
}

/*
 * Conditionally create a pv entry.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_page_t mpte, vm_offset_t va,
    vm_page_t m)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = uma_zalloc(pvzone, M_NOWAIT)) != NULL) {
		pv_entry_count++;
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		pv->pv_ptem = mpte;
		TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count++;
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va)
{
	pt_entry_t oldpte;
	vm_page_t m;
	vm_paddr_t pa;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	oldpte = *ptq;
	if (is_kernel_pmap(pmap))
		*ptq = PTE_G;
	else
		*ptq = 0;

	if (pte_test(&oldpte, PTE_W))
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	pa = TLBLO_PTE_TO_PA(oldpte);

	if (page_is_managed(pa)) {
		m = PHYS_TO_VM_PAGE(pa);
		if (pte_test(&oldpte, PTE_D)) {
			KASSERT(!pte_test(&oldpte, PTE_RO),
			    ("%s: modified page not writable: va: %p, pte: %#jx",
			    __func__, (void *)va, (uintmax_t)oldpte));
			vm_page_dirty(m);
		}
		if (m->md.pv_flags & PV_TABLE_REF)
			vm_page_flag_set(m, PG_REFERENCED);
		m->md.pv_flags &= ~(PV_TABLE_REF | PV_TABLE_MOD);

		pmap_remove_entry(pmap, m, va);
	}
	return (pmap_unuse_pt(pmap, va, NULL));
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(struct pmap *pmap, vm_offset_t va)
{
	pt_entry_t *ptq;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	ptq = pmap_pte(pmap, va);

	/*
	 * if there is no pte for this address, just skip it!!!
	 */
	if (!ptq || !pte_test(ptq, PTE_V)) {
		return;
	}

	/*
	 * Write back all caches from the page being destroyed
	 */
	mips_dcache_wbinv_range_index(va, PAGE_SIZE);

	/*
	 * get a local va for mappings for this pmap.
	 */
	(void)pmap_remove_pte(pmap, ptq, va);
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
pmap_remove(struct pmap *pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t va_next;
	pd_entry_t *pde, *pdpe;
	pt_entry_t *pte;

	if (pmap == NULL)
		return;

	if (pmap->pm_stats.resident_count == 0)
		return;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);

	/*
	 * special handling of removing one page.  a very common operation
	 * and easy to short circuit some code.
	 */
	if ((sva + PAGE_SIZE) == eva) {
		pmap_remove_page(pmap, sva);
		goto out;
	}
	for (; sva < eva; sva = va_next) {
		pdpe = pmap_segmap(pmap, sva);
#ifdef __mips_n64
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
#endif
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		if (*pde == 0)
			continue;
		if (va_next > eva)
			va_next = eva;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; 
		    pte++, sva += PAGE_SIZE) {
			pmap_remove_page(pmap, sva);
		}
	}
out:
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);
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
	pv_entry_t pv;
	pt_entry_t *pte, tpte;

	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_remove_all: page %p is fictitious", m));
	vm_page_lock_queues();

	if (m->md.pv_flags & PV_TABLE_REF)
		vm_page_flag_set(m, PG_REFERENCED);

	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		PMAP_LOCK(pv->pv_pmap);

		/*
		 * If it's last mapping writeback all caches from 
		 * the page being destroyed
	 	 */
		if (m->md.pv_list_count == 1) 
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);

		pv->pv_pmap->pm_stats.resident_count--;

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);

		tpte = *pte;
		if (is_kernel_pmap(pv->pv_pmap))
			*pte = PTE_G;
		else
			*pte = 0;

		if (pte_test(&tpte, PTE_W))
			pv->pv_pmap->pm_stats.wired_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (pte_test(&tpte, PTE_D)) {
			KASSERT(!pte_test(&tpte, PTE_RO),
			    ("%s: modified page not writable: va: %p, pte: %#jx",
			    __func__, (void *)pv->pv_va, (uintmax_t)tpte));
			vm_page_dirty(m);
		}
		pmap_invalidate_page(pv->pv_pmap, pv->pv_va);

		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		pmap_unuse_pt(pv->pv_pmap, pv->pv_va, pv->pv_ptem);
		PMAP_UNLOCK(pv->pv_pmap);
		free_pv_entry(pv);
	}

	vm_page_flag_clear(m, PG_WRITEABLE);
	m->md.pv_flags &= ~(PV_TABLE_REF | PV_TABLE_MOD);
	vm_page_unlock_queues();
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pt_entry_t *pte;
	pd_entry_t *pde, *pdpe;
	vm_offset_t va_next;

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pt_entry_t pbits;
		vm_page_t m;
		vm_paddr_t pa;

		pdpe = pmap_segmap(pmap, sva);
#ifdef __mips_n64
		if (*pdpe == 0) {
			va_next = (sva + NBSEG) & ~SEGMASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
#endif
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		if (pde == NULL || *pde == NULL)
			continue;
		if (va_next > eva)
			va_next = eva;

		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		     sva += PAGE_SIZE) {

			/* Skip invalid PTEs */
			if (!pte_test(pte, PTE_V))
				continue;
			pbits = *pte;
			pa = TLBLO_PTE_TO_PA(pbits);
			if (page_is_managed(pa) && pte_test(&pbits, PTE_D)) {
				m = PHYS_TO_VM_PAGE(pa);
				vm_page_dirty(m);
				m->md.pv_flags &= ~PV_TABLE_MOD;
			}
			pte_clear(&pbits, PTE_D);
			pte_set(&pbits, PTE_RO);
			
			if (pbits != *pte) {
				*pte = pbits;
				pmap_update_page(pmap, sva, pbits);
			}
		}
	}
	vm_page_unlock_queues();
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
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	vm_paddr_t pa, opa;
	pt_entry_t *pte;
	pt_entry_t origpte, newpte;
	pv_entry_t pv;
	vm_page_t mpte, om;
	pt_entry_t rw = 0;

	if (pmap == NULL)
		return;

	va &= ~PAGE_MASK;
 	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0 ||
	    (m->oflags & VPO_BUSY) != 0,
	    ("pmap_enter: page %p is not busy", m));

	mpte = NULL;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);

	/*
	 * In the case that a page table page is not resident, we are
	 * creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_allocpte(pmap, va, M_WAITOK);
	}
	pte = pmap_pte(pmap, va);

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL) {
		panic("pmap_enter: invalid page directory, pdir=%p, va=%p",
		    (void *)pmap->pm_segtab, (void *)va);
	}
	pa = VM_PAGE_TO_PHYS(m);
	om = NULL;
	origpte = *pte;
	opa = TLBLO_PTE_TO_PA(origpte);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (pte_test(&origpte, PTE_V) && opa == pa) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is
		 * wired, the PT page will be also.
		 */
		if (wired && !pte_test(&origpte, PTE_W))
			pmap->pm_stats.wired_count++;
		else if (!wired && pte_test(&origpte, PTE_W))
			pmap->pm_stats.wired_count--;

		KASSERT(!pte_test(&origpte, PTE_D | PTE_RO),
		    ("%s: modified page not writable: va: %p, pte: %#jx",
		    __func__, (void *)va, (uintmax_t)origpte));

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->wire_count--;

		if (page_is_managed(opa)) {
			om = m;
		}
		goto validate;
	}

	pv = NULL;

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		if (pte_test(&origpte, PTE_W))
			pmap->pm_stats.wired_count--;

		if (page_is_managed(opa)) {
			om = PHYS_TO_VM_PAGE(opa);
			pv = pmap_pvh_remove(&om->md, pmap, va);
		}
		if (mpte != NULL) {
			mpte->wire_count--;
			KASSERT(mpte->wire_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			    " va: %p", (void *)va));
		}
	} else
		pmap->pm_stats.resident_count++;

	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
		KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva,
		    ("pmap_enter: managed mapping within the clean submap"));
		if (pv == NULL)
			pv = get_pv_entry(pmap);
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		pv->pv_ptem = mpte;
		TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count++;
	} else if (pv != NULL)
		free_pv_entry(pv);

	/*
	 * Increment counters
	 */
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	if ((access & VM_PROT_WRITE) != 0)
		m->md.pv_flags |= PV_TABLE_MOD | PV_TABLE_REF;
	rw = init_pte_prot(va, m, prot);

#ifdef PMAP_DEBUG
	printf("pmap_enter:  va: %p -> pa: %p\n", (void *)va, (void *)pa);
#endif
	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	newpte = TLBLO_PA_TO_PFN(pa) | rw | PTE_V;

	if (is_cacheable_mem(pa))
		newpte |= PTE_C_CACHE;
	else
		newpte |= PTE_C_UNCACHED;

	if (wired)
		newpte |= PTE_W;

	if (is_kernel_pmap(pmap))
	         newpte |= PTE_G;

	/*
	 * if the mapping or permission bits are different, we need to
	 * update the pte.
	 */
	if (origpte != newpte) {
		if (pte_test(&origpte, PTE_V)) {
			*pte = newpte;
			if (page_is_managed(opa) && (opa != pa)) {
				if (om->md.pv_flags & PV_TABLE_REF)
					vm_page_flag_set(om, PG_REFERENCED);
				om->md.pv_flags &=
				    ~(PV_TABLE_REF | PV_TABLE_MOD);
			}
			if (pte_test(&origpte, PTE_D)) {
				KASSERT(!pte_test(&origpte, PTE_RO),
				    ("pmap_enter: modified page not writable:"
				    " va: %p, pte: %#jx", (void *)va, (uintmax_t)origpte));
				if (page_is_managed(opa))
					vm_page_dirty(om);
			}
			if (page_is_managed(opa) &&
			    TAILQ_EMPTY(&om->md.pv_list))
				vm_page_flag_clear(om, PG_WRITEABLE);
		} else {
			*pte = newpte;
		}
	}
	pmap_update_page(pmap, va, newpte);

	/*
	 * Sync I & D caches for executable pages.  Do this only if the
	 * target pmap belongs to the current process.  Otherwise, an
	 * unresolvable TLB miss may occur.
	 */
	if (!is_kernel_pmap(pmap) && (pmap == &curproc->p_vmspace->vm_pmap) &&
	    (prot & VM_PROT_EXECUTE)) {
		mips_icache_sync_range(va, PAGE_SIZE);
		mips_dcache_wbinv_range(va, PAGE_SIZE);
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);
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

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(pmap, va, m, prot, NULL);
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);
}

static vm_page_t
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte)
{
	pt_entry_t *pte;
	vm_paddr_t pa;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * In the case that a page table page is not resident, we are
	 * creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		pd_entry_t *pde;
		unsigned ptepindex;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = pmap_pde_pindex(va);
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->wire_count++;
		} else {
			/*
			 * Get the page directory entry
			 */
			pde = pmap_pde(pmap, va);

			/*
			 * If the page table page is mapped, we just
			 * increment the hold count, and activate it.
			 */
			if (pde && *pde != 0) {
				if (pmap->pm_ptphint &&
				    (pmap->pm_ptphint->pindex == ptepindex)) {
					mpte = pmap->pm_ptphint;
				} else {
					mpte = PHYS_TO_VM_PAGE(
						MIPS_DIRECT_TO_PHYS(*pde));
					pmap->pm_ptphint = mpte;
				}
				mpte->wire_count++;
			} else {
				mpte = _pmap_allocpte(pmap, ptepindex,
				    M_NOWAIT);
				if (mpte == NULL)
					return (mpte);
			}
		}
	} else {
		mpte = NULL;
	}

	pte = pmap_pte(pmap, va);
	if (pte_test(pte, PTE_V)) {
		if (mpte != NULL) {
			mpte->wire_count--;
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0 &&
	    !pmap_try_insert_pv_entry(pmap, mpte, va, m)) {
		if (mpte != NULL) {
			pmap_unwire_pte_hold(pmap, va, mpte);
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * Now validate mapping with RO protection
	 */
	*pte = TLBLO_PA_TO_PFN(pa) | PTE_V;

	if (is_cacheable_mem(pa))
		*pte |= PTE_C_CACHE;
	else
		*pte |= PTE_C_UNCACHED;

	if (is_kernel_pmap(pmap))
		*pte |= PTE_G;
	else {
		*pte |= PTE_RO;
		/*
		 * Sync I & D caches.  Do this only if the target pmap
		 * belongs to the current process.  Otherwise, an
		 * unresolvable TLB miss may occur. */
		if (pmap == &curproc->p_vmspace->vm_pmap) {
			va &= ~PAGE_MASK;
			mips_icache_sync_range(va, PAGE_SIZE);
			mips_dcache_wbinv_range(va, PAGE_SIZE);
		}
	}
	return (mpte);
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 *
 * Use XKPHYS for 64 bit, and KSEG0 where possible for 32 bit.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	vm_offset_t va;

	if (i != 0)
		printf("%s: ERROR!!! More than one page of virtual address mapping not supported\n",
		    __func__);

	if (MIPS_DIRECT_MAPPABLE(pa)) {
		va = MIPS_PHYS_TO_DIRECT(pa);
	} else {
#ifndef __mips_n64    /* XXX : to be converted to new style */
		int cpu;
		register_t intr;
		struct local_sysmaps *sysm;
		pt_entry_t *pte, npte;

		/* If this is used other than for dumps, we may need to leave
		 * interrupts disasbled on return. If crash dumps don't work when
		 * we get to this point, we might want to consider this (leaving things
		 * disabled as a starting point ;-)
	 	 */
		intr = intr_disable();
		cpu = PCPU_GET(cpuid);
		sysm = &sysmap_lmem[cpu];
		/* Since this is for the debugger, no locks or any other fun */
		npte = TLBLO_PA_TO_PFN(pa) | PTE_D | PTE_V | PTE_G | PTE_W | PTE_C_CACHE;
		pte = pmap_pte(kernel_pmap, sysm->base);
		*pte = npte;
		sysm->valid1 = 1;
		pmap_update_page(kernel_pmap, sysm->base, npte);
		va = sysm->base;
		intr_restore(intr);
#endif
	}
	return ((void *)va);
}

void
pmap_kenter_temporary_free(vm_paddr_t pa)
{
#ifndef __mips_n64    /* XXX : to be converted to new style */
	int cpu;
	register_t intr;
	struct local_sysmaps *sysm;
#endif

	if (MIPS_DIRECT_MAPPABLE(pa)) {
		/* nothing to do for this case */
		return;
	}
#ifndef __mips_n64    /* XXX : to be converted to new style */
	cpu = PCPU_GET(cpuid);
	sysm = &sysmap_lmem[cpu];
	if (sysm->valid1) {
		pt_entry_t *pte;

		intr = intr_disable();
		pte = pmap_pte(kernel_pmap, sysm->base);
		*pte = PTE_G;
		pmap_invalidate_page(kernel_pmap, sysm->base);
		intr_restore(intr);
		sysm->valid1 = 0;
	}
#endif
}

/*
 * Moved the code to Machine Independent
 *	 vm_map_pmap_enter()
 */

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
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;

	VM_OBJECT_LOCK_ASSERT(m_start->object, MA_OWNED);
	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		mpte = pmap_enter_quick_locked(pmap, start + ptoa(diff), m,
		    prot, mpte);
		m = TAILQ_NEXT(m, listq);
	}
	vm_page_unlock_queues();
 	PMAP_UNLOCK(pmap);
}

/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size)
{
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	pt_entry_t *pte;

	if (pmap == NULL)
		return;

	PMAP_LOCK(pmap);
	pte = pmap_pte(pmap, va);

	if (wired && !pte_test(pte, PTE_W))
		pmap->pm_stats.wired_count++;
	else if (!wired && pte_test(pte, PTE_W))
		pmap->pm_stats.wired_count--;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	if (wired)
		pte_set(pte, PTE_W);
	else
		pte_clear(pte, PTE_W);
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
 *	pmap_zero_page zeros the specified hardware page by mapping
 *	the page into KVM and using bzero to clear its contents.
 *
 * 	Use XKPHYS for 64 bit, and KSEG0 where possible for 32 bit.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	if (MIPS_DIRECT_MAPPABLE(phys)) {
		va = MIPS_PHYS_TO_DIRECT(phys);
		bzero((caddr_t)va, PAGE_SIZE);
		mips_dcache_wbinv_range(va, PAGE_SIZE);
	} else {
		va = pmap_lmem_map1(phys);
		bzero((caddr_t)va, PAGE_SIZE);
		mips_dcache_wbinv_range(va, PAGE_SIZE);
		pmap_lmem_unmap();
	}
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
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	if (MIPS_DIRECT_MAPPABLE(phys)) {
		va = MIPS_PHYS_TO_DIRECT(phys);
		bzero((char *)(caddr_t)va + off, size);
		mips_dcache_wbinv_range(va + off, size);
	} else {
		va = pmap_lmem_map1(phys);
		bzero((char *)va + off, size);
		mips_dcache_wbinv_range(va + off, size);
		pmap_lmem_unmap();
	}
}

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

	if (MIPS_DIRECT_MAPPABLE(phys)) {
		va = MIPS_PHYS_TO_DIRECT(phys);
		bzero((caddr_t)va, PAGE_SIZE);
		mips_dcache_wbinv_range(va, PAGE_SIZE);
	} else {
		va = pmap_lmem_map1(phys);
		bzero((caddr_t)va, PAGE_SIZE);
		mips_dcache_wbinv_range(va, PAGE_SIZE);
		pmap_lmem_unmap();
	}
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 *
 * 	Use XKPHYS for 64 bit, and KSEG0 where possible for 32 bit.
 */
void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	vm_offset_t va_src, va_dst;
	vm_paddr_t phys_src = VM_PAGE_TO_PHYS(src);
	vm_paddr_t phys_dst = VM_PAGE_TO_PHYS(dst);

	if (MIPS_DIRECT_MAPPABLE(phys_src) && MIPS_DIRECT_MAPPABLE(phys_dst)) {
		/* easy case, all can be accessed via KSEG0 */
		/*
		 * Flush all caches for VA that are mapped to this page
		 * to make sure that data in SDRAM is up to date
		 */
		pmap_flush_pvcache(src);
		mips_dcache_wbinv_range_index(
		    MIPS_PHYS_TO_DIRECT(phys_dst), PAGE_SIZE);
		va_src = MIPS_PHYS_TO_DIRECT(phys_src);
		va_dst = MIPS_PHYS_TO_DIRECT(phys_dst);
		bcopy((caddr_t)va_src, (caddr_t)va_dst, PAGE_SIZE);
		mips_dcache_wbinv_range(va_dst, PAGE_SIZE);
	} else {
		va_src = pmap_lmem_map2(phys_src, phys_dst);
		va_dst = va_src + PAGE_SIZE;
		bcopy((void *)va_src, (void *)va_dst, PAGE_SIZE);
		mips_dcache_wbinv_range(va_dst, PAGE_SIZE);
		pmap_lmem_unmap();
	}
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	pv_entry_t pv;
	int loops = 0;
	boolean_t rv;

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	rv = FALSE;
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pmap) {
			rv = TRUE;
			break;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	vm_page_unlock_queues();
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
	pt_entry_t *pte, tpte;
	pv_entry_t pv, npv;
	vm_page_t m;

	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	for (pv = TAILQ_FIRST(&pmap->pm_pvlist); pv != NULL; pv = npv) {

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		if (!pte_test(pte, PTE_V))
			panic("pmap_remove_pages: page on pm_pvlist has no pte");
		tpte = *pte;

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (pte_test(&tpte, PTE_W)) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}
		*pte = is_kernel_pmap(pmap) ? PTE_G : 0;

		m = PHYS_TO_VM_PAGE(TLBLO_PTE_TO_PA(tpte));
		KASSERT(m != NULL,
		    ("pmap_remove_pages: bad tpte %#jx", (uintmax_t)tpte));

		pv->pv_pmap->pm_stats.resident_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (pte_test(&tpte, PTE_D)) {
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
	pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	vm_page_unlock_queues();
}

/*
 * pmap_testbit tests bits in pte's
 * note that the testbit/changebit routines are inline,
 * and a lot of things compile-time evaluate.
 */
static boolean_t
pmap_testbit(vm_page_t m, int bit)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	boolean_t rv = FALSE;

	if (m->flags & PG_FICTITIOUS)
		return (rv);

	if (TAILQ_FIRST(&m->md.pv_list) == NULL)
		return (rv);

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		rv = pte_test(pte, bit);
		PMAP_UNLOCK(pv->pv_pmap);
		if (rv)
			break;
	}
	return (rv);
}

/*
 * this routine is used to clear dirty bits in ptes
 */
static __inline void
pmap_changebit(vm_page_t m, int bit, boolean_t setem)
{
	pv_entry_t pv;
	pt_entry_t *pte;

	if (m->flags & PG_FICTITIOUS)
		return;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		if (setem) {
			*pte |= bit;
			pmap_update_page(pv->pv_pmap, pv->pv_va, *pte);
		} else {
			pt_entry_t pbits = *pte;

			if (pbits & bit) {
				if (bit == PTE_D) {
					if (pbits & PTE_D)
						vm_page_dirty(m);
					*pte = (pbits & ~PTE_D) | PTE_RO;
				} else {
					*pte = pbits & ~bit;
				}
				pmap_update_page(pv->pv_pmap, pv->pv_va, *pte);
			}
		}
		PMAP_UNLOCK(pv->pv_pmap);
	}
	if (!setem && bit == PTE_D)
		vm_page_flag_clear(m, PG_WRITEABLE);
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
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *pte;
	int count;

	count = 0;
	if ((m->flags & PG_FICTITIOUS) != 0)
		return (count);
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap = pv->pv_pmap;
		PMAP_LOCK(pmap);
		pte = pmap_pte(pmap, pv->pv_va);
		if (pte_test(pte, PTE_W))
			count++;
		PMAP_UNLOCK(pmap);
	}
	vm_page_unlock_queues();
	return (count);
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	pv_entry_t pv, npv;
	vm_offset_t va;
	pt_entry_t *pte;

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	/*
	 * If the page is not VPO_BUSY, then PG_WRITEABLE cannot be set by
	 * another thread while the object is locked.  Thus, if PG_WRITEABLE
	 * is clear, no page table entries need updating.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->flags & PG_WRITEABLE) == 0)
		return;

	/*
	 * Loop over all current mappings setting/clearing as appropos.
	 */
	vm_page_lock_queues();
	for (pv = TAILQ_FIRST(&m->md.pv_list); pv; pv = npv) {
		npv = TAILQ_NEXT(pv, pv_plist);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		if (pte == NULL || !pte_test(pte, PTE_V))
			panic("page on pm_pvlist has no pte");

		va = pv->pv_va;
		pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE,
		    VM_PROT_READ | VM_PROT_EXECUTE);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
	vm_page_unlock_queues();
}

/*
 *	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 */
int
pmap_ts_referenced(vm_page_t m)
{

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	if (m->md.pv_flags & PV_TABLE_REF) {
		vm_page_lock_queues();
		m->md.pv_flags &= ~PV_TABLE_REF;
		vm_page_unlock_queues();
		return (1);
	}
	return (0);
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
	boolean_t rv;

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_is_modified: page %p is not managed", m));

	/*
	 * If the page is not VPO_BUSY, then PG_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PG_WRITEABLE
	 * is clear, no PTEs can have PTE_D set.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->flags & PG_WRITEABLE) == 0)
		return (FALSE);
	vm_page_lock_queues();
	if (m->md.pv_flags & PV_TABLE_MOD)
		rv = TRUE;
	else
		rv = pmap_testbit(m, PTE_D);
	vm_page_unlock_queues();
	return (rv);
}

/* N/C */

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	boolean_t rv;

	rv = FALSE;
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, addr);
	if (pde != NULL && *pde != 0) {
		pte = pmap_pde_to_pte(pde, addr);
		rv = (*pte == 0);
	}
	PMAP_UNLOCK(pmap);
	return (rv);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("pmap_clear_modify: page %p is busy", m));

	/*
	 * If the page is not PG_WRITEABLE, then no PTEs can have PTE_D set.
	 * If the object containing the page is locked and the page is not
	 * VPO_BUSY, then PG_WRITEABLE cannot be concurrently set.
	 */
	if ((m->flags & PG_WRITEABLE) == 0)
		return;
	vm_page_lock_queues();
	if (m->md.pv_flags & PV_TABLE_MOD) {
		pmap_changebit(m, PTE_D, FALSE);
		m->md.pv_flags &= ~PV_TABLE_MOD;
	}
	vm_page_unlock_queues();
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

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	return ((m->md.pv_flags & PV_TABLE_REF) != 0);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_page_t m)
{

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_clear_reference: page %p is not managed", m));
	vm_page_lock_queues();
	if (m->md.pv_flags & PV_TABLE_REF) {
		m->md.pv_flags &= ~PV_TABLE_REF;
	}
	vm_page_unlock_queues();
}

/*
 * Miscellaneous support routines follow
 */

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 *
 * Use XKPHYS uncached for 64 bit, and KSEG1 where possible for 32 bit.
 */
void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
        vm_offset_t va, tmpva, offset;

	/* 
	 * KSEG1 maps only first 512M of phys address space. For 
	 * pa > 0x20000000 we should make proper mapping * using pmap_kenter.
	 */
	if (MIPS_DIRECT_MAPPABLE(pa + size - 1))
		return ((void *)MIPS_PHYS_TO_DIRECT_UNCACHED(pa));
	else {
		offset = pa & PAGE_MASK;
		size = roundup(size + offset, PAGE_SIZE);
        
		va = kmem_alloc_nofault(kernel_map, size);
		if (!va)
			panic("pmap_mapdev: Couldn't alloc kernel virtual memory");
		pa = trunc_page(pa);
		for (tmpva = va; size > 0;) {
			pmap_kenter_attr(tmpva, pa, PTE_C_UNCACHED);
			size -= PAGE_SIZE;
			tmpva += PAGE_SIZE;
			pa += PAGE_SIZE;
		}
	}

	return ((void *)(va + offset));
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
#ifndef __mips_n64
	vm_offset_t base, offset, tmpva;

	/* If the address is within KSEG1 then there is nothing to do */
	if (va >= MIPS_KSEG1_START && va <= MIPS_KSEG1_END)
		return;

	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = roundup(size + offset, PAGE_SIZE);
	for (tmpva = base; tmpva < base + size; tmpva += PAGE_SIZE)
		pmap_kremove(tmpva);
	kmem_free(kernel_map, base, size);
#endif
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	vm_page_t m;
	int val;
	boolean_t managed;

	PMAP_LOCK(pmap);
retry:
	ptep = pmap_pte(pmap, addr);
	pte = (ptep != NULL) ? *ptep : 0;
	if (!pte_test(&pte, PTE_V)) {
		val = 0;
		goto out;
	}
	val = MINCORE_INCORE;
	if (pte_test(&pte, PTE_D))
		val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
	pa = TLBLO_PTE_TO_PA(pte);
	managed = page_is_managed(pa);
	if (managed) {
		/*
		 * This may falsely report the given address as
		 * MINCORE_REFERENCED.  Unfortunately, due to the lack of
		 * per-PTE reference information, it is impossible to
		 * determine if the address is MINCORE_REFERENCED.  
		 */
		m = PHYS_TO_VM_PAGE(pa);
		if ((m->flags & PG_REFERENCED) != 0)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
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
pmap_activate(struct thread *td)
{
	pmap_t pmap, oldpmap;
	struct proc *p = td->td_proc;

	critical_enter();

	pmap = vmspace_pmap(p->p_vmspace);
	oldpmap = PCPU_GET(curpmap);

	if (oldpmap)
		CPU_NAND_ATOMIC(&oldpmap->pm_active, PCPU_PTR(cpumask));
	CPU_OR_ATOMIC(&pmap->pm_active, PCPU_PTR(cpumask));
	pmap_asid_alloc(pmap);
	if (td == curthread) {
		PCPU_SET(segbase, pmap->pm_segtab);
		mips_wr_entryhi(pmap->pm_asid[PCPU_GET(cpuid)].asid);
	}

	PCPU_SET(curpmap, pmap);
	critical_exit();
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
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

	if (size < NBSEG)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & SEGMASK;
	if (size - ((NBSEG - superpage_offset) & SEGMASK) < NBSEG ||
	    (*addr & SEGMASK) == superpage_offset)
		return;
	if ((*addr & SEGMASK) < superpage_offset)
		*addr = (*addr & ~SEGMASK) + superpage_offset;
	else
		*addr = ((*addr + SEGMASK) & ~SEGMASK) + superpage_offset;
}

/*
 * 	Increase the starting virtual address of the given mapping so
 * 	that it is aligned to not be the second page in a TLB entry.
 * 	This routine assumes that the length is appropriately-sized so
 * 	that the allocation does not share a TLB entry at all if required.
 */
void
pmap_align_tlb(vm_offset_t *addr)
{
	if ((*addr & PAGE_SIZE) == 0)
		return;
	*addr += PAGE_SIZE;
	return;
}

#ifdef DDB
DB_SHOW_COMMAND(ptable, ddb_pid_dump)
{
	pmap_t pmap;
	struct thread *td = NULL;
	struct proc *p;
	int i, j, k;
	vm_paddr_t pa;
	vm_offset_t va;

	if (have_addr) {
		td = db_lookup_thread(addr, TRUE);
		if (td == NULL) {
			db_printf("Invalid pid or tid");
			return;
		}
		p = td->td_proc;
		if (p->p_vmspace == NULL) {
			db_printf("No vmspace for process");
			return;
		}
			pmap = vmspace_pmap(p->p_vmspace);
	} else
		pmap = kernel_pmap;

	db_printf("pmap:%p segtab:%p asid:%x generation:%x\n",
	    pmap, pmap->pm_segtab, pmap->pm_asid[0].asid,
	    pmap->pm_asid[0].gen);
	for (i = 0; i < NPDEPG; i++) {
		pd_entry_t *pdpe;
		pt_entry_t *pde;
		pt_entry_t pte;

		pdpe = (pd_entry_t *)pmap->pm_segtab[i];
		if (pdpe == NULL)
			continue;
		db_printf("[%4d] %p\n", i, pdpe);
#ifdef __mips_n64
		for (j = 0; j < NPDEPG; j++) {
			pde = (pt_entry_t *)pdpe[j];
			if (pde == NULL)
				continue;
			db_printf("\t[%4d] %p\n", j, pde);
#else
		{
			j = 0;
			pde =  (pt_entry_t *)pdpe;
#endif
			for (k = 0; k < NPTEPG; k++) {
				pte = pde[k];
				if (pte == 0 || !pte_test(&pte, PTE_V))
					continue;
				pa = TLBLO_PTE_TO_PA(pte);
				va = ((u_long)i << SEGSHIFT) | (j << PDRSHIFT) | (k << PAGE_SHIFT);
				db_printf("\t\t[%04d] va: %p pte: %8jx pa:%jx\n",
				       k, (void *)va, (uintmax_t)pte, (uintmax_t)pa);
			}
		}
	}
}
#endif

#if defined(DEBUG)

static void pads(pmap_t pm);
void pmap_pvdump(vm_offset_t pa);

/* print address space of pmap*/
static void
pads(pmap_t pm)
{
	unsigned va, i, j;
	pt_entry_t *ptep;

	if (pm == kernel_pmap)
		return;
	for (i = 0; i < NPTEPG; i++)
		if (pm->pm_segtab[i])
			for (j = 0; j < NPTEPG; j++) {
				va = (i << SEGSHIFT) + (j << PAGE_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
					continue;
				if (pm != kernel_pmap &&
				    va >= VM_MAXUSER_ADDRESS)
					continue;
				ptep = pmap_pte(pm, va);
				if (pte_test(ptep, PTE_V))
					printf("%x:%x ", va, *(int *)ptep);
			}

}

void
pmap_pvdump(vm_offset_t pa)
{
	register pv_entry_t pv;
	vm_page_t m;

	printf("pa %x", pa);
	m = PHYS_TO_VM_PAGE(pa);
	for (pv = TAILQ_FIRST(&m->md.pv_list); pv;
	    pv = TAILQ_NEXT(pv, pv_list)) {
		printf(" -> pmap %p, va %x", (void *)pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}

/* N/C */
#endif


/*
 * Allocate TLB address space tag (called ASID or TLBPID) and return it.
 * It takes almost as much or more time to search the TLB for a
 * specific ASID and flush those entries as it does to flush the entire TLB.
 * Therefore, when we allocate a new ASID, we just take the next number. When
 * we run out of numbers, we flush the TLB, increment the generation count
 * and start over. ASID zero is reserved for kernel use.
 */
static void
pmap_asid_alloc(pmap)
	pmap_t pmap;
{
	if (pmap->pm_asid[PCPU_GET(cpuid)].asid != PMAP_ASID_RESERVED &&
	    pmap->pm_asid[PCPU_GET(cpuid)].gen == PCPU_GET(asid_generation));
	else {
		if (PCPU_GET(next_asid) == pmap_max_asid) {
			tlb_invalidate_all_user(NULL);
			PCPU_SET(asid_generation,
			    (PCPU_GET(asid_generation) + 1) & ASIDGEN_MASK);
			if (PCPU_GET(asid_generation) == 0) {
				PCPU_SET(asid_generation, 1);
			}
			PCPU_SET(next_asid, 1);	/* 0 means invalid */
		}
		pmap->pm_asid[PCPU_GET(cpuid)].asid = PCPU_GET(next_asid);
		pmap->pm_asid[PCPU_GET(cpuid)].gen = PCPU_GET(asid_generation);
		PCPU_SET(next_asid, PCPU_GET(next_asid) + 1);
	}
}

int
page_is_managed(vm_paddr_t pa)
{
	vm_offset_t pgnum = atop(pa);

	if (pgnum >= first_page) {
		vm_page_t m;

		m = PHYS_TO_VM_PAGE(pa);
		if (m == NULL)
			return (0);
		if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0)
			return (1);
	}
	return (0);
}

static pt_entry_t
init_pte_prot(vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	pt_entry_t rw;

	if (!(prot & VM_PROT_WRITE))
		rw =  PTE_V | PTE_RO | PTE_C_CACHE;
	else if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
		if ((m->md.pv_flags & PV_TABLE_MOD) != 0)
			rw =  PTE_V | PTE_D | PTE_C_CACHE;
		else
			rw = PTE_V | PTE_C_CACHE;
		vm_page_flag_set(m, PG_WRITEABLE);
	} else
		/* Needn't emulate a modified bit for unmanaged pages. */
		rw =  PTE_V | PTE_D | PTE_C_CACHE;
	return (rw);
}

/*
 * pmap_emulate_modified : do dirty bit emulation
 *
 * On SMP, update just the local TLB, other CPUs will update their
 * TLBs from PTE lazily, if they get the exception.
 * Returns 0 in case of sucess, 1 if the page is read only and we
 * need to fault.
 */
int
pmap_emulate_modified(pmap_t pmap, vm_offset_t va)
{
	vm_page_t m;
	pt_entry_t *pte;
 	vm_paddr_t pa;

	PMAP_LOCK(pmap);
	pte = pmap_pte(pmap, va);
	if (pte == NULL)
		panic("pmap_emulate_modified: can't find PTE");
#ifdef SMP
	/* It is possible that some other CPU changed m-bit */
	if (!pte_test(pte, PTE_V) || pte_test(pte, PTE_D)) {
		pmap_update_page_local(pmap, va, *pte);
		PMAP_UNLOCK(pmap);
		return (0);
	}
#else
	if (!pte_test(pte, PTE_V) || pte_test(pte, PTE_D))
		panic("pmap_emulate_modified: invalid pte");
#endif
	if (pte_test(pte, PTE_RO)) {
		/* write to read only page in the kernel */
		PMAP_UNLOCK(pmap);
		return (1);
	}
	pte_set(pte, PTE_D);
	pmap_update_page_local(pmap, va, *pte);
	pa = TLBLO_PTE_TO_PA(*pte);
	if (!page_is_managed(pa))
		panic("pmap_emulate_modified: unmanaged page");
	m = PHYS_TO_VM_PAGE(pa);
	m->md.pv_flags |= (PV_TABLE_REF | PV_TABLE_MOD);
	PMAP_UNLOCK(pmap);
	return (0);
}

/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		virtual address.
 */
 /* PMAP_INLINE */ vm_offset_t
pmap_kextract(vm_offset_t va)
{
	int mapped;

	/*
	 * First, the direct-mapped regions.
	 */
#if defined(__mips_n64)
	if (va >= MIPS_XKPHYS_START && va < MIPS_XKPHYS_END)
		return (MIPS_XKPHYS_TO_PHYS(va));
#endif
	if (va >= MIPS_KSEG0_START && va < MIPS_KSEG0_END)
		return (MIPS_KSEG0_TO_PHYS(va));

	if (va >= MIPS_KSEG1_START && va < MIPS_KSEG1_END)
		return (MIPS_KSEG1_TO_PHYS(va));

	/*
	 * User virtual addresses.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		pt_entry_t *ptep;

		if (curproc && curproc->p_vmspace) {
			ptep = pmap_pte(&curproc->p_vmspace->vm_pmap, va);
			if (ptep) {
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PAGE_MASK));
			}
			return (0);
		}
	}

	/*
	 * Should be kernel virtual here, otherwise fail
	 */
	mapped = (va >= MIPS_KSEG2_START || va < MIPS_KSEG2_END);
#if defined(__mips_n64)
	mapped = mapped || (va >= MIPS_XKSEG_START || va < MIPS_XKSEG_END);
#endif 
	/*
	 * Kernel virtual.
	 */

	if (mapped) {
		pt_entry_t *ptep;

		/* Is the kernel pmap initialized? */
		if (!CPU_EMPTY(&kernel_pmap->pm_active)) {
			/* It's inside the virtual address range */
			ptep = pmap_pte(kernel_pmap, va);
			if (ptep) {
				return (TLBLO_PTE_TO_PA(*ptep) |
				    (va & PAGE_MASK));
			}
		}
		return (0);
	}

	panic("%s for unknown address space %p.", __func__, (void *)va);
}


void 
pmap_flush_pvcache(vm_page_t m)
{
	pv_entry_t pv;

	if (m != NULL) {
		for (pv = TAILQ_FIRST(&m->md.pv_list); pv;
		    pv = TAILQ_NEXT(pv, pv_list)) {
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);
		}
	}
}
