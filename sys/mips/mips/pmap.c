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
#include "opt_msgbuf.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

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
#include <sys/pcpu.h>
#include <sys/sched.h>
#ifdef SMP
#include <sys/smp.h>
#endif

#include <machine/cache.h>
#include <machine/pltfm.h>
#include <machine/md_var.h>

#if defined(DIAGNOSTIC)
#define	PMAP_DIAGNOSTIC
#endif

#ifndef PMAP_SHPGPERPROC
#define	PMAP_SHPGPERPROC 200
#endif

#if !defined(PMAP_DIAGNOSTIC)
#define	PMAP_INLINE __inline
#else
#define	PMAP_INLINE
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	       (&((m)->pm_segtab[(vm_offset_t)(v) >> SEGSHIFT]))
#define	segtab_pde(m, v)	(m[(vm_offset_t)(v) >> SEGSHIFT])

#define	pmap_pte_w(pte)		((*(int *)pte & PTE_W) != 0)
#define	pmap_pde_v(pte)		((*(int *)pte) != 0)
#define	pmap_pte_m(pte)		((*(int *)pte & PTE_M) != 0)
#define	pmap_pte_v(pte)		((*(int *)pte & PTE_V) != 0)

#define	pmap_pte_set_w(pte, v)	((v)?(*(int *)pte |= PTE_W):(*(int *)pte &= ~PTE_W))
#define	pmap_pte_set_prot(pte, v) ((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

#define	MIPS_SEGSIZE		(1L << SEGSHIFT)
#define	mips_segtrunc(va)	((va) & ~(MIPS_SEGSIZE-1))
#define	pmap_TLB_invalidate_all() MIPS_TBIAP()
#define	pmap_va_asid(pmap, va)	((va) | ((pmap)->pm_asid[PCPU_GET(cpuid)].asid << VMTLB_PID_SHIFT))
#define	is_kernel_pmap(x)	((x) == kernel_pmap)

static struct pmap kernel_pmap_store;
pmap_t kernel_pmap;
pd_entry_t *kernel_segmap;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

static int nkpt;
unsigned pmap_max_asid;		/* max ASID supported by the system */


#define	PMAP_ASID_RESERVED	0


vm_offset_t kernel_vm_end;

static void pmap_asid_alloc(pmap_t pmap);

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static struct vm_object pvzone_obj;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
int pmap_pagedaemon_waken = 0;

struct fpage fpages_shared[FPAGES_SHARED];

struct sysmaps sysmaps_pcpu[MAXCPU];

static PMAP_INLINE void free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(void);
static __inline void pmap_changebit(vm_page_t m, int bit, boolean_t setem);

static int pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va);
static void pmap_remove_entry(struct pmap *pmap, vm_page_t m, vm_offset_t va);
static boolean_t pmap_testbit(vm_page_t m, int bit);
static void 
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t mpte,
    vm_page_t m, boolean_t wired);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags);

static vm_page_t _pmap_allocpte(pmap_t pmap, unsigned ptepindex, int flags);
static int pmap_unuse_pt(pmap_t, vm_offset_t, vm_page_t);
static int init_pte_prot(vm_offset_t va, vm_page_t m, vm_prot_t prot);
static void pmap_TLB_invalidate_kernel(vm_offset_t);
static void pmap_TLB_update_kernel(vm_offset_t, pt_entry_t);
static void pmap_init_fpage(void);

#ifdef SMP
static void pmap_invalidate_page_action(void *arg);
static void pmap_invalidate_all_action(void *arg);
static void pmap_update_page_action(void *arg);

#endif

struct local_sysmaps {
	struct mtx lock;
	pt_entry_t CMAP1;
	pt_entry_t CMAP2;
	caddr_t CADDR1;
	caddr_t CADDR2;
	uint16_t valid1, valid2;
};

/* This structure is for large memory
 * above 512Meg. We can't (in 32 bit mode)
 * just use the direct mapped MIPS_CACHED_TO_PHYS()
 * macros since we can't see the memory and must
 * map it in when we need to access it. In 64
 * bit mode this goes away.
 */
static struct local_sysmaps sysmap_lmem[MAXCPU];
caddr_t virtual_sys_start = (caddr_t)0;

pd_entry_t
pmap_segmap(pmap_t pmap, vm_offset_t va)
{
	if (pmap->pm_segtab)
		return (pmap->pm_segtab[((vm_offset_t)(va) >> SEGSHIFT)]);
	else
		return ((pd_entry_t)0);
}

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 */
pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *pdeaddr;

	if (pmap) {
		pdeaddr = (pt_entry_t *)pmap_segmap(pmap, va);
		if (pdeaddr) {
			return pdeaddr + vad_to_pte_offset(va);
		}
	}
	return ((pt_entry_t *)0);
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
	if (pa >= MIPS_KSEG0_LARGEST_PHYS) {
		panic("Out of memory below 512Meg?");
	}
	va = MIPS_PHYS_TO_CACHED(pa);
	bzero((caddr_t)va, size);
	return va;
}

/*
 *	Bootstrap the system enough to run with virtual memory.  This
 * assumes that the phys_avail array has been initialized.
 */
void
pmap_bootstrap(void)
{
	pt_entry_t *pgtab;
	pt_entry_t *pte;
	int i, j;
	int memory_larger_than_512meg = 0;

	/* Sort. */
again:
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] >= MIPS_KSEG0_LARGEST_PHYS) {
			memory_larger_than_512meg++;
		}
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
	}
	/*
	 * Steal the message buffer from the beginning of memory.
	 */
	msgbufp = (struct msgbuf *)pmap_steal_memory(MSGBUF_SIZE);
	msgbufinit(msgbufp, MSGBUF_SIZE);

	/*
	 * Steal thread0 kstack.
	 */
	kstack0 = pmap_steal_memory(KSTACK_PAGES << PAGE_SHIFT);


	virtual_avail = VM_MIN_KERNEL_ADDRESS + VM_KERNEL_ALLOC_OFFSET;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Steal some virtual space that will not be in kernel_segmap. This
	 * va memory space will be used to map in kernel pages that are
	 * outside the 512Meg region. Note that we only do this steal when
	 * we do have memory in this region, that way for systems with
	 * smaller memory we don't "steal" any va ranges :-)
	 */
	if (memory_larger_than_512meg) {
		for (i = 0; i < MAXCPU; i++) {
			sysmap_lmem[i].CMAP1 = PTE_G;
			sysmap_lmem[i].CMAP2 = PTE_G;
			sysmap_lmem[i].CADDR1 = (caddr_t)virtual_avail;
			virtual_avail += PAGE_SIZE;
			sysmap_lmem[i].CADDR2 = (caddr_t)virtual_avail;
			virtual_avail += PAGE_SIZE;
			sysmap_lmem[i].valid1 = sysmap_lmem[i].valid2 = 0;
			PMAP_LGMEM_LOCK_INIT(&sysmap_lmem[i]);
		}
	}
	virtual_sys_start = (caddr_t)virtual_avail;
	/*
	 * Allocate segment table for the kernel
	 */
	kernel_segmap = (pd_entry_t *)pmap_steal_memory(PAGE_SIZE);

	/*
	 * Allocate second level page tables for the kernel
	 */
	nkpt = NKPT;
	if (memory_larger_than_512meg) {
		/*
		 * If we have a large memory system we CANNOT afford to hit
		 * pmap_growkernel() and allocate memory. Since we MAY end
		 * up with a page that is NOT mappable. For that reason we
		 * up front grab more. Normall NKPT is 120 (YMMV see pmap.h)
		 * this gives us 480meg of kernel virtual addresses at the
		 * cost of 120 pages (each page gets us 4 Meg). Since the
		 * kernel starts at virtual_avail, we can use this to
		 * calculate how many entris are left from there to the end
		 * of the segmap, we want to allocate all of it, which would
		 * be somewhere above 0xC0000000 - 0xFFFFFFFF which results
		 * in about 256 entries or so instead of the 120.
		 */
		nkpt = (PAGE_SIZE / sizeof(pd_entry_t)) - (virtual_avail >> SEGSHIFT);
	}
	pgtab = (pt_entry_t *)pmap_steal_memory(PAGE_SIZE * nkpt);

	/*
	 * The R[4-7]?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry. Thus invalid
	 * entrys must have the Global bit set so when Entry LO and Entry HI
	 * G bits are anded together they will produce a global bit to store
	 * in the tlb.
	 */
	for (i = 0, pte = pgtab; i < (nkpt * NPTEPG); i++, pte++)
		*pte = PTE_G;

	printf("Va=0x%x Ve=%x\n", virtual_avail, virtual_end);
	/*
	 * The segment table contains the KVA of the pages in the second
	 * level page table.
	 */
	printf("init kernel_segmap va >> = %d nkpt:%d\n",
	    (virtual_avail >> SEGSHIFT),
	    nkpt);
	for (i = 0, j = (virtual_avail >> SEGSHIFT); i < nkpt; i++, j++)
		kernel_segmap[j] = (pd_entry_t)(pgtab + (i * NPTEPG));

	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i + 2]; i += 2);
	avail_end = phys_avail[i + 1];

	/*
	 * The kernel's pmap is statically allocated so we don't have to use
	 * pmap_create, which is unlikely to work correctly at this part of
	 * the boot sequence (XXX and which no longer exists).
	 */
	kernel_pmap = &kernel_pmap_store;

	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_segtab = kernel_segmap;
	kernel_pmap->pm_active = ~0;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	printf("avail_start:0x%x avail_end:0x%x\n",
	    avail_start, avail_end);

	kernel_pmap->pm_asid[PCPU_GET(cpuid)].asid = PMAP_ASID_RESERVED;
	kernel_pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
	pmap_max_asid = VMNUM_PIDS;
	MachSetPID(0);
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

	if (need_wired_tlb_page_pool)
		pmap_init_fpage();
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

