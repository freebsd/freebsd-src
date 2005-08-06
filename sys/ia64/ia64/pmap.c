/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 1998,2000 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>

#include <machine/md_var.h>
#include <machine/pal.h>

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
 * Following the Linux model, region IDs are allocated in groups of
 * eight so that a single region ID can be used for as many RRs as we
 * want by encoding the RR number into the low bits of the ID.
 *
 * We reserve region ID 0 for the kernel and allocate the remaining
 * IDs for user pmaps.
 *
 * Region 0..4
 *	User virtually mapped
 *
 * Region 5
 *	Kernel virtually mapped
 *
 * Region 6
 *	Kernel physically mapped uncacheable
 *
 * Region 7
 *	Kernel physically mapped cacheable
 */

/* XXX move to a header. */
extern uint64_t ia64_gateway_page[];

MALLOC_DEFINE(M_PMAP, "PMAP", "PMAP Structures");

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#define MINPV 2048	/* Preallocate at least this many */

#if !defined(DIAGNOSTIC)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

#define	pmap_lpte_accessed(lpte)	((lpte)->pte & PTE_ACCESSED)
#define	pmap_lpte_dirty(lpte)		((lpte)->pte & PTE_DIRTY)
#define	pmap_lpte_managed(lpte)		((lpte)->pte & PTE_MANAGED)
#define	pmap_lpte_ppn(lpte)		((lpte)->pte & PTE_PPN_MASK)
#define	pmap_lpte_present(lpte)		((lpte)->pte & PTE_PRESENT)
#define	pmap_lpte_prot(lpte)		(((lpte)->pte & PTE_PROT_MASK) >> 56)
#define	pmap_lpte_wired(lpte)		((lpte)->pte & PTE_WIRED)

#define	pmap_clear_accessed(lpte)	(lpte)->pte &= ~PTE_ACCESSED
#define	pmap_clear_dirty(lpte)		(lpte)->pte &= ~PTE_DIRTY
#define	pmap_clear_present(lpte)	(lpte)->pte &= ~PTE_PRESENT
#define	pmap_clear_wired(lpte)		(lpte)->pte &= ~PTE_WIRED

#define	pmap_set_wired(lpte)		(lpte)->pte |= PTE_WIRED

/*
 * Statically allocated kernel pmap
 */
struct pmap kernel_pmap_store;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

struct ia64_bucket {
	uint64_t	chain;
	struct mtx	mutex;
	u_int		length;
};

struct ia64_bucket *vhpt_bucket;
uint64_t vhpt_base[MAXCPU];
size_t vhpt_size;

/*
 * Kernel virtual memory management.
 */
static int nkpt;
struct ia64_lpte **ia64_kptdir;
#define KPTE_DIR_INDEX(va) \
	((va >> (2*PAGE_SHIFT-5)) & ((1<<(PAGE_SHIFT-3))-1))
#define KPTE_PTE_INDEX(va) \
	((va >> PAGE_SHIFT) & ((1<<(PAGE_SHIFT-5))-1))
#define NKPTEPG		(PAGE_SIZE / sizeof(struct ia64_lpte))

vm_offset_t kernel_vm_end;

/* Values for ptc.e. XXX values for SKI. */
static uint64_t pmap_ptc_e_base = 0x100000000;
static uint64_t pmap_ptc_e_count1 = 3;
static uint64_t pmap_ptc_e_count2 = 2;
static uint64_t pmap_ptc_e_stride1 = 0x2000;
static uint64_t pmap_ptc_e_stride2 = 0x100000000;
struct mtx pmap_ptcmutex;

/*
 * Data for the RID allocator
 */
static int pmap_ridcount;
static int pmap_rididx;
static int pmap_ridmapsz;
static int pmap_ridmax;
static uint64_t *pmap_ridmap;
struct mtx pmap_ridmutex;

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
int pmap_pagedaemon_waken;

/*
 * Data for allocating PTEs for user processes.
 */
static uma_zone_t ptezone;

/*
 * VHPT instrumentation.
 */
static int pmap_vhpt_inserts;
static int pmap_vhpt_resident;
SYSCTL_DECL(_vm_stats);
SYSCTL_NODE(_vm_stats, OID_AUTO, vhpt, CTLFLAG_RD, 0, "");
SYSCTL_INT(_vm_stats_vhpt, OID_AUTO, inserts, CTLFLAG_RD,
	   &pmap_vhpt_inserts, 0, "");
SYSCTL_INT(_vm_stats_vhpt, OID_AUTO, resident, CTLFLAG_RD,
	   &pmap_vhpt_resident, 0, "");

static PMAP_INLINE void	free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(void);

static pmap_t	pmap_install(pmap_t);
static void	pmap_invalidate_all(pmap_t pmap);

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

	va = IA64_PHYS_TO_RR7(pa);
	bzero((caddr_t) va, size);
	return va;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap()
{
	struct ia64_pal_result res;
	struct ia64_lpte *pte;
	vm_offset_t base, limit;
	size_t size;
	int i, j, count, ridbits;

	/*
	 * Query the PAL Code to find the loop parameters for the
	 * ptc.e instruction.
	 */
	res = ia64_call_pal_static(PAL_PTCE_INFO, 0, 0, 0);
	if (res.pal_status != 0)
		panic("Can't configure ptc.e parameters");
	pmap_ptc_e_base = res.pal_result[0];
	pmap_ptc_e_count1 = res.pal_result[1] >> 32;
	pmap_ptc_e_count2 = res.pal_result[1] & ((1L<<32) - 1);
	pmap_ptc_e_stride1 = res.pal_result[2] >> 32;
	pmap_ptc_e_stride2 = res.pal_result[2] & ((1L<<32) - 1);
	if (bootverbose)
		printf("ptc.e base=0x%lx, count1=%ld, count2=%ld, "
		       "stride1=0x%lx, stride2=0x%lx\n",
		       pmap_ptc_e_base,
		       pmap_ptc_e_count1,
		       pmap_ptc_e_count2,
		       pmap_ptc_e_stride1,
		       pmap_ptc_e_stride2);
	mtx_init(&pmap_ptcmutex, "Global PTC lock", NULL, MTX_SPIN);

	/*
	 * Setup RIDs. RIDs 0..7 are reserved for the kernel.
	 *
	 * We currently need at least 19 bits in the RID because PID_MAX
	 * can only be encoded in 17 bits and we need RIDs for 5 regions
	 * per process. With PID_MAX equalling 99999 this means that we
	 * need to be able to encode 499995 (=5*PID_MAX).
	 * The Itanium processor only has 18 bits and the architected
	 * minimum is exactly that. So, we cannot use a PID based scheme
	 * in those cases. Enter pmap_ridmap...
	 * We should avoid the map when running on a processor that has
	 * implemented enough bits. This means that we should pass the
	 * process/thread ID to pmap. This we currently don't do, so we
	 * use the map anyway. However, we don't want to allocate a map
	 * that is large enough to cover the range dictated by the number
	 * of bits in the RID, because that may result in a RID map of
	 * 2MB in size for a 24-bit RID. A 64KB map is enough.
	 * The bottomline: we create a 32KB map when the processor only
	 * implements 18 bits (or when we can't figure it out). Otherwise
	 * we create a 64KB map.
	 */
	res = ia64_call_pal_static(PAL_VM_SUMMARY, 0, 0, 0);
	if (res.pal_status != 0) {
		if (bootverbose)
			printf("Can't read VM Summary - assuming 18 Region ID bits\n");
		ridbits = 18; /* guaranteed minimum */
	} else {
		ridbits = (res.pal_result[1] >> 8) & 0xff;
		if (bootverbose)
			printf("Processor supports %d Region ID bits\n",
			    ridbits);
	}
	if (ridbits > 19)
		ridbits = 19;

	pmap_ridmax = (1 << ridbits);
	pmap_ridmapsz = pmap_ridmax / 64;
	pmap_ridmap = (uint64_t *)pmap_steal_memory(pmap_ridmax / 8);
	pmap_ridmap[0] |= 0xff;
	pmap_rididx = 0;
	pmap_ridcount = 8;
	mtx_init(&pmap_ridmutex, "RID allocator lock", NULL, MTX_DEF);

	/*
	 * Allocate some memory for initial kernel 'page tables'.
	 */
	ia64_kptdir = (void *)pmap_steal_memory(PAGE_SIZE);
	for (i = 0; i < NKPT; i++) {
		ia64_kptdir[i] = (void*)pmap_steal_memory(PAGE_SIZE);
	}
	nkpt = NKPT;
	kernel_vm_end = NKPT * PAGE_SIZE * NKPTEPG + VM_MIN_KERNEL_ADDRESS -
	    VM_GATEWAY_SIZE;

	for (i = 0; phys_avail[i+2]; i+= 2)
		;
	count = i+2;

	/*
	 * Figure out a useful size for the VHPT, based on the size of
	 * physical memory and try to locate a region which is large
	 * enough to contain the VHPT (which must be a power of two in
	 * size and aligned to a natural boundary).
	 */
	vhpt_size = 15;
	size = 1UL << vhpt_size;
	while (size < Maxmem * 32) {
		vhpt_size++;
		size <<= 1;
	}

	vhpt_base[0] = 0;
	base = limit = 0;
	while (vhpt_base[0] == 0) {
		if (bootverbose)
			printf("Trying VHPT size 0x%lx\n", size);
		for (i = 0; i < count; i += 2) {
			base = (phys_avail[i] + size - 1) & ~(size - 1);
			limit = base + MAXCPU * size;
			if (limit <= phys_avail[i+1])
				/*
				 * VHPT can fit in this region
				 */
				break;
		}
		if (!phys_avail[i]) {
			/* Can't fit, try next smaller size. */
			vhpt_size--;
			size >>= 1;
		} else
			vhpt_base[0] = IA64_PHYS_TO_RR7(base);
	}
	if (vhpt_size < 15)
		panic("Can't find space for VHPT");

	if (bootverbose)
		printf("Putting VHPT at 0x%lx\n", base);

	if (base != phys_avail[i]) {
		/* Split this region. */
		if (bootverbose)
			printf("Splitting [%p-%p]\n", (void *)phys_avail[i],
			    (void *)phys_avail[i+1]);
		for (j = count; j > i; j -= 2) {
			phys_avail[j] = phys_avail[j-2];
			phys_avail[j+1] = phys_avail[j-2+1];
		}
		phys_avail[i+1] = base;
		phys_avail[i+2] = limit;
	} else
		phys_avail[i] = limit;

	count = size / sizeof(struct ia64_lpte);

	vhpt_bucket = (void *)pmap_steal_memory(count * sizeof(struct ia64_bucket));
	pte = (struct ia64_lpte *)vhpt_base[0];
	for (i = 0; i < count; i++) {
		pte[i].pte = 0;
		pte[i].itir = 0;
		pte[i].tag = 1UL << 63;	/* Invalid tag */
		pte[i].chain = (uintptr_t)(vhpt_bucket + i);
		/* Stolen memory is zeroed! */
		mtx_init(&vhpt_bucket[i].mutex, "VHPT bucket lock", NULL,
		    MTX_SPIN);
	}

	for (i = 1; i < MAXCPU; i++) {
		vhpt_base[i] = vhpt_base[i - 1] + size;
		bcopy((void *)vhpt_base[i - 1], (void *)vhpt_base[i], size);
	}

	__asm __volatile("mov cr.pta=%0;; srlz.i;;" ::
	    "r" (vhpt_base[0] + (1<<8) + (vhpt_size<<2) + 1));

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	for (i = 0; i < 5; i++)
		kernel_pmap->pm_rid[i] = 0;
	kernel_pmap->pm_active = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	PCPU_SET(current_pmap, kernel_pmap);

	/*
	 * Region 5 is mapped via the vhpt.
	 */
	ia64_set_rr(IA64_RR_BASE(5),
		    (5 << 8) | (PAGE_SHIFT << 2) | 1);

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest 
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (IA64_ID_PAGE_SHIFT << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (IA64_ID_PAGE_SHIFT << 2));

	/*
	 * Clear out any random TLB entries left over from booting.
	 */
	pmap_invalidate_all(kernel_pmap);

	map_gateway_page();
}

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_list_count = 0;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{

	/*
	 * Init the pv free list and the PTE free list.
	 */
	pvzone = uma_zcreate("PV ENTRY", sizeof (struct pv_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM|UMA_ZONE_NOFREE);
	uma_prealloc(pvzone, MINPV);

	ptezone = uma_zcreate("PT ENTRY", sizeof (struct ia64_lpte), 
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM|UMA_ZONE_NOFREE);
	uma_prealloc(ptezone, MINPV);
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
}


/***************************************************
 * Manipulate TLBs for a pmap
 ***************************************************/

#if 0
static __inline void
pmap_invalidate_page_locally(void *arg)
{
	vm_offset_t va = (uintptr_t)arg;
	struct ia64_lpte *pte;

	pte = (struct ia64_lpte *)ia64_thash(va);
	if (pte->tag == ia64_ttag(va))
		pte->tag = 1UL << 63;
	ia64_ptc_l(va, PAGE_SHIFT << 2);
}

#ifdef SMP
static void
pmap_invalidate_page_1(void *arg)
{
	void **args = arg;
	pmap_t oldpmap;

	critical_enter();
	oldpmap = pmap_install(args[0]);
	pmap_invalidate_page_locally(args[1]);
	pmap_install(oldpmap);
	critical_exit();
}
#endif

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("invalidating TLB for non-current pmap"));