#if defined(PMAP_DIAGNOSTIC)

/*
 * This code checks for non-writeable/modified pages.
 * This should be an invalid condition.
 */
static int
pmap_nw_modified(pt_entry_t pte)
{
	if ((pte & (PTE_M | PTE_RO)) == (PTE_M | PTE_RO))
		return (1);
	else
		return (0);
}

#endif


static void
pmap_invalidate_all(pmap_t pmap)
{
#ifdef SMP
	smp_rendezvous(0, pmap_invalidate_all_action, 0, (void *)pmap);
}

static void
pmap_invalidate_all_action(void *arg)
{
	pmap_t pmap = (pmap_t)arg;

#endif

	if (pmap->pm_active & PCPU_GET(cpumask)) {
		pmap_TLB_invalidate_all();
	} else
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
}

struct pmap_invalidate_page_arg {
	pmap_t pmap;
	vm_offset_t va;
};

static __inline void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
#ifdef SMP
	struct pmap_invalidate_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;

	smp_rendezvous(0, pmap_invalidate_page_action, 0, (void *)&arg);
}

static void
pmap_invalidate_page_action(void *arg)
{
	pmap_t pmap = ((struct pmap_invalidate_page_arg *)arg)->pmap;
	vm_offset_t va = ((struct pmap_invalidate_page_arg *)arg)->va;

#endif

	if (is_kernel_pmap(pmap)) {
		pmap_TLB_invalidate_kernel(va);
		return;
	}
	if (pmap->pm_asid[PCPU_GET(cpuid)].gen != PCPU_GET(asid_generation))
		return;
	else if (!(pmap->pm_active & PCPU_GET(cpumask))) {
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
		return;
	}
	va = pmap_va_asid(pmap, (va & ~PGOFSET));
	mips_TBIS(va);
}

static void
pmap_TLB_invalidate_kernel(vm_offset_t va)
{
	u_int32_t pid;

	MachTLBGetPID(pid);
	va = va | (pid << VMTLB_PID_SHIFT);
	mips_TBIS(va);
}

struct pmap_update_page_arg {
	pmap_t pmap;
	vm_offset_t va;
	pt_entry_t pte;
};

void
pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte)
{
#ifdef SMP
	struct pmap_update_page_arg arg;

	arg.pmap = pmap;
	arg.va = va;
	arg.pte = pte;

	smp_rendezvous(0, pmap_update_page_action, 0, (void *)&arg);
}

static void
pmap_update_page_action(void *arg)
{
	pmap_t pmap = ((struct pmap_update_page_arg *)arg)->pmap;
	vm_offset_t va = ((struct pmap_update_page_arg *)arg)->va;
	pt_entry_t pte = ((struct pmap_update_page_arg *)arg)->pte;

#endif
	if (is_kernel_pmap(pmap)) {
		pmap_TLB_update_kernel(va, pte);
		return;
	}
	if (pmap->pm_asid[PCPU_GET(cpuid)].gen != PCPU_GET(asid_generation))
		return;
	else if (!(pmap->pm_active & PCPU_GET(cpumask))) {
		pmap->pm_asid[PCPU_GET(cpuid)].gen = 0;
		return;
	}
	va = pmap_va_asid(pmap, va);
	MachTLBUpdate(va, pte);
}

static void
pmap_TLB_update_kernel(vm_offset_t va, pt_entry_t pte)
{
	u_int32_t pid;

	MachTLBGetPID(pid);
	va = va | (pid << VMTLB_PID_SHIFT);

	MachTLBUpdate(va, pte);
}

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
		retval = mips_tlbpfn_to_paddr(*pte) | (va & PAGE_MASK);
	}
	PMAP_UNLOCK(pmap);
	return retval;
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

	m = NULL;
	vm_page_lock_queues();
	PMAP_LOCK(pmap);

	pte = *pmap_pte(pmap, va);
	if (pte != 0 && pmap_pte_v(&pte) &&
	    ((pte & PTE_RW) || (prot & VM_PROT_WRITE) == 0)) {
		m = PHYS_TO_VM_PAGE(mips_tlbpfn_to_paddr(pte));
		vm_page_hold(m);
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);
	return (m);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * add a wired page to the kva
 */
 /* PMAP_INLINE */ void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	register pt_entry_t *pte;
	pt_entry_t npte, opte;

	npte = mips_paddr_to_tlbpfn(pa) | PTE_RW | PTE_V | PTE_G | PTE_W;

	if (is_cacheable_mem(pa))
		npte |= PTE_CACHE;
	else
		npte |= PTE_UNCACHED;

	pte = pmap_pte(kernel_pmap, va);
	opte = *pte;
	*pte = npte;

	pmap_update_page(kernel_pmap, va, npte);
}

/*
 * remove a page from the kernel pagetables
 */
 /* PMAP_INLINE */ void
pmap_kremove(vm_offset_t va)
{
	register pt_entry_t *pte;

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
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t start, vm_offset_t end, int prot)
{
	vm_offset_t va, sva;

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

	for (i = 0; i < count; i++) {
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
		va += PAGE_SIZE;
	}
}

/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

/*
 * floating pages (FPAGES) management routines
 *
 * FPAGES are the reserved virtual memory areas which can be
 * mapped to any physical memory. This gets used typically
 * in the following functions:
 *
 * pmap_zero_page
 * pmap_copy_page
 */

/*
 * Create the floating pages, aka FPAGES!
 */