#ifdef SMP
	if (mp_ncpus > 1) {
		void *args[2];
		args[0] = pmap;
		args[1] = (void *)va;
		smp_rendezvous(NULL, pmap_invalidate_page_1, NULL, args);
	} else
#endif
	pmap_invalidate_page_locally((void *)va);
}
#endif /* 0 */

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	struct ia64_lpte *pte;
	int i, vhpt_ofs;

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("invalidating TLB for non-current pmap"));

	vhpt_ofs = ia64_thash(va) - vhpt_base[PCPU_GET(cpuid)];
	critical_enter();
	for (i = 0; i < MAXCPU; i++) {
		pte = (struct ia64_lpte *)(vhpt_base[i] + vhpt_ofs);
		if (pte->tag == ia64_ttag(va))
			pte->tag = 1UL << 63;
	}
	critical_exit();
	mtx_lock_spin(&pmap_ptcmutex);
	ia64_ptc_ga(va, PAGE_SHIFT << 2);
	mtx_unlock_spin(&pmap_ptcmutex);
}

static void
pmap_invalidate_all_1(void *arg)
{
	uint64_t addr;
	int i, j;

	critical_enter();
	addr = pmap_ptc_e_base;
	for (i = 0; i < pmap_ptc_e_count1; i++) {
		for (j = 0; j < pmap_ptc_e_count2; j++) {
			ia64_ptc_e(addr);
			addr += pmap_ptc_e_stride2;
		}
		addr += pmap_ptc_e_stride1;
	}
	critical_exit();
}

static void
pmap_invalidate_all(pmap_t pmap)
{

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("invalidating TLB for non-current pmap"));

#ifdef SMP
	if (mp_ncpus > 1)
		smp_rendezvous(NULL, pmap_invalidate_all_1, NULL, NULL);
	else
#endif
	pmap_invalidate_all_1(NULL);
}

static uint32_t
pmap_allocate_rid(void)
{
	uint64_t bit, bits;
	int rid;

	mtx_lock(&pmap_ridmutex);
	if (pmap_ridcount == pmap_ridmax)
		panic("pmap_allocate_rid: All Region IDs used");

	/* Find an index with a free bit. */
	while ((bits = pmap_ridmap[pmap_rididx]) == ~0UL) {
		pmap_rididx++;
		if (pmap_rididx == pmap_ridmapsz)
			pmap_rididx = 0;
	}
	rid = pmap_rididx * 64;

	/* Find a free bit. */
	bit = 1UL;
	while (bits & bit) {
		rid++;
		bit <<= 1;
	}

	pmap_ridmap[pmap_rididx] |= bit;
	pmap_ridcount++;
	mtx_unlock(&pmap_ridmutex);

	return rid;
}

static void
pmap_free_rid(uint32_t rid)
{
	uint64_t bit;
	int idx;

	idx = rid / 64;
	bit = ~(1UL << (rid & 63));

	mtx_lock(&pmap_ridmutex);
	pmap_ridmap[idx] &= bit;
	pmap_ridcount--;
	mtx_unlock(&pmap_ridmutex);
}

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

/***************************************************
 * Page table page management routines.....
 ***************************************************/