static void
pmap_init_fpage()
{
	vm_offset_t kva;
	int i, j;
	struct sysmaps *sysmaps;

	/*
	 * We allocate a total of (FPAGES*MAXCPU + FPAGES_SHARED + 1) pages
	 * at first. FPAGES & FPAGES_SHARED should be EVEN Then we'll adjust
	 * 'kva' to be even-page aligned so that the fpage area can be wired
	 * in the TLB with a single TLB entry.
	 */
	kva = kmem_alloc_nofault(kernel_map,
	    (FPAGES * MAXCPU + 1 + FPAGES_SHARED) * PAGE_SIZE);
	if ((void *)kva == NULL)
		panic("pmap_init_fpage: fpage allocation failed");

	/*
	 * Make up start at an even page number so we can wire down the
	 * fpage area in the tlb with a single tlb entry.
	 */
	if ((((vm_offset_t)kva) >> PGSHIFT) & 1) {
		/*
		 * 'kva' is not even-page aligned. Adjust it and free the
		 * first page which is unused.
		 */
		kmem_free(kernel_map, (vm_offset_t)kva, NBPG);
		kva = ((vm_offset_t)kva) + NBPG;
	} else {
		/*
		 * 'kva' is even page aligned. We don't need the last page,
		 * free it.
		 */
		kmem_free(kernel_map, ((vm_offset_t)kva) + FSPACE, NBPG);
	}

	for (i = 0; i < MAXCPU; i++) {
		sysmaps = &sysmaps_pcpu[i];
		mtx_init(&sysmaps->lock, "SYSMAPS", NULL, MTX_DEF);

		/* Assign FPAGES pages to the CPU */
		for (j = 0; j < FPAGES; j++)
			sysmaps->fp[j].kva = kva + (j) * PAGE_SIZE;
		kva = ((vm_offset_t)kva) + (FPAGES * PAGE_SIZE);
	}

	/*
	 * An additional 2 pages are needed, one for pmap_zero_page_idle()
	 * and one for coredump. These pages are shared by all cpu's
	 */
	fpages_shared[PMAP_FPAGE3].kva = kva;
	fpages_shared[PMAP_FPAGE_KENTER_TEMP].kva = kva + PAGE_SIZE;
}

/*
 * Map the page to the fpage virtual address as specified thru' fpage id
 */
vm_offset_t
pmap_map_fpage(vm_paddr_t pa, struct fpage *fp, boolean_t check_unmaped)
{
	vm_offset_t kva;
	register pt_entry_t *pte;
	pt_entry_t npte;

	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));
	/*
	 * Check if the fpage is free
	 */
	if (fp->state) {
		if (check_unmaped == TRUE)
			pmap_unmap_fpage(pa, fp);
		else
			panic("pmap_map_fpage: fpage is busy");
	}
	fp->state = TRUE;
	kva = fp->kva;

	npte = mips_paddr_to_tlbpfn(pa) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
	pte = pmap_pte(kernel_pmap, kva);
	*pte = npte;

	pmap_TLB_update_kernel(kva, npte);

	return (kva);
}

/*
 * Unmap the page from the fpage virtual address as specified thru' fpage id
 */
void
pmap_unmap_fpage(vm_paddr_t pa, struct fpage *fp)
{
	vm_offset_t kva;
	register pt_entry_t *pte;

	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));
	/*
	 * Check if the fpage is busy
	 */
	if (!(fp->state)) {
		panic("pmap_unmap_fpage: fpage is free");
	}
	kva = fp->kva;

	pte = pmap_pte(kernel_pmap, kva);
	*pte = PTE_G;
	pmap_TLB_invalidate_kernel(kva);

	fp->state = FALSE;

	/*
	 * Should there be any flush operation at the end?
	 */
}

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
static int
_pmap_unwire_pte_hold(pmap_t pmap, vm_page_t m)
{

	/*
	 * unmap the page table page
	 */
	pmap->pm_segtab[m->pindex] = 0;
	--pmap->pm_stats.resident_count;

	if (pmap->pm_ptphint == m)
		pmap->pm_ptphint = NULL;

	/*
	 * If the page is finally unwired, simply free it.
	 */
	vm_page_free_zero(m);
	atomic_subtract_int(&cnt.v_wire_count, 1);
	return (1);
}

static PMAP_INLINE int
pmap_unwire_pte_hold(pmap_t pmap, vm_page_t m)
{
	--m->wire_count;
	if (m->wire_count == 0)
		return (_pmap_unwire_pte_hold(pmap, m));
	else
		return (0);
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
		ptepindex = (va >> SEGSHIFT);
		if (pmap->pm_ptphint &&
		    (pmap->pm_ptphint->pindex == ptepindex)) {
			mpte = pmap->pm_ptphint;
		} else {
			pteva = *pmap_pde(pmap, va);
			mpte = PHYS_TO_VM_PAGE(MIPS_CACHED_TO_PHYS(pteva));
			pmap->pm_ptphint = mpte;
		}
	}
	return pmap_unwire_pte_hold(pmap, mpte);
}