void
pmap_pinit0(struct pmap *pmap)
{
	/* kernel_pmap is the same as any other pmap. */
	pmap_pinit(pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(struct pmap *pmap)
{
	int i;

	PMAP_LOCK_INIT(pmap);
	for (i = 0; i < 5; i++)
		pmap->pm_rid[i] = pmap_allocate_rid();
	pmap->pm_active = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
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
	int i;

	for (i = 0; i < 5; i++)
		if (pmap->pm_rid[i])
			pmap_free_rid(pmap->pm_rid[i]);
	PMAP_LOCK_DESTROY(pmap);
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	struct ia64_lpte *ptepage;
	vm_page_t nkpg;

	if (kernel_vm_end >= addr)
		return;

	critical_enter();

	while (kernel_vm_end < addr) {
		/* We could handle more by increasing the size of kptdir. */
		if (nkpt == MAXKPT)
			panic("pmap_growkernel: out of kernel address space");

		nkpg = vm_page_alloc(NULL, nkpt,
		    VM_ALLOC_NOOBJ | VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");

		ptepage = (struct ia64_lpte *)
		    IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(nkpg));
		bzero(ptepage, PAGE_SIZE);
		ia64_kptdir[KPTE_DIR_INDEX(kernel_vm_end)] = ptepage;

		nkpt++;
		kernel_vm_end += PAGE_SIZE * NKPTEPG;
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
 * Add an ia64_lpte to the VHPT.
 */
static void
pmap_enter_vhpt(struct ia64_lpte *pte, vm_offset_t va)
{
	struct ia64_bucket *bckt;
	struct ia64_lpte *vhpte;

	pmap_vhpt_inserts++;
	pmap_vhpt_resident++;

	vhpte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)vhpte->chain;

	mtx_lock_spin(&bckt->mutex);
	pte->chain = bckt->chain;
	ia64_mf();
	bckt->chain = ia64_tpa((vm_offset_t)pte);
	ia64_mf();

	bckt->length++;
	mtx_unlock_spin(&bckt->mutex);
}

/*
 * Remove the ia64_lpte matching va from the VHPT. Return zero if it
 * worked or an appropriate error code otherwise.
 */
static int
pmap_remove_vhpt(vm_offset_t va)
{
	struct ia64_bucket *bckt;
	struct ia64_lpte *pte;
	struct ia64_lpte *lpte;
	struct ia64_lpte *vhpte;
	uint64_t tag;

	tag = ia64_ttag(va);
	vhpte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)vhpte->chain;

	mtx_lock_spin(&bckt->mutex);
	lpte = NULL;
	pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(bckt->chain);
	while (pte != NULL && pte->tag != tag) {
		lpte = pte;
		pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(pte->chain);
	}
	if (pte == NULL) {
		mtx_unlock_spin(&bckt->mutex);
		return (ENOENT);
	}

	/* Snip this pv_entry out of the collision chain. */
	if (lpte == NULL)
		bckt->chain = pte->chain;
	else
		lpte->chain = pte->chain;
	ia64_mf();

	bckt->length--;
	mtx_unlock_spin(&bckt->mutex);
	pmap_vhpt_resident--;
	return (0);
}

/*
 * Find the ia64_lpte for the given va, if any.
 */
static struct ia64_lpte *
pmap_find_vhpt(vm_offset_t va)
{
	struct ia64_bucket *bckt;
	struct ia64_lpte *pte;
	uint64_t tag;

	tag = ia64_ttag(va);
	pte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)pte->chain;
	if (bckt->chain == 0)
		return (NULL);

	pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(bckt->chain);
	while (pte->tag != tag) {
		if (pte->chain == 0)
			return (NULL);
		pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(pte->chain);
	}
	return (pte);
}

/*
 * Remove an entry from the list of managed mappings.
 */
static int
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va, pv_entry_t pv)
{
	if (!pv) {
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
	}

	if (pv) {
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		if (TAILQ_FIRST(&m->md.pv_list) == NULL)
			vm_page_flag_clear(m, PG_WRITEABLE);

		TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
		free_pv_entry(pv);
		return 0;
	} else {
		return ENOENT;
	}
}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	pv = get_pv_entry();
	pv->pv_pmap = pmap;
	pv->pv_va = va;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	vm_paddr_t pa;

	pa = 0;
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);
	pte = pmap_find_vhpt(va);
	if (pte != NULL && pmap_lpte_present(pte))
		pa = pmap_lpte_ppn(pte);
	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);
	return (pa);
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	vm_page_t m;

	m = NULL;
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);
	pte = pmap_find_vhpt(va);
	if (pte != NULL && pmap_lpte_present(pte) &&
	    (pmap_lpte_prot(pte) & prot) == prot) {
		m = PHYS_TO_VM_PAGE(pmap_lpte_ppn(pte));
		vm_page_hold(m);
	}
	vm_page_unlock_queues();
	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);
	return (m);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Find the kernel lpte for mapping the given virtual address, which
 * must be in the part of region 5 which we can cover with our kernel
 * 'page tables'.
 */
static struct ia64_lpte *
pmap_find_kpte(vm_offset_t va)
{
	KASSERT((va >> 61) == 5,
		("kernel mapping 0x%lx not in region 5", va));
	KASSERT(IA64_RR_MASK(va) < (nkpt * PAGE_SIZE * NKPTEPG),
		("kernel mapping 0x%lx out of range", va));
	return (&ia64_kptdir[KPTE_DIR_INDEX(va)][KPTE_PTE_INDEX(va)]);
}

/*
 * Find a pte suitable for mapping a user-space address. If one exists 
 * in the VHPT, that one will be returned, otherwise a new pte is
 * allocated.
 */
static struct ia64_lpte *
pmap_find_pte(vm_offset_t va)
{
	struct ia64_lpte *pte;

	if (va >= VM_MAXUSER_ADDRESS)
		return pmap_find_kpte(va);

	pte = pmap_find_vhpt(va);
	if (!pte)
		pte = uma_zalloc(ptezone, M_NOWAIT | M_ZERO);
	return (pte);
}

/*
 * Free a pte which is now unused. This simply returns it to the zone
 * allocator if it is a user mapping. For kernel mappings, clear the
 * valid bit to make it clear that the mapping is not currently used.
 */
static void
pmap_free_pte(struct ia64_lpte *pte, vm_offset_t va)
{
	if (va < VM_MAXUSER_ADDRESS)
		uma_zfree(ptezone, pte);
	else
		pmap_clear_present(pte);
}

static PMAP_INLINE void
pmap_pte_prot(pmap_t pm, struct ia64_lpte *pte, vm_prot_t prot)
{
	static int prot2ar[4] = {
		PTE_AR_R,	/* VM_PROT_NONE */
		PTE_AR_RW,	/* VM_PROT_WRITE */
		PTE_AR_RX,	/* VM_PROT_EXECUTE */
		PTE_AR_RWX	/* VM_PROT_WRITE|VM_PROT_EXECUTE */
	};

	pte->pte &= ~(PTE_PROT_MASK | PTE_PL_MASK | PTE_AR_MASK);
	pte->pte |= (uint64_t)(prot & VM_PROT_ALL) << 56;
	pte->pte |= (prot == VM_PROT_NONE || pm == kernel_pmap)
	    ? PTE_PL_KERN : PTE_PL_USER;
	pte->pte |= prot2ar[(prot & VM_PROT_ALL) >> 1];
}

/*
 * Set a pte to contain a valid mapping and enter it in the VHPT. If
 * the pte was orginally valid, then its assumed to already be in the
 * VHPT.
 * This functions does not set the protection bits.  It's expected
 * that those have been set correctly prior to calling this function.
 */
static void
pmap_set_pte(struct ia64_lpte *pte, vm_offset_t va, vm_offset_t pa,
    boolean_t wired, boolean_t managed)
{

	pte->pte &= PTE_PROT_MASK | PTE_PL_MASK | PTE_AR_MASK;
	pte->pte |= PTE_PRESENT | PTE_MA_WB;
	pte->pte |= (managed) ? PTE_MANAGED : (PTE_DIRTY | PTE_ACCESSED);
	pte->pte |= (wired) ? PTE_WIRED : 0;
	pte->pte |= pa & PTE_PPN_MASK;

	pte->itir = PAGE_SHIFT << 2;

	pte->tag = ia64_ttag(va);
}

/*
 * Remove the (possibly managed) mapping represented by pte from the
 * given pmap.
 */
static int
pmap_remove_pte(pmap_t pmap, struct ia64_lpte *pte, vm_offset_t va,
		pv_entry_t pv, int freepte)
{
	int error;
	vm_page_t m;

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("removing pte for non-current pmap"));

	/*
	 * First remove from the VHPT.
	 */
	error = pmap_remove_vhpt(va);
	if (error)
		return (error);

	if (freepte)
		pmap_invalidate_page(pmap, va);

	if (pmap_lpte_wired(pte))
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	if (pmap_lpte_managed(pte)) {
		m = PHYS_TO_VM_PAGE(pmap_lpte_ppn(pte));
		if (pmap_lpte_dirty(pte))
			if (pmap_track_modified(va))
				vm_page_dirty(m);
		if (pmap_lpte_accessed(pte))
			vm_page_flag_set(m, PG_REFERENCED);

		if (freepte)
			pmap_free_pte(pte, va);
		error = pmap_remove_entry(pmap, m, va, pv);
	} else {
		if (freepte)
			pmap_free_pte(pte, va);
	}

	return (error);
}

/*
 * Extract the physical page address associated with a kernel
 * virtual address.
 */
vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	struct ia64_lpte *pte;
	vm_offset_t gwpage;

	KASSERT(va >= IA64_RR_BASE(5), ("Must be kernel VA"));

	/* Regions 6 and 7 are direct mapped. */
	if (va >= IA64_RR_BASE(6))
		return (IA64_RR_MASK(va));

	/* EPC gateway page? */
	gwpage = (vm_offset_t)ia64_get_k5();
	if (va >= gwpage && va < gwpage + VM_GATEWAY_SIZE)
		return (IA64_RR_MASK((vm_offset_t)ia64_gateway_page));

	/* Bail out if the virtual address is beyond our limits. */
	if (IA64_RR_MASK(va) >= nkpt * PAGE_SIZE * NKPTEPG)
		return (0);

	pte = pmap_find_kpte(va);
	if (!pmap_lpte_present(pte))
		return (0);
	return (pmap_lpte_ppn(pte) | (va & PAGE_MASK));
}

/*
 * Add a list of wired pages to the kva this routine is only used for
 * temporary kernel mappings that do not need to have page modification
 * or references recorded.  Note that old mappings are simply written
 * over.  The page is effectively wired, but it's customary to not have
 * the PTE reflect that, nor update statistics.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	struct ia64_lpte *pte;
	int i;

	for (i = 0; i < count; i++) {
		pte = pmap_find_kpte(va);
		if (pmap_lpte_present(pte))
			pmap_invalidate_page(kernel_pmap, va);
		else
			pmap_enter_vhpt(pte, va);
		pmap_pte_prot(kernel_pmap, pte, VM_PROT_ALL);
		pmap_set_pte(pte, va, VM_PAGE_TO_PHYS(m[i]), FALSE, FALSE);
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
	struct ia64_lpte *pte;
	int i;

	for (i = 0; i < count; i++) {
		pte = pmap_find_kpte(va);
		if (pmap_lpte_present(pte)) {
			pmap_remove_vhpt(va);
			pmap_invalidate_page(kernel_pmap, va);
			pmap_clear_present(pte);
		}
		va += PAGE_SIZE;
	}
}

/*
 * Add a wired page to the kva.  As for pmap_qenter(), it's customary
 * to not have the PTE reflect that, nor update statistics.
 */
void 
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	struct ia64_lpte *pte;

	pte = pmap_find_kpte(va);
	if (pmap_lpte_present(pte))
		pmap_invalidate_page(kernel_pmap, va);
	else
		pmap_enter_vhpt(pte, va);
	pmap_pte_prot(kernel_pmap, pte, VM_PROT_ALL);
	pmap_set_pte(pte, va, pa, FALSE, FALSE);
}

/*
 * Remove a page from the kva
 */