void
pmap_pinit0(pmap_t pmap)
{
	int i;

	PMAP_LOCK_INIT(pmap);
	pmap->pm_segtab = kernel_segmap;
	pmap->pm_active = 0;
	pmap->pm_ptphint = NULL;
	for (i = 0; i < MAXCPU; i++) {
		pmap->pm_asid[i].asid = PMAP_ASID_RESERVED;
		pmap->pm_asid[i].gen = 0;
	}
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit(pmap_t pmap)
{
	vm_page_t ptdpg;
	int i;
	int req;

	PMAP_LOCK_INIT(pmap);

	req = VM_ALLOC_NOOBJ | VM_ALLOC_NORMAL | VM_ALLOC_WIRED |
	    VM_ALLOC_ZERO;

#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool)
		req |= VM_ALLOC_WIRED_TLB_PG_POOL;
#endif
	/*
	 * allocate the page directory page
	 */
	ptdpg = vm_page_alloc(NULL, NUSERPGTBLS, req);

#if 0
	/* I think we can just delete these, now that PG_BUSY is gone */
	vm_page_lock_queues();
	vm_page_flag_clear(ptdpg, PTE_BUSY);	/* not usually mapped */
#endif
	ptdpg->valid = VM_PAGE_BITS_ALL;

#if 0
	vm_page_unlock_queues();
#endif

	pmap->pm_segtab = (pd_entry_t *)
	    MIPS_PHYS_TO_CACHED(VM_PAGE_TO_PHYS(ptdpg));
	if ((ptdpg->flags & PG_ZERO) == 0)
		bzero(pmap->pm_segtab, PAGE_SIZE);

	pmap->pm_active = 0;
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
	vm_offset_t pteva, ptepa;
	vm_page_t m;
	int req;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("_pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	req = VM_ALLOC_WIRED | VM_ALLOC_ZERO | VM_ALLOC_NOOBJ;
#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool)
		req |= VM_ALLOC_WIRED_TLB_PG_POOL;
#endif
	/*
	 * Find or fabricate a new pagetable page
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, req)) == NULL) {
		if (flags & M_WAITOK) {
			PMAP_UNLOCK(pmap);
			vm_page_unlock_queues();
			VM_WAIT;
			vm_page_lock_queues();
			PMAP_LOCK(pmap);
		}
		/*
		 * Indicate the need to retry.	While waiting, the page
		 * table page may have been allocated.
		 */
		return (NULL);
	}
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	KASSERT(m->queue == PQ_NONE,
	    ("_pmap_allocpte: %p->queue != PQ_NONE", m));

	/*
	 * Map the pagetable page into the process address space, if it
	 * isn't already there.
	 */

	pmap->pm_stats.resident_count++;

	ptepa = VM_PAGE_TO_PHYS(m);
	pteva = MIPS_PHYS_TO_CACHED(ptepa);
	pmap->pm_segtab[ptepindex] = (pd_entry_t)pteva;

	/*
	 * Set the page table hint
	 */
	pmap->pm_ptphint = m;

	/*
	 * Kernel page tables are allocated in pmap_bootstrap() or
	 * pmap_growkernel().
	 */
	if (is_kernel_pmap(pmap))
		panic("_pmap_allocpte() called for kernel pmap\n");

	m->valid = VM_PAGE_BITS_ALL;
	vm_page_flag_clear(m, PG_ZERO);

	return (m);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags)
{
	unsigned ptepindex;
	vm_offset_t pteva;
	vm_page_t m;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = va >> SEGSHIFT;
retry:
	/*
	 * Get the page directory entry
	 */
	pteva = (vm_offset_t)pmap->pm_segtab[ptepindex];

	/*
	 * If the page table page is mapped, we just increment the hold
	 * count, and activate it.
	 */
	if (pteva) {
		/*
		 * In order to get the page table page, try the hint first.
		 */
		if (pmap->pm_ptphint &&
		    (pmap->pm_ptphint->pindex == ptepindex)) {
			m = pmap->pm_ptphint;
		} else {
			m = PHYS_TO_VM_PAGE(MIPS_CACHED_TO_PHYS(pteva));
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
	return m;
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
	vm_page_t ptdpg;

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	ptdpg = PHYS_TO_VM_PAGE(MIPS_CACHED_TO_PHYS(pmap->pm_segtab));

	vm_page_lock_queues();
	ptdpg->wire_count--;
	atomic_subtract_int(&cnt.v_wire_count, 1);
	vm_page_free_zero(ptdpg);
	vm_page_unlock_queues();
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_offset_t ptppaddr;
	vm_page_t nkpg;
	pt_entry_t *pte;
	int i, req;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
	if (kernel_vm_end == 0) {
		kernel_vm_end = VM_MIN_KERNEL_ADDRESS + VM_KERNEL_ALLOC_OFFSET;
		nkpt = 0;
		while (segtab_pde(kernel_segmap, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) &
			    ~(PAGE_SIZE * NPTEPG - 1);
			nkpt++;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
		}
	}
	addr = (addr + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		if (segtab_pde(kernel_segmap, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) &
			    ~(PAGE_SIZE * NPTEPG - 1);
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
			continue;
		}
		/*
		 * This index is bogus, but out of the way
		 */
		req = VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED | VM_ALLOC_NOOBJ;
#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
		if (need_wired_tlb_page_pool)
			req |= VM_ALLOC_WIRED_TLB_PG_POOL;
#endif
		nkpg = vm_page_alloc(NULL, nkpt, req);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");

		nkpt++;

		ptppaddr = VM_PAGE_TO_PHYS(nkpg);
		if (ptppaddr >= MIPS_KSEG0_LARGEST_PHYS) {
			/*
			 * We need to do something here, but I am not sure
			 * what. We can access anything in the 0 - 512Meg
			 * region, but if we get a page to go in the kernel
			 * segmap that is outside of of that we really need
			 * to have another mapping beyond the temporary ones
			 * I have. Not sure how to do this yet. FIXME FIXME.
			 */
			panic("Gak, can't handle a k-page table outside of lower 512Meg");
		}
		pte = (pt_entry_t *)MIPS_PHYS_TO_CACHED(ptppaddr);
		segtab_pde(kernel_segmap, kernel_vm_end) = (pd_entry_t)pte;

		/*
		 * The R[4-7]?00 stores only one copy of the Global bit in
		 * the translation lookaside buffer for each 2 page entry.
		 * Thus invalid entrys must have the Global bit set so when
		 * Entry LO and Entry HI G bits are anded together they will
		 * produce a global bit to store in the tlb.
		 */
		for (i = 0; i < NPTEPG; i++, pte++)
			*pte = PTE_G;

		kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) &
		    ~(PAGE_SIZE * NPTEPG - 1);
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
get_pv_entry(void)
{

	pv_entry_count++;
	if ((pv_entry_count > pv_entry_high_water) &&
	    (pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup(&vm_pages_needed);
	}
	return uma_zalloc(pvzone, M_NOWAIT);
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

static void
pmap_remove_entry(struct pmap *pmap, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
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

	KASSERT(pv != NULL, ("pmap_remove_entry: pv not found"));
	TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count--;
	if (TAILQ_FIRST(&m->md.pv_list) == NULL)
		vm_page_flag_clear(m, PG_WRITEABLE);

	TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
	free_pv_entry(pv);
}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t mpte, vm_page_t m,
    boolean_t wired)
{

	pv_entry_t pv;

	pv = get_pv_entry();
	if (pv == NULL)
		panic("no pv entries: increase vm.pmap.shpgperproc");
	pv->pv_va = va;
	pv->pv_pmap = pmap;
	pv->pv_ptem = mpte;
	pv->pv_wired = wired;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;

}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(struct pmap *pmap, pt_entry_t *ptq, vm_offset_t va)
{
	pt_entry_t oldpte;
	vm_page_t m;
	vm_offset_t pa;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	oldpte = loadandclear((u_int *)ptq);
	if (is_kernel_pmap(pmap))
		*ptq = PTE_G;

	if (oldpte & PTE_W)
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	pa = mips_tlbpfn_to_paddr(oldpte);

	if (page_is_managed(pa)) {
		m = PHYS_TO_VM_PAGE(pa);
		if (oldpte & PTE_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified(oldpte)) {
				printf(
				    "pmap_remove: modified page not writable: va: 0x%x, pte: 0x%x\n",
				    va, oldpte);
			}
#endif
			vm_page_dirty(m);
		}
		if (m->md.pv_flags & PV_TABLE_REF)
			vm_page_flag_set(m, PG_REFERENCED);
		m->md.pv_flags &= ~(PV_TABLE_REF | PV_TABLE_MOD);

		pmap_remove_entry(pmap, m, va);
	}
	return pmap_unuse_pt(pmap, va, NULL);

}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(struct pmap *pmap, vm_offset_t va)
{
	register pt_entry_t *ptq;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	ptq = pmap_pte(pmap, va);

	/*
	 * if there is no pte for this address, just skip it!!!
	 */
	if (!ptq || !pmap_pte_v(ptq)) {
		return;
	}
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
	vm_offset_t va, nva;

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
	for (va = sva; va < eva; va = nva) {
		if (!*pmap_pde(pmap, va)) {
			nva = mips_segtrunc(va + MIPS_SEGSIZE);
			continue;
		}
		pmap_remove_page(pmap, va);
		nva = va + PAGE_SIZE;
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
	register pv_entry_t pv;
	register pt_entry_t *pte, tpte;

#if defined(PMAP_DEBUG)
	/*
	 * XXX This makes pmap_remove_all() illegal for non-managed pages!
	 */
	if (m->flags & PG_FICTITIOUS) {
		panic("pmap_page_protect: illegal for unmanaged page, va: 0x%x", VM_PAGE_TO_PHYS(m));
	}
#endif
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);

	if (m->md.pv_flags & PV_TABLE_REF)
		vm_page_flag_set(m, PG_REFERENCED);

	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		PMAP_LOCK(pv->pv_pmap);
		pv->pv_pmap->pm_stats.resident_count--;

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);

		tpte = loadandclear((u_int *)pte);
		if (is_kernel_pmap(pv->pv_pmap))
			*pte = PTE_G;

		if (tpte & PTE_W)
			pv->pv_pmap->pm_stats.wired_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PTE_M) {
#if defined(PMAP_DIAGNOSTIC)
			if (pmap_nw_modified(tpte)) {
				printf(
				    "pmap_remove_all: modified page not writable: va: 0x%x, pte: 0x%x\n",
				    pv->pv_va, tpte);
			}
#endif
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
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pt_entry_t *pte;

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
	while (sva < eva) {
		pt_entry_t pbits, obits;
		vm_page_t m;
		vm_offset_t pa;

		/*
		 * If segment table entry is empty, skip this segment.
		 */
		if (!*pmap_pde(pmap, sva)) {
			sva = mips_segtrunc(sva + MIPS_SEGSIZE);
			continue;
		}
		/*
		 * If pte is invalid, skip this page
		 */
		pte = pmap_pte(pmap, sva);
		if (!pmap_pte_v(pte)) {
			sva += PAGE_SIZE;
			continue;
		}
retry:
		obits = pbits = *pte;
		pa = mips_tlbpfn_to_paddr(pbits);

		if (page_is_managed(pa)) {
			m = PHYS_TO_VM_PAGE(pa);
			if (m->md.pv_flags & PV_TABLE_REF) {
				vm_page_flag_set(m, PG_REFERENCED);
				m->md.pv_flags &= ~PV_TABLE_REF;
			}
			if (pbits & PTE_M) {
				vm_page_dirty(m);
				m->md.pv_flags &= ~PV_TABLE_MOD;
			}
		}
		pbits = (pbits & ~PTE_M) | PTE_RO;

		if (pbits != *pte) {
			if (!atomic_cmpset_int((u_int *)pte, obits, pbits))
				goto retry;
			pmap_update_page(pmap, sva, pbits);
		}
		sva += PAGE_SIZE;
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
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t fault_type, vm_page_t m, vm_prot_t prot,
    boolean_t wired)
{
	vm_offset_t pa, opa;
	register pt_entry_t *pte;
	pt_entry_t origpte, newpte;
	vm_page_t mpte, om;
	int rw = 0;

	if (pmap == NULL)
		return;

	va &= ~PAGE_MASK;
#ifdef PMAP_DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
#endif

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
		panic("pmap_enter: invalid page directory, pdir=%p, va=0x%x\n",
		    (void *)pmap->pm_segtab, va);
	}
	pa = VM_PAGE_TO_PHYS(m);
	om = NULL;
	origpte = *pte;
	opa = mips_tlbpfn_to_paddr(origpte);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if ((origpte & PTE_V) && (opa == pa)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is
		 * wired, the PT page will be also.
		 */
		if (wired && ((origpte & PTE_W) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (origpte & PTE_W))
			pmap->pm_stats.wired_count--;

#if defined(PMAP_DIAGNOSTIC)
		if (pmap_nw_modified(origpte)) {
			printf(
			    "pmap_enter: modified page not writable: va: 0x%x, pte: 0x%x\n",
			    va, origpte);
		}
#endif

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->wire_count--;

		/*
		 * We might be turning off write access to the page, so we
		 * go ahead and sense modify status.
		 */
		if (page_is_managed(opa)) {
			om = m;
		}
		goto validate;
	}
	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		if (origpte & PTE_W)
			pmap->pm_stats.wired_count--;

		if (page_is_managed(opa)) {
			om = PHYS_TO_VM_PAGE(opa);
			pmap_remove_entry(pmap, om, va);
		}
		if (mpte != NULL) {
			mpte->wire_count--;
			KASSERT(mpte->wire_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			    " va: 0x%x", va));
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
		pmap_insert_entry(pmap, va, mpte, m, wired);
	}
	/*
	 * Increment counters
	 */
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	rw = init_pte_prot(va, m, prot);

	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	newpte = mips_paddr_to_tlbpfn(pa) | rw | PTE_V;

	if (is_cacheable_mem(pa))
		newpte |= PTE_CACHE;
	else
		newpte |= PTE_UNCACHED;

	if (wired)
		newpte |= PTE_W;

	if (is_kernel_pmap(pmap)) {
	         newpte |= PTE_G;
	}

	/*
	 * if the mapping or permission bits are different, we need to
	 * update the pte.
	 */
	if (origpte != newpte) {
		if (origpte & PTE_V) {
			*pte = newpte;
			if (page_is_managed(opa) && (opa != pa)) {
				if (om->md.pv_flags & PV_TABLE_REF)
					vm_page_flag_set(om, PG_REFERENCED);
				om->md.pv_flags &=
				    ~(PV_TABLE_REF | PV_TABLE_MOD);
			}
			if (origpte & PTE_M) {
				KASSERT((origpte & PTE_RW),
				    ("pmap_enter: modified page not writable:"
				    " va: 0x%x, pte: 0x%lx", va, origpte));
				if (page_is_managed(opa))
					vm_page_dirty(om);
			}
		} else {
			*pte = newpte;
		}
	}
	pmap_update_page(pmap, va, newpte);

	/*
	 * Sync I & D caches for executable pages.  Do this only if the the
	 * target pmap belongs to the current process.  Otherwise, an
	 * unresolvable TLB miss may occur.
	 */
	if (!is_kernel_pmap(pmap) && (pmap == &curproc->p_vmspace->vm_pmap) &&
	    (prot & VM_PROT_EXECUTE)) {
		mips_icache_sync_range(va, NBPG);
		mips_dcache_wbinv_range(va, NBPG);
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
	pt_entry_t *pte;
	vm_offset_t pa;
	vm_page_t mpte = NULL;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0,
	    ("pmap_enter_quick: managed mapping within the clean submap"));
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	PMAP_LOCK(pmap);
	/*
	 * In the case that a page table page is not resident, we are
	 * creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		unsigned ptepindex;
		vm_offset_t pteva;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = va >> SEGSHIFT;
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->wire_count++;
		} else {
	retry:
			/*
			 * Get the page directory entry
			 */
			pteva = (vm_offset_t)pmap->pm_segtab[ptepindex];

			/*
			 * If the page table page is mapped, we just
			 * increment the hold count, and activate it.
			 */
			if (pteva) {
				if (pmap->pm_ptphint &&
				    (pmap->pm_ptphint->pindex == ptepindex)) {
					mpte = pmap->pm_ptphint;
				} else {
					mpte = PHYS_TO_VM_PAGE(MIPS_CACHED_TO_PHYS(pteva));
					pmap->pm_ptphint = mpte;
				}
				mpte->wire_count++;
			} else {
				mpte = _pmap_allocpte(pmap, ptepindex, M_NOWAIT);
				if (mpte == NULL) {
					PMAP_UNLOCK(pmap);
					vm_page_busy(m);
					vm_page_unlock_queues();
					VM_OBJECT_UNLOCK(m->object);
					VM_WAIT;
					VM_OBJECT_LOCK(m->object);
					vm_page_lock_queues();
					vm_page_wakeup(m);
					PMAP_LOCK(pmap);
					goto retry;
				}
			}
		}
	} else {
		mpte = NULL;
	}

	pte = pmap_pte(pmap, va);
	if (pmap_pte_v(pte)) {
		if (mpte)
			pmap_unwire_pte_hold(pmap, mpte);
		PMAP_UNLOCK(pmap);
		return;
	}
	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0)
		pmap_insert_entry(pmap, va, mpte, m, FALSE);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * Now validate mapping with RO protection
	 */
	*pte = mips_paddr_to_tlbpfn(pa) | PTE_V;

	if (is_cacheable_mem(pa))
		*pte |= PTE_CACHE;
	else
		*pte |= PTE_UNCACHED;

	if (is_kernel_pmap(pmap))
		*pte |= PTE_G;
	else {
		*pte |= PTE_RO;
		/*
		 * Sync I & D caches.  Do this only if the the target pmap
		 * belongs to the current process.  Otherwise, an
		 * unresolvable TLB miss may occur. */
		if (pmap == &curproc->p_vmspace->vm_pmap) {
			va &= ~PAGE_MASK;
			mips_icache_sync_range(va, NBPG);
			mips_dcache_wbinv_range(va, NBPG);
		}
	}

	PMAP_UNLOCK(pmap);
	return;
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	vm_offset_t va;

	if (i != 0)
		printf("%s: ERROR!!! More than one page of virtual address mapping not supported\n",
		    __func__);

#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool) {
		va = pmap_map_fpage(pa, &fpages_shared[PMAP_FPAGE_KENTER_TEMP],
		    TRUE);
	} else