void
pmap_kremove(vm_offset_t va)
{
	struct ia64_lpte *pte;

	pte = pmap_find_kpte(va);
	if (pmap_lpte_present(pte)) {
		pmap_remove_vhpt(va);
		pmap_invalidate_page(kernel_pmap, va);
		pmap_clear_present(pte);
	}
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
	return IA64_PHYS_TO_RR7(start);
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va)
{
	struct ia64_lpte *pte;

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("removing page for non-current pmap"));

	pte = pmap_find_vhpt(va);
	if (pte)
		pmap_remove_pte(pmap, pte, va, 0, 1);
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
	pmap_t oldpmap;
	vm_offset_t va;
	pv_entry_t npv, pv;
	struct ia64_lpte *pte;

	if (pmap->pm_stats.resident_count == 0)
		return;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pmap_remove_page(pmap, sva);
		goto out;
	}

	if (pmap->pm_stats.resident_count < ((eva - sva) >> PAGE_SHIFT)) {
		TAILQ_FOREACH_SAFE(pv, &pmap->pm_pvlist, pv_plist, npv) {
			va = pv->pv_va;
			if (va >= sva && va < eva) {
				pte = pmap_find_vhpt(va);
				KASSERT(pte != NULL, ("pte"));
				pmap_remove_pte(pmap, pte, va, pv, 1);
			}
		}
		
	} else {
		for (va = sva; va < eva; va = va += PAGE_SIZE) {
			pte = pmap_find_vhpt(va);
			if (pte)
				pmap_remove_pte(pmap, pte, va, 0, 1);
		}
	}
out:
	vm_page_unlock_queues();
	pmap_install(oldpmap);
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
	pmap_t oldpmap;
	pv_entry_t pv;
	int s;

#if defined(DIAGNOSTIC)
	/*
	 * XXX this makes pmap_page_protect(NONE) illegal for non-managed
	 * pages!
	 */
	if (m->flags & PG_FICTITIOUS) {
		panic("pmap_page_protect: illegal for unmanaged page, va: 0x%lx", VM_PAGE_TO_PHYS(m));
	}
#endif

	s = splvm();

	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		struct ia64_lpte *pte;
		pmap_t pmap = pv->pv_pmap;
		vm_offset_t va = pv->pv_va;

		PMAP_LOCK(pmap);
		oldpmap = pmap_install(pmap);
		pte = pmap_find_vhpt(va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_lpte_ppn(pte) != VM_PAGE_TO_PHYS(m))
			panic("pmap_remove_all: pv_table for %lx is inconsistent", VM_PAGE_TO_PHYS(m));
		pmap_remove_pte(pmap, pte, va, pv, 1);
		pmap_install(oldpmap);
		PMAP_UNLOCK(pmap);
	}

	vm_page_flag_clear(m, PG_WRITEABLE);

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
	pmap_t oldpmap;
	struct ia64_lpte *pte;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	if ((sva & PAGE_MASK) || (eva & PAGE_MASK))
		panic("pmap_protect: unaligned addresses");

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);
	while (sva < eva) {
		/* 
		 * If page is invalid, skip this page
		 */
		pte = pmap_find_vhpt(sva);
		if (!pte) {
			sva += PAGE_SIZE;
			continue;
		}

		if (pmap_lpte_prot(pte) != prot) {
			if (pmap_lpte_managed(pte)) {
				vm_offset_t pa = pmap_lpte_ppn(pte);
				vm_page_t m = PHYS_TO_VM_PAGE(pa);
				if (pmap_lpte_dirty(pte)) {
					if (pmap_track_modified(sva))
						vm_page_dirty(m);
					pmap_clear_dirty(pte);
				}
				if (pmap_lpte_accessed(pte)) {
					vm_page_flag_set(m, PG_REFERENCED);
					pmap_clear_accessed(pte);
				}
			}
			pmap_pte_prot(pmap, pte, prot);
			pmap_invalidate_page(pmap, sva);
		}

		sva += PAGE_SIZE;
	}
	vm_page_unlock_queues();
	pmap_install(oldpmap);
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
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    boolean_t wired)
{
	pmap_t oldpmap;
	vm_offset_t pa;
	vm_offset_t opa;
	struct ia64_lpte origpte;
	struct ia64_lpte *pte;
	boolean_t managed;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);

	va &= ~PAGE_MASK;
#ifdef DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
#endif

	/*
	 * Find (or create) a pte for the given mapping.
	 */
	while ((pte = pmap_find_pte(va)) == NULL) {
		pmap_install(oldpmap);
		PMAP_UNLOCK(pmap);
		vm_page_unlock_queues();
		VM_WAIT;
		vm_page_lock_queues();
		PMAP_LOCK(pmap);
		oldpmap = pmap_install(pmap);
	}
	origpte = *pte;
	if (!pmap_lpte_present(pte)) {
		opa = ~0UL;
		pmap_enter_vhpt(pte, va);
	} else
		opa = pmap_lpte_ppn(pte);
	managed = FALSE;
	pa = VM_PAGE_TO_PHYS(m);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && !pmap_lpte_wired(&origpte))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_lpte_wired(&origpte))
			pmap->pm_stats.wired_count--;

		managed = (pmap_lpte_managed(&origpte)) ? TRUE : FALSE;

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (managed && pmap_lpte_dirty(&origpte) &&
		    pmap_track_modified(va))
			vm_page_dirty(m);

		pmap_invalidate_page(pmap, va);
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall
	 * through to handle validating new mapping.
	 */
	if (opa != ~0UL) {
		pmap_remove_pte(pmap, pte, va, 0, 0);
		pmap_enter_vhpt(pte, va);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
		pmap_insert_entry(pmap, va, m);
		managed = TRUE;
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:

	/*
	 * Now validate mapping with desired protection/wiring. This
	 * adds the pte to the VHPT if necessary.
	 */
	pmap_pte_prot(pmap, pte, prot);
	pmap_set_pte(pte, va, pa, wired, managed);

	vm_page_unlock_queues();
	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * 6. Page IS managed.
 * but is *MUCH* faster than pmap_enter...
 */

vm_page_t
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_page_t mpte)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	boolean_t managed;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);

	while ((pte = pmap_find_pte(va)) == NULL) {
		pmap_install(oldpmap);
		PMAP_UNLOCK(pmap);
		vm_page_busy(m);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(m->object);
		VM_WAIT;
		VM_OBJECT_LOCK(m->object);
		vm_page_lock_queues();
		vm_page_wakeup(m);
		PMAP_LOCK(pmap);
		oldpmap = pmap_install(pmap);
	}

	if (!pmap_lpte_present(pte)) {
		/* Enter on the PV list if its managed. */
		if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
			pmap_insert_entry(pmap, va, m);
			managed = TRUE;
		} else
			managed = FALSE;

		/* Increment counters. */
		pmap->pm_stats.resident_count++;

		/* Initialise with R/O protection and enter into VHPT. */
		pmap_enter_vhpt(pte, va);
		pmap_pte_prot(pmap, pte, VM_PROT_READ);
		pmap_set_pte(pte, va, VM_PAGE_TO_PHYS(m), FALSE, managed);
	}

	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);
	return (NULL);
}