#endif
	if (pa < MIPS_KSEG0_LARGEST_PHYS) {
		va = MIPS_PHYS_TO_CACHED(pa);
	} else {
		int cpu;
		struct local_sysmaps *sysm;

		cpu = PCPU_GET(cpuid);
		sysm = &sysmap_lmem[cpu];
		/* Since this is for the debugger, no locks or any other fun */
		sysm->CMAP1 = mips_paddr_to_tlbpfn(pa) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
		pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
		sysm->valid1 = 1;
		va = (vm_offset_t)sysm->CADDR1;
	}
	return ((void *)va);
}

void
pmap_kenter_temporary_free(vm_paddr_t pa)
{
	int cpu;
	struct local_sysmaps *sysm;

	if (pa < MIPS_KSEG0_LARGEST_PHYS) {
		/* nothing to do for this case */
		return;
	}
	cpu = PCPU_GET(cpuid);
	sysm = &sysmap_lmem[cpu];
	if (sysm->valid1) {
		pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR1);
		sysm->CMAP1 = 0;
		sysm->valid1 = 0;
	}
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
	vm_page_t m;
	vm_pindex_t diff, psize;

	psize = atop(end - start);
	m = m_start;
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
	        /* FIX ME FIX ME - prot is passed in both the
		 * the normal spot m, prot but also as the fault_type
		 * which we don't use. If we ever use it in pmap_enter
		 * we will have to fix this.
	         */
		pmap_enter(pmap, start + ptoa(diff), prot, m, prot &
		    (VM_PROT_READ | VM_PROT_EXECUTE), FALSE);
		m = TAILQ_NEXT(m, listq);
	}
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
	KASSERT(object->type == OBJT_DEVICE,
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
	register pt_entry_t *pte;

	if (pmap == NULL)
		return;

	PMAP_LOCK(pmap);
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
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool) {
		struct fpage *fp1;
		struct sysmaps *sysmaps;

		sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
		mtx_lock(&sysmaps->lock);
		sched_pin();

		fp1 = &sysmaps->fp[PMAP_FPAGE1];
		va = pmap_map_fpage(phys, fp1, FALSE);
		bzero((caddr_t)va, PAGE_SIZE);
		pmap_unmap_fpage(phys, fp1);
		sched_unpin();
		mtx_unlock(&sysmaps->lock);
		/*
		 * Should you do cache flush?
		 */
	} else
#endif
	if (phys < MIPS_KSEG0_LARGEST_PHYS) {

		va = MIPS_PHYS_TO_CACHED(phys);

		bzero((caddr_t)va, PAGE_SIZE);
	} else {
		int cpu;
		struct local_sysmaps *sysm;

		cpu = PCPU_GET(cpuid);
		sysm = &sysmap_lmem[cpu];
		PMAP_LGMEM_LOCK(sysm);
		sched_pin();
		sysm->CMAP1 = mips_paddr_to_tlbpfn(phys) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
		pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
		sysm->valid1 = 1;
		bzero(sysm->CADDR1, PAGE_SIZE);
		pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR1);
		sysm->CMAP1 = 0;
		sysm->valid1 = 0;
		sched_unpin();
		PMAP_LGMEM_UNLOCK(sysm);
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

#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool) {
		struct fpage *fp1;
		struct sysmaps *sysmaps;

		sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
		mtx_lock(&sysmaps->lock);
		sched_pin();

		fp1 = &sysmaps->fp[PMAP_FPAGE1];
		va = pmap_map_fpage(phys, fp1, FALSE);
		bzero((caddr_t)va + off, size);
		pmap_unmap_fpage(phys, fp1);

		sched_unpin();
		mtx_unlock(&sysmaps->lock);
	} else
#endif
	if (phys < MIPS_KSEG0_LARGEST_PHYS) {
		va = MIPS_PHYS_TO_CACHED(phys);
		bzero((char *)(caddr_t)va + off, size);
	} else {
		int cpu;
		struct local_sysmaps *sysm;

		cpu = PCPU_GET(cpuid);
		sysm = &sysmap_lmem[cpu];
		PMAP_LGMEM_LOCK(sysm);
		sched_pin();
		sysm->CMAP1 = mips_paddr_to_tlbpfn(phys) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
		pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
		sysm->valid1 = 1;
		bzero((char *)sysm->CADDR1 + off, size);
		pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR1);
		sysm->CMAP1 = 0;
		sysm->valid1 = 0;
		sched_unpin();
		PMAP_LGMEM_UNLOCK(sysm);
	}
}

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va;
	vm_paddr_t phys = VM_PAGE_TO_PHYS(m);

#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool) {
		sched_pin();
		va = pmap_map_fpage(phys, &fpages_shared[PMAP_FPAGE3], FALSE);
		bzero((caddr_t)va, PAGE_SIZE);
		pmap_unmap_fpage(phys, &fpages_shared[PMAP_FPAGE3]);
		sched_unpin();
	} else
#endif
	if (phys < MIPS_KSEG0_LARGEST_PHYS) {
		va = MIPS_PHYS_TO_CACHED(phys);
		bzero((caddr_t)va, PAGE_SIZE);
	} else {
		int cpu;
		struct local_sysmaps *sysm;

		cpu = PCPU_GET(cpuid);
		sysm = &sysmap_lmem[cpu];
		PMAP_LGMEM_LOCK(sysm);
		sched_pin();
		sysm->CMAP1 = mips_paddr_to_tlbpfn(phys) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
		pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
		sysm->valid1 = 1;
		bzero(sysm->CADDR1, PAGE_SIZE);
		pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR1);
		sysm->CMAP1 = 0;
		sysm->valid1 = 0;
		sched_unpin();
		PMAP_LGMEM_UNLOCK(sysm);
	}

}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	vm_offset_t va_src, va_dst;
	vm_paddr_t phy_src = VM_PAGE_TO_PHYS(src);
	vm_paddr_t phy_dst = VM_PAGE_TO_PHYS(dst);


#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	if (need_wired_tlb_page_pool) {
		struct fpage *fp1, *fp2;
		struct sysmaps *sysmaps;

		sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
		mtx_lock(&sysmaps->lock);
		sched_pin();

		fp1 = &sysmaps->fp[PMAP_FPAGE1];
		fp2 = &sysmaps->fp[PMAP_FPAGE2];

		va_src = pmap_map_fpage(phy_src, fp1, FALSE);
		va_dst = pmap_map_fpage(phy_dst, fp2, FALSE);

		bcopy((caddr_t)va_src, (caddr_t)va_dst, PAGE_SIZE);

		pmap_unmap_fpage(phy_src, fp1);
		pmap_unmap_fpage(phy_dst, fp2);
		sched_unpin();
		mtx_unlock(&sysmaps->lock);

		/*
		 * Should you flush the cache?
		 */
	} else
#endif
	{
		if ((phy_src < MIPS_KSEG0_LARGEST_PHYS) && (phy_dst < MIPS_KSEG0_LARGEST_PHYS)) {
			/* easy case, all can be accessed via KSEG0 */
			va_src = MIPS_PHYS_TO_CACHED(phy_src);
			va_dst = MIPS_PHYS_TO_CACHED(phy_dst);
			bcopy((caddr_t)va_src, (caddr_t)va_dst, PAGE_SIZE);
		} else {
			int cpu;
			struct local_sysmaps *sysm;

			cpu = PCPU_GET(cpuid);
			sysm = &sysmap_lmem[cpu];
			PMAP_LGMEM_LOCK(sysm);
			sched_pin();
			if (phy_src < MIPS_KSEG0_LARGEST_PHYS) {
				/* one side needs mapping - dest */
				va_src = MIPS_PHYS_TO_CACHED(phy_src);
				sysm->CMAP2 = mips_paddr_to_tlbpfn(phy_dst) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
				pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR2, sysm->CMAP2);
				sysm->valid2 = 2;
				va_dst = (vm_offset_t)sysm->CADDR2;
			} else if (phy_dst < MIPS_KSEG0_LARGEST_PHYS) {
				/* one side needs mapping - src */
				va_dst = MIPS_PHYS_TO_CACHED(phy_dst);
				sysm->CMAP1 = mips_paddr_to_tlbpfn(phy_src) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
				pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
				va_src = (vm_offset_t)sysm->CADDR1;
				sysm->valid1 = 1;
			} else {
				/* all need mapping */
				sysm->CMAP1 = mips_paddr_to_tlbpfn(phy_src) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
				sysm->CMAP2 = mips_paddr_to_tlbpfn(phy_dst) | PTE_RW | PTE_V | PTE_G | PTE_W | PTE_CACHE;
				pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR1, sysm->CMAP1);
				pmap_TLB_update_kernel((vm_offset_t)sysm->CADDR2, sysm->CMAP2);
				sysm->valid1 = sysm->valid2 = 1;
				va_src = (vm_offset_t)sysm->CADDR1;
				va_dst = (vm_offset_t)sysm->CADDR2;
			}
			bcopy((void *)va_src, (void *)va_dst, PAGE_SIZE);
			if (sysm->valid1) {
				pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR1);
				sysm->CMAP1 = 0;
				sysm->valid1 = 0;
			}
			if (sysm->valid2) {
				pmap_TLB_invalidate_kernel((vm_offset_t)sysm->CADDR2);
				sysm->CMAP2 = 0;
				sysm->valid2 = 0;
			}
			sched_unpin();
			PMAP_LGMEM_UNLOCK(sysm);
		}
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

	if (m->flags & PG_FICTITIOUS)
		return FALSE;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pmap) {
			PMAP_UNLOCK(pmap);
			vm_page_unlock_queues();
			return TRUE;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	PMAP_UNLOCK(pmap);
	vm_page_unlock_queues();
	return (FALSE);
}