/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size)
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
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	pmap_t oldpmap;
	struct ia64_lpte *pte;

	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);

	pte = pmap_find_vhpt(va);
	KASSERT(pte != NULL, ("pte"));
	if (wired && !pmap_lpte_wired(pte)) {
		pmap->pm_stats.wired_count++;
		pmap_set_wired(pte);
	} else if (!wired && pmap_lpte_wired(pte)) {
		pmap->pm_stats.wired_count--;
		pmap_clear_wired(pte);
	}

	pmap_install(oldpmap);
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
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
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
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
	bzero((char *)(caddr_t)va + off, size);
}


/*
 *	pmap_zero_page_idle zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.  This is for the vm_idlezero process.
 */

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
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
	vm_offset_t src = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(mdst));
	bcopy((caddr_t) src, (caddr_t) dst, PAGE_SIZE);
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
	int s;

	if (m->flags & PG_FICTITIOUS)
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

/*
 * Remove all pages from specified address space
 * this aids process exit speeds.  Also, this code
 * is special cased for current process only, but
 * can have the more generic (and slightly slower)
 * mode enabled.  This is much faster than pmap_remove
 * in the case of running down an entire address space.
 */
void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pmap_t oldpmap;
	pv_entry_t pv, npv;

	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);

	for (pv = TAILQ_FIRST(&pmap->pm_pvlist); pv; pv = npv) {
		struct ia64_lpte *pte;

		npv = TAILQ_NEXT(pv, pv_plist);

		if (pv->pv_va >= eva || pv->pv_va < sva)
			continue;

		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (!pmap_lpte_wired(pte))
			pmap_remove_pte(pmap, pte, pv->pv_va, pv, 1);
	}

	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);
	vm_page_unlock_queues();
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap, pmap;
	pv_entry_t pv;

	if ((prot & VM_PROT_WRITE) != 0)
		return;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		if ((m->flags & PG_WRITEABLE) == 0)
			return;
		TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
			pmap = pv->pv_pmap;
			PMAP_LOCK(pmap);
			oldpmap = pmap_install(pmap);
			pte = pmap_find_vhpt(pv->pv_va);
			KASSERT(pte != NULL, ("pte"));
			pmap_pte_prot(pmap, pte, prot);
			pmap_invalidate_page(pmap, pv->pv_va);
			pmap_install(oldpmap);
			PMAP_UNLOCK(pmap);
		}
		vm_page_flag_clear(m, PG_WRITEABLE);
	} else {
		pmap_remove_all(m);
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;
	int count = 0;

	if (m->flags & PG_FICTITIOUS)
		return 0;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_install(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_lpte_accessed(pte)) {
			count++;
			pmap_clear_accessed(pte);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
	}

	return count;
}

#if 0
/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
static boolean_t
pmap_is_referenced(vm_page_t m)
{
	pv_entry_t pv;

	if (m->flags & PG_FICTITIOUS)
		return FALSE;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte = pmap_find_vhpt(pv->pv_va);
		pmap_install(oldpmap);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_lpte_accessed(pte))
			return 1;
	}

	return 0;
}
#endif

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_page_t m)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;
	boolean_t rv;

	rv = FALSE;
	if (m->flags & PG_FICTITIOUS)
		return (rv);

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_install(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		pmap_install(oldpmap);
		KASSERT(pte != NULL, ("pte"));
		rv = pmap_lpte_dirty(pte) ? TRUE : FALSE;
		PMAP_UNLOCK(pv->pv_pmap);
		if (rv)
			break;
	}

	return (rv);
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	struct ia64_lpte *pte;

	pte = pmap_find_vhpt(addr);
	if (pte && pmap_lpte_present(pte))
		return (FALSE);
	return (TRUE);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;

	if (m->flags & PG_FICTITIOUS)
		return;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_install(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_lpte_dirty(pte)) {
			pmap_clear_dirty(pte);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;

	if (m->flags & PG_FICTITIOUS)
		return;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_install(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_lpte_accessed(pte)) {
			pmap_clear_accessed(pte);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
	}
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
	return (void*) IA64_PHYS_TO_RR6(pa);
}

/*
 * 'Unmap' a range mapped by pmap_mapdev().
 */