#define	PMAP_REMOVE_PAGES_CURPROC_ONLY
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

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
#endif

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	sched_pin();
	//XXX need to be TAILQ_FOREACH_SAFE ?
	    for (pv = TAILQ_FIRST(&pmap->pm_pvlist);
	    pv;
	    pv = npv) {

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		if (!pmap_pte_v(pte))
			panic("pmap_remove_pages: page on pm_pvlist has no pte\n");
		tpte = *pte;

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (tpte & PTE_W) {
			npv = TAILQ_NEXT(pv, pv_plist);
			continue;
		}
		*pte = is_kernel_pmap(pmap) ? PTE_G : 0;

		m = PHYS_TO_VM_PAGE(mips_tlbpfn_to_paddr(tpte));

		KASSERT(m < &vm_page_array[vm_page_array_size],
		    ("pmap_remove_pages: bad tpte %lx", tpte));

		pv->pv_pmap->pm_stats.resident_count--;

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tpte & PTE_M) {
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
	sched_unpin();
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
		return rv;

	if (TAILQ_FIRST(&m->md.pv_list) == NULL)
		return rv;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (tb) at va: 0x%x\n", pv->pv_va);
			continue;
		}
#endif
		PMAP_LOCK(pv->pv_pmap);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
		rv = (*pte & bit) != 0;
		PMAP_UNLOCK(pv->pv_pmap);
		if (rv)
			break;
	}
	return (rv);
}

/*
 * this routine is used to modify bits in ptes
 */
static __inline void
pmap_changebit(vm_page_t m, int bit, boolean_t setem)
{
	register pv_entry_t pv;
	register pt_entry_t *pte;

	if (m->flags & PG_FICTITIOUS)
		return;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
#if defined(PMAP_DIAGNOSTIC)
		if (!pv->pv_pmap) {
			printf("Null pmap (cb) at va: 0x%x\n", pv->pv_va);
			continue;
		}
#endif

		PMAP_LOCK(pv->pv_pmap);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);

		if (setem) {
			*(int *)pte |= bit;
			pmap_update_page(pv->pv_pmap, pv->pv_va, *pte);
		} else {
			vm_offset_t pbits = *(vm_offset_t *)pte;

			if (pbits & bit) {
				if (bit == PTE_RW) {
					if (pbits & PTE_M) {
						vm_page_dirty(m);
					}
					*(int *)pte = (pbits & ~(PTE_M | PTE_RW)) |
					    PTE_RO;
				} else {
					*(int *)pte = pbits & ~bit;
				}
				pmap_update_page(pv->pv_pmap, pv->pv_va, *pte);
			}
		}
		PMAP_UNLOCK(pv->pv_pmap);
	}
	if (!setem && bit == PTE_RW)
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
	int count;

	count = 0;
	if ((m->flags & PG_FICTITIOUS) != 0)
		return (count);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list)
	    if (pv->pv_wired)
		count++;
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

	if ((m->flags & PG_WRITEABLE) == 0)
		return;

	/*
	 * Loop over all current mappings setting/clearing as appropos.
	 */
	for (pv = TAILQ_FIRST(&m->md.pv_list); pv; pv = npv) {
		npv = TAILQ_NEXT(pv, pv_plist);
		pte = pmap_pte(pv->pv_pmap, pv->pv_va);

		if ((pte == NULL) || !mips_pg_v(*pte))
			panic("page on pm_pvlist has no pte\n");

		va = pv->pv_va;
		pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE,
		    VM_PROT_READ | VM_PROT_EXECUTE);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
}

/*
 *	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 */
int
pmap_ts_referenced(vm_page_t m)
{
	if (m->flags & PG_FICTITIOUS)
		return (0);

	if (m->md.pv_flags & PV_TABLE_REF) {
		m->md.pv_flags &= ~PV_TABLE_REF;
		return 1;
	}
	return 0;
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
	if (m->flags & PG_FICTITIOUS)
		return FALSE;

	if (m->md.pv_flags & PV_TABLE_MOD)
		return TRUE;
	else
		return pmap_testbit(m, PTE_M);
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
	pt_entry_t *pte;
	boolean_t rv;

	rv = FALSE;
	PMAP_LOCK(pmap);
	if (*pmap_pde(pmap, addr)) {
		pte = pmap_pte(pmap, addr);
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
	if (m->flags & PG_FICTITIOUS)
		return;
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->md.pv_flags & PV_TABLE_MOD) {
		pmap_changebit(m, PTE_M, FALSE);
		m->md.pv_flags &= ~PV_TABLE_MOD;
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
	if (m->flags & PG_FICTITIOUS)
		return;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (m->md.pv_flags & PV_TABLE_REF) {
		m->md.pv_flags &= ~PV_TABLE_REF;
	}
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
 */
void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
        vm_offset_t va, tmpva, offset;

	/* 
	 * KSEG1 maps only first 512M of phys address space. For 
	 * pa > 0x20000000 we should make proper mapping * using pmap_kenter.
	 */
	if (pa + size < MIPS_KSEG0_LARGEST_PHYS)
		return (void *)MIPS_PHYS_TO_KSEG1(pa);
	else {
		offset = pa & PAGE_MASK;
		size = roundup(size, PAGE_SIZE);
        
		va = kmem_alloc_nofault(kernel_map, size);
		if (!va)
			panic("pmap_mapdev: Couldn't alloc kernel virtual memory");
		for (tmpva = va; size > 0;) {
			pmap_kenter(tmpva, pa);
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
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{

	pt_entry_t *ptep, pte;
	vm_page_t m;
	int val = 0;

	PMAP_LOCK(pmap);
	ptep = pmap_pte(pmap, addr);
	pte = (ptep != NULL) ? *ptep : 0;
	PMAP_UNLOCK(pmap);

	if (mips_pg_v(pte)) {
		vm_offset_t pa;

		val = MINCORE_INCORE;
		pa = mips_tlbpfn_to_paddr(pte);
		if (!page_is_managed(pa))
			return val;

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if (pte & PTE_M)
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else {
			vm_page_lock_queues();
			if (m->dirty || pmap_is_modified(m))
				val |= MINCORE_MODIFIED_OTHER;
			vm_page_unlock_queues();
		}
		/*
		 * Referenced by us or someone
		 */
		vm_page_lock_queues();
		if ((m->flags & PG_REFERENCED) || pmap_ts_referenced(m)) {
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
			vm_page_flag_set(m, PG_REFERENCED);
		}
		vm_page_unlock_queues();
	}
	return val;
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
		atomic_clear_32(&oldpmap->pm_active, PCPU_GET(cpumask));
	atomic_set_32(&pmap->pm_active, PCPU_GET(cpumask));
	pmap_asid_alloc(pmap);
	if (td == curthread) {
		PCPU_SET(segbase, pmap->pm_segtab);
		MachSetPID(pmap->pm_asid[PCPU_GET(cpuid)].asid);
	}
	PCPU_SET(curpmap, pmap);
	critical_exit();
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
	superpage_offset = offset & SEGOFSET;
	if (size - ((NBSEG - superpage_offset) & SEGOFSET) < NBSEG ||
	    (*addr & SEGOFSET) == superpage_offset)
		return;
	if ((*addr & SEGOFSET) < superpage_offset)
		*addr = (*addr & ~SEGOFSET) + superpage_offset;
	else
		*addr = ((*addr + SEGOFSET) & ~SEGOFSET) + superpage_offset;
}

int pmap_pid_dump(int pid);

int
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
			int i, j;

			printf("vmspace is %p\n",
			       p->p_vmspace);
			index = 0;
			pmap = vmspace_pmap(p->p_vmspace);
			printf("pmap asid:%x generation:%x\n",
			       pmap->pm_asid[0].asid,
			       pmap->pm_asid[0].gen);
			for (i = 0; i < NUSERPGTBLS; i++) {
				pd_entry_t *pde;
				pt_entry_t *pte;
				unsigned base = i << SEGSHIFT;

				pde = &pmap->pm_segtab[i];
				if (pde && pmap_pde_v(pde)) {
					for (j = 0; j < 1024; j++) {
						unsigned va = base +
						(j << PAGE_SHIFT);

						pte = pmap_pte(pmap, va);
						if (pte && pmap_pte_v(pte)) {
							vm_offset_t pa;
							vm_page_t m;

							pa = mips_tlbpfn_to_paddr(*pte);
							m = PHYS_TO_VM_PAGE(pa);
							printf("va: 0x%x, pt: 0x%x, h: %d, w: %d, f: 0x%x",
							    va, pa,
							    m->hold_count,
							    m->wire_count,
							    m->flags);
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
		} else {
		  printf("Process pid:%d has no vm_space\n", pid);
		}
		break;
	}
	sx_sunlock(&allproc_lock);
	return npte;
}


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
				if (pmap_pte_v(ptep))
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
			MIPS_TBIAP();
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

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW | PDB_TLBPID)) {
		if (curproc)
			printf("pmap_asid_alloc: curproc %d '%s' ",
			    curproc->p_pid, curproc->p_comm);
		else
			printf("pmap_asid_alloc: curproc <none> ");
		printf("segtab %p asid %d\n", pmap->pm_segtab,
		    pmap->pm_asid[PCPU_GET(cpuid)].asid);
	}
#endif
}

int
page_is_managed(vm_offset_t pa)
{
	vm_offset_t pgnum = mips_btop(pa);

	if (pgnum >= first_page && (pgnum < (first_page + vm_page_array_size))) {
		vm_page_t m;

		m = PHYS_TO_VM_PAGE(pa);
		if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0)
			return 1;
	}
	return 0;
}

static int
init_pte_prot(vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	int rw = 0;

	if (!(prot & VM_PROT_WRITE))
		rw = PTE_ROPAGE;
	else {
		if (va >= VM_MIN_KERNEL_ADDRESS) {
			/*
			 * Don't bother to trap on kernel writes, just
			 * record page as dirty.
			 */
			rw = PTE_RWPAGE;
			vm_page_dirty(m);
		} else if ((m->md.pv_flags & PV_TABLE_MOD) || m->dirty)
			rw = PTE_RWPAGE;
		else
			rw = PTE_CWPAGE;
	}
	return rw;
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
 *	pmap_set_modified:
 *
 *	Sets the page modified and reference bits for the specified page.
 */
void
pmap_set_modified(vm_offset_t pa)
{

	PHYS_TO_VM_PAGE(pa)->md.pv_flags |= (PV_TABLE_REF | PV_TABLE_MOD);
}

#include <machine/db_machdep.h>

/*
 *  Dump the translation buffer (TLB) in readable form.
 */

void
db_dump_tlb(int first, int last)
{
	struct tlb tlb;
	int tlbno;

	tlbno = first;

	while (tlbno <= last) {
		MachTLBRead(tlbno, &tlb);
		if (tlb.tlb_lo0 & PTE_V || tlb.tlb_lo1 & PTE_V) {
			printf("TLB %2d vad 0x%08x ", tlbno, (tlb.tlb_hi & 0xffffff00));
		} else {
			printf("TLB*%2d vad 0x%08x ", tlbno, (tlb.tlb_hi & 0xffffff00));
		}
		printf("0=0x%08x ", pfn_to_vad(tlb.tlb_lo0));
		printf("%c", tlb.tlb_lo0 & PTE_M ? 'M' : ' ');
		printf("%c", tlb.tlb_lo0 & PTE_G ? 'G' : ' ');
		printf(" atr %x ", (tlb.tlb_lo0 >> 3) & 7);
		printf("1=0x%08x ", pfn_to_vad(tlb.tlb_lo1));
		printf("%c", tlb.tlb_lo1 & PTE_M ? 'M' : ' ');
		printf("%c", tlb.tlb_lo1 & PTE_G ? 'G' : ' ');
		printf(" atr %x ", (tlb.tlb_lo1 >> 3) & 7);
		printf(" sz=%x pid=%x\n", tlb.tlb_mask,
		       (tlb.tlb_hi & 0x000000ff)
		       );
		tlbno++;
	}
}

#ifdef DDB
#include <sys/kernel.h>
#include <ddb/ddb.h>

DB_SHOW_COMMAND(tlb, ddb_dump_tlb)
{
	db_dump_tlb(0, num_tlbentries - 1);
}

#endif

/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		virtual address.
 */
 /* PMAP_INLINE */ vm_offset_t
pmap_kextract(vm_offset_t va)
{
	vm_offset_t pa = 0;

	if (va < MIPS_CACHED_MEMORY_ADDR) {
		/* user virtual address */
		pt_entry_t *ptep;

		if (curproc && curproc->p_vmspace) {
			ptep = pmap_pte(&curproc->p_vmspace->vm_pmap, va);
			if (ptep)
				pa = mips_tlbpfn_to_paddr(*ptep) |
				    (va & PAGE_MASK);
		}
	} else if (va >= MIPS_CACHED_MEMORY_ADDR &&
	    va < MIPS_UNCACHED_MEMORY_ADDR)
		pa = MIPS_CACHED_TO_PHYS(va);
	else if (va >= MIPS_UNCACHED_MEMORY_ADDR &&
	    va < MIPS_KSEG2_START)
		pa = MIPS_UNCACHED_TO_PHYS(va);
#ifdef VM_ALLOC_WIRED_TLB_PG_POOL
	else if (need_wired_tlb_page_pool && ((va >= VM_MIN_KERNEL_ADDRESS) &&
	    (va < (VM_MIN_KERNEL_ADDRESS + VM_KERNEL_ALLOC_OFFSET))))
		pa = MIPS_CACHED_TO_PHYS(va);
#endif
	else if (va >= MIPS_KSEG2_START && va < VM_MAX_KERNEL_ADDRESS) {
		pt_entry_t *ptep;

		if (kernel_pmap) {
			if (va >= (vm_offset_t)virtual_sys_start) {
				/* Its inside the virtual address range */
				ptep = pmap_pte(kernel_pmap, va);
				if (ptep)
					pa = mips_tlbpfn_to_paddr(*ptep) |
					    (va & PAGE_MASK);
			} else {
				int i;

				/*
				 * its inside the special mapping area, I
				 * don't think this should happen, but if it
				 * does I want it toa all work right :-)
				 * Note if it does happen, we assume the
				 * caller has the lock? FIXME, this needs to
				 * be checked FIXEM - RRS.
				 */
				for (i = 0; i < MAXCPU; i++) {
					if ((sysmap_lmem[i].valid1) && ((vm_offset_t)sysmap_lmem[i].CADDR1 == va)) {
						pa = mips_tlbpfn_to_paddr(sysmap_lmem[i].CMAP1);
						break;
					}
					if ((sysmap_lmem[i].valid2) && ((vm_offset_t)sysmap_lmem[i].CADDR2 == va)) {
						pa = mips_tlbpfn_to_paddr(sysmap_lmem[i].CMAP2);
						break;
					}
				}
			}
		}
	}
	return pa;
}