void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	return;
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	pmap_t oldpmap;
	struct ia64_lpte *pte, tpte;
	int val = 0;
	
	PMAP_LOCK(pmap);
	oldpmap = pmap_install(pmap);
	pte = pmap_find_vhpt(addr);
	if (pte != NULL) {
		tpte = *pte;
		pte = &tpte;
	}
	pmap_install(oldpmap);
	PMAP_UNLOCK(pmap);

	if (!pte)
		return 0;

	if (pmap_lpte_present(pte)) {
		vm_page_t m;
		vm_offset_t pa;

		val = MINCORE_INCORE;
		if (!pmap_lpte_managed(pte))
			return val;

		pa = pmap_lpte_ppn(pte);

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if (pmap_lpte_dirty(pte))
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		else {
			/*
			 * Modified by someone
			 */
			vm_page_lock_queues();
			if (pmap_is_modified(m))
				val |= MINCORE_MODIFIED_OTHER;
			vm_page_unlock_queues();
		}
		/*
		 * Referenced by us
		 */
		if (pmap_lpte_accessed(pte))
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;
		else {
			/*
			 * Referenced by someone
			 */
			vm_page_lock_queues();
			if (pmap_ts_referenced(m)) {
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
	pmap_install(vmspace_pmap(td->td_proc->p_vmspace));
}

pmap_t
pmap_switch(pmap_t pm)
{
	pmap_t prevpm;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);

	prevpm = PCPU_GET(current_pmap);
	if (prevpm == pm)
		return (prevpm);
	if (prevpm != NULL)
		atomic_clear_32(&prevpm->pm_active, PCPU_GET(cpumask));
	if (pm == NULL) {
		for (i = 0; i < 5; i++) {
			ia64_set_rr(IA64_RR_BASE(i),
			    (i << 8)|(PAGE_SHIFT << 2)|1);
		}
	} else {
		for (i = 0; i < 5; i++) {
			ia64_set_rr(IA64_RR_BASE(i),
			    (pm->pm_rid[i] << 8)|(PAGE_SHIFT << 2)|1);
		}
		atomic_set_32(&pm->pm_active, PCPU_GET(cpumask));
	}
	PCPU_SET(current_pmap, pm);
	__asm __volatile("srlz.d");
	return (prevpm);
}

static pmap_t
pmap_install(pmap_t pm)
{
	pmap_t prevpm;

	mtx_lock_spin(&sched_lock);
	prevpm = pmap_switch(pm);
	mtx_unlock_spin(&sched_lock);
	return (prevpm);
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{

	return addr;
}

#include "opt_ddb.h"

#ifdef DDB

#include <ddb/ddb.h>

static const char*	psnames[] = {
	"1B",	"2B",	"4B",	"8B",
	"16B",	"32B",	"64B",	"128B",
	"256B",	"512B",	"1K",	"2K",
	"4K",	"8K",	"16K",	"32K",
	"64K",	"128K",	"256K",	"512K",
	"1M",	"2M",	"4M",	"8M",
	"16M",	"32M",	"64M",	"128M",
	"256M",	"512M",	"1G",	"2G"
};

static void
print_trs(int type)
{
	struct ia64_pal_result res;
	int i, maxtr;
	struct {
		pt_entry_t	pte;
		uint64_t	itir;
		uint64_t	ifa;
		struct ia64_rr	rr;
	} buf;
	static const char *manames[] = {
		"WB",	"bad",	"bad",	"bad",
		"UC",	"UCE",	"WC",	"NaT",
	};

	res = ia64_call_pal_static(PAL_VM_SUMMARY, 0, 0, 0);
	if (res.pal_status != 0) {
		db_printf("Can't get VM summary\n");
		return;
	}

	if (type == 0)
		maxtr = (res.pal_result[0] >> 40) & 0xff;
	else
		maxtr = (res.pal_result[0] >> 32) & 0xff;

	db_printf("V RID    Virtual Page  Physical Page PgSz ED AR PL D A MA  P KEY\n");
	for (i = 0; i <= maxtr; i++) {
		bzero(&buf, sizeof(buf));
		res = ia64_call_pal_stacked_physical
			(PAL_VM_TR_READ, i, type, ia64_tpa((uint64_t) &buf));
		if (!(res.pal_result[0] & 1))
			buf.pte &= ~PTE_AR_MASK;
		if (!(res.pal_result[0] & 2))
			buf.pte &= ~PTE_PL_MASK;
		if (!(res.pal_result[0] & 4))
			pmap_clear_dirty(&buf);
		if (!(res.pal_result[0] & 8))
			buf.pte &= ~PTE_MA_MASK;
		db_printf("%d %06x %013lx %013lx %4s %d  %d  %d  %d %d %-3s "
		    "%d %06x\n", (int)buf.ifa & 1, buf.rr.rr_rid,
		    buf.ifa >> 12, (buf.pte & PTE_PPN_MASK) >> 12,
		    psnames[(buf.itir & ITIR_PS_MASK) >> 2],
		    (buf.pte & PTE_ED) ? 1 : 0,
		    (int)(buf.pte & PTE_AR_MASK) >> 9,
		    (int)(buf.pte & PTE_PL_MASK) >> 7,
		    (pmap_lpte_dirty(&buf)) ? 1 : 0,
		    (pmap_lpte_accessed(&buf)) ? 1 : 0,
		    manames[(buf.pte & PTE_MA_MASK) >> 2],
		    (pmap_lpte_present(&buf)) ? 1 : 0,
		    (int)((buf.itir & ITIR_KEY_MASK) >> 8));
	}
}

DB_COMMAND(itr, db_itr)
{
	print_trs(0);
}

DB_COMMAND(dtr, db_dtr)
{
	print_trs(1);
}

DB_COMMAND(rr, db_rr)
{
	int i;
	uint64_t t;
	struct ia64_rr rr;

	printf("RR RID    PgSz VE\n");
	for (i = 0; i < 8; i++) {
		__asm __volatile ("mov %0=rr[%1]"
				  : "=r"(t)
				  : "r"(IA64_RR_BASE(i)));
		*(uint64_t *) &rr = t;
		printf("%d  %06x %4s %d\n",
		       i, rr.rr_rid, psnames[rr.rr_ps], rr.rr_ve);
	}
}

DB_COMMAND(thash, db_thash)
{
	if (!have_addr)
		return;

	db_printf("%p\n", (void *) ia64_thash(addr));
}

DB_COMMAND(ttag, db_ttag)
{
	if (!have_addr)
		return;

	db_printf("0x%lx\n", ia64_ttag(addr));
}

DB_COMMAND(kpte, db_kpte)
{
	struct ia64_lpte *pte;

	if (!have_addr) {
		db_printf("usage: kpte <kva>\n");
		return;
	}
	if (addr < VM_MIN_KERNEL_ADDRESS) {
		db_printf("kpte: error: invalid <kva>\n");
		return;
	}
	pte = &ia64_kptdir[KPTE_DIR_INDEX(addr)][KPTE_PTE_INDEX(addr)];
	db_printf("kpte at %p:\n", pte);
	db_printf("  pte  =%016lx\n", pte->pte);
	db_printf("  itir =%016lx\n", pte->itir);
	db_printf("  tag  =%016lx\n", pte->tag);
	db_printf("  chain=%016lx\n", pte->chain);
}

#endif
