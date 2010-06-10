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

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#if !defined(DIAGNOSTIC)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

#define	pmap_accessed(lpte)		((lpte)->pte & PTE_ACCESSED)
#define	pmap_dirty(lpte)		((lpte)->pte & PTE_DIRTY)
#define	pmap_exec(lpte)			((lpte)->pte & PTE_AR_RX)
#define	pmap_managed(lpte)		((lpte)->pte & PTE_MANAGED)
#define	pmap_ppn(lpte)			((lpte)->pte & PTE_PPN_MASK)
#define	pmap_present(lpte)		((lpte)->pte & PTE_PRESENT)
#define	pmap_prot(lpte)			(((lpte)->pte & PTE_PROT_MASK) >> 56)
#define	pmap_wired(lpte)		((lpte)->pte & PTE_WIRED)

#define	pmap_clear_accessed(lpte)	(lpte)->pte &= ~PTE_ACCESSED
#define	pmap_clear_dirty(lpte)		(lpte)->pte &= ~PTE_DIRTY
#define	pmap_clear_present(lpte)	(lpte)->pte &= ~PTE_PRESENT
#define	pmap_clear_wired(lpte)		(lpte)->pte &= ~PTE_WIRED

#define	pmap_set_wired(lpte)		(lpte)->pte |= PTE_WIRED

/*
 * The VHPT bucket head structure.
 */
struct ia64_bucket {
	uint64_t	chain;
	struct mtx	mutex;
	u_int		length;
};

/*
 * Statically allocated kernel pmap
 */
struct pmap kernel_pmap_store;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

/*
 * Kernel virtual memory management.
 */
static int nkpt;
struct ia64_lpte ***ia64_kptdir;
#define KPTE_DIR0_INDEX(va) \
	(((va) >> (3*PAGE_SHIFT-8)) & ((1<<(PAGE_SHIFT-3))-1))
#define KPTE_DIR1_INDEX(va) \
	(((va) >> (2*PAGE_SHIFT-5)) & ((1<<(PAGE_SHIFT-3))-1))
#define KPTE_PTE_INDEX(va) \
	(((va) >> PAGE_SHIFT) & ((1<<(PAGE_SHIFT-5))-1))
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

/*
 * Data for allocating PTEs for user processes.
 */
static uma_zone_t ptezone;

/*
 * Virtual Hash Page Table (VHPT) data.
 */
/* SYSCTL_DECL(_machdep); */
SYSCTL_NODE(_machdep, OID_AUTO, vhpt, CTLFLAG_RD, 0, "");

struct ia64_bucket *pmap_vhpt_bucket;

int pmap_vhpt_nbuckets;
SYSCTL_INT(_machdep_vhpt, OID_AUTO, nbuckets, CTLFLAG_RD,
    &pmap_vhpt_nbuckets, 0, "");

int pmap_vhpt_log2size = 0;
TUNABLE_INT("machdep.vhpt.log2size", &pmap_vhpt_log2size);
SYSCTL_INT(_machdep_vhpt, OID_AUTO, log2size, CTLFLAG_RD,
    &pmap_vhpt_log2size, 0, "");

static int pmap_vhpt_inserts;
SYSCTL_INT(_machdep_vhpt, OID_AUTO, inserts, CTLFLAG_RD,
    &pmap_vhpt_inserts, 0, "");

static int pmap_vhpt_population(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_machdep_vhpt, OID_AUTO, population, CTLTYPE_INT | CTLFLAG_RD,
    NULL, 0, pmap_vhpt_population, "I", "");

static struct ia64_lpte *pmap_find_vhpt(vm_offset_t va);

static PMAP_INLINE void	free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t locked_pmap);

static void	pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
		    vm_page_t m, vm_prot_t prot);
static void	pmap_free_pte(struct ia64_lpte *pte, vm_offset_t va);
static void	pmap_invalidate_all(void);
static int	pmap_remove_pte(pmap_t pmap, struct ia64_lpte *pte,
		    vm_offset_t va, pv_entry_t pv, int freepte);
static int	pmap_remove_vhpt(vm_offset_t va);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
		    vm_page_t m);

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

static void
pmap_initialize_vhpt(vm_offset_t vhpt)
{
	struct ia64_lpte *pte;
	u_int i;

	pte = (struct ia64_lpte *)vhpt;
	for (i = 0; i < pmap_vhpt_nbuckets; i++) {
		pte[i].pte = 0;
		pte[i].itir = 0;
		pte[i].tag = 1UL << 63; /* Invalid tag */
		pte[i].chain = (uintptr_t)(pmap_vhpt_bucket + i);
	}
}

#ifdef SMP
MALLOC_DECLARE(M_SMP);

vm_offset_t
pmap_alloc_vhpt(void)
{
	vm_offset_t vhpt;
	vm_size_t size;

	size = 1UL << pmap_vhpt_log2size;
	vhpt = (uintptr_t)contigmalloc(size, M_SMP, 0, 0UL, ~0UL, size, 0UL);
	if (vhpt != 0) {
		vhpt = IA64_PHYS_TO_RR7(ia64_tpa(vhpt));
		pmap_initialize_vhpt(vhpt);
	}
	return (vhpt);
}
#endif

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap()
{
	struct ia64_pal_result res;
	vm_offset_t base;
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
	nkpt = 0;
	kernel_vm_end = VM_MIN_KERNEL_ADDRESS - VM_GATEWAY_SIZE;

	for (i = 0; phys_avail[i+2]; i+= 2)
		;
	count = i+2;

	TUNABLE_INT_FETCH("machdep.vhpt.log2size", &pmap_vhpt_log2size);
	if (pmap_vhpt_log2size == 0)
		pmap_vhpt_log2size = 20;
	else if (pmap_vhpt_log2size < 15)
		pmap_vhpt_log2size = 15;
	else if (pmap_vhpt_log2size > 61)
		pmap_vhpt_log2size = 61;

	base = 0;
	size = 1UL << pmap_vhpt_log2size;
	for (i = 0; i < count; i += 2) {
		base = (phys_avail[i] + size - 1) & ~(size - 1);
		if (base + size <= phys_avail[i+1])
			break;
	}
	if (!phys_avail[i])
		panic("Unable to allocate VHPT");

	if (base != phys_avail[i]) {
		/* Split this region. */
		for (j = count; j > i; j -= 2) {
			phys_avail[j] = phys_avail[j-2];
			phys_avail[j+1] = phys_avail[j-2+1];
		}
		phys_avail[i+1] = base;
		phys_avail[i+2] = base + size;
	} else
		phys_avail[i] = base + size;

	base = IA64_PHYS_TO_RR7(base);
	PCPU_SET(md.vhpt, base);
	if (bootverbose)
		printf("VHPT: address=%#lx, size=%#lx\n", base, size);

	pmap_vhpt_nbuckets = size / sizeof(struct ia64_lpte);
	pmap_vhpt_bucket = (void *)pmap_steal_memory(pmap_vhpt_nbuckets *
	    sizeof(struct ia64_bucket));
	for (i = 0; i < pmap_vhpt_nbuckets; i++) {
		/* Stolen memory is zeroed. */
		mtx_init(&pmap_vhpt_bucket[i].mutex, "VHPT bucket lock", NULL,
		    MTX_NOWITNESS | MTX_SPIN);
	}

	pmap_initialize_vhpt(base);
	map_vhpt(base);
	ia64_set_pta(base + (1 << 8) + (pmap_vhpt_log2size << 2) + 1);
	ia64_srlz_i();

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	for (i = 0; i < 5; i++)
		kernel_pmap->pm_rid[i] = 0;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	PCPU_SET(md.current_pmap, kernel_pmap);

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
	ia64_srlz_d();

	/*
	 * Clear out any random TLB entries left over from booting.
	 */
	pmap_invalidate_all();

	map_gateway_page();
}

static int
pmap_vhpt_population(SYSCTL_HANDLER_ARGS)
{
	int count, error, i;

	count = 0;
	for (i = 0; i < pmap_vhpt_nbuckets; i++)
		count += pmap_vhpt_bucket[i].length;

	error = SYSCTL_OUT(req, &count, sizeof(count));
	return (error);
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
	int shpgperproc = PMAP_SHPGPERPROC;

	/*
	 * Initialize the address space (zone) for the pv entries.  Set a
	 * high water mark so that the system can recover from excessive
	 * numbers of pv entries.
	 */
	pvzone = uma_zcreate("PV ENTRY", sizeof(struct pv_entry), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM | UMA_ZONE_NOFREE);
	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + cnt.v_page_count;
	TUNABLE_INT_FETCH("vm.pmap.pv_entries", &pv_entry_max);
	pv_entry_high_water = 9 * (pv_entry_max / 10);

	ptezone = uma_zcreate("PT ENTRY", sizeof (struct ia64_lpte), 
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM|UMA_ZONE_NOFREE);
}


/***************************************************
 * Manipulate TLBs for a pmap
 ***************************************************/

static void
pmap_invalidate_page(vm_offset_t va)
{
	struct ia64_lpte *pte;
	struct pcpu *pc;
	uint64_t tag;
	u_int vhpt_ofs;

	critical_enter();
	vhpt_ofs = ia64_thash(va) - PCPU_GET(md.vhpt);
	tag = ia64_ttag(va);
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		pte = (struct ia64_lpte *)(pc->pc_md.vhpt + vhpt_ofs);
		atomic_cmpset_64(&pte->tag, tag, 1UL << 63);
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
pmap_invalidate_all(void)
{

#ifdef SMP
	if (mp_ncpus > 1) {
		smp_rendezvous(NULL, pmap_invalidate_all_1, NULL, NULL);
		return;
	}
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
int
pmap_pinit(struct pmap *pmap)
{
	int i;

	PMAP_LOCK_INIT(pmap);
	for (i = 0; i < 5; i++)
		pmap->pm_rid[i] = pmap_allocate_rid();
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	return (1);
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
	struct ia64_lpte **dir1;
	struct ia64_lpte *leaf;
	vm_page_t nkpg;

	while (kernel_vm_end <= addr) {
		if (nkpt == PAGE_SIZE/8 + PAGE_SIZE*PAGE_SIZE/64)
			panic("%s: out of kernel address space", __func__);

		dir1 = ia64_kptdir[KPTE_DIR0_INDEX(kernel_vm_end)];
		if (dir1 == NULL) {
			nkpg = vm_page_alloc(NULL, nkpt++,
			    VM_ALLOC_NOOBJ|VM_ALLOC_INTERRUPT|VM_ALLOC_WIRED);
			if (!nkpg)
				panic("%s: cannot add dir. page", __func__);

			dir1 = (struct ia64_lpte **) 
			    IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(nkpg));
			bzero(dir1, PAGE_SIZE);
			ia64_kptdir[KPTE_DIR0_INDEX(kernel_vm_end)] = dir1;
		}

		nkpg = vm_page_alloc(NULL, nkpt++,
		    VM_ALLOC_NOOBJ|VM_ALLOC_INTERRUPT|VM_ALLOC_WIRED);
		if (!nkpg)
			panic("%s: cannot add PTE page", __func__);

		leaf = (struct ia64_lpte *)
		    IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(nkpg));
		bzero(leaf, PAGE_SIZE);
		dir1[KPTE_DIR1_INDEX(kernel_vm_end)] = leaf;

		kernel_vm_end += PAGE_SIZE * NKPTEPG;
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
 */
static pv_entry_t
get_pv_entry(pmap_t locked_pmap)
{
	static const struct timeval printinterval = { 60, 0 };
	static struct timeval lastprint;
	struct vpgqueues *vpq;
	struct ia64_lpte *pte;
	pmap_t oldpmap, pmap;
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
			oldpmap = pmap_switch(pmap);
			pte = pmap_find_vhpt(va);
			KASSERT(pte != NULL, ("pte"));
			pmap_remove_vhpt(va);
			pmap_invalidate_page(va);
			pmap_switch(oldpmap);
			if (pmap_accessed(pte))
				vm_page_flag_set(m, PG_REFERENCED);
			if (pmap_dirty(pte))
				vm_page_dirty(m);
			pmap_free_pte(pte, va);
			TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
			m->md.pv_list_count--;
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
			if (pmap != locked_pmap)
				PMAP_UNLOCK(pmap);
			if (allocated_pv == NULL)
				allocated_pv = pv;
			else
				free_pv_entry(pv);
		}
		if (TAILQ_EMPTY(&m->md.pv_list))
			vm_page_flag_clear(m, PG_WRITEABLE);
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
 * Conditionally create a pv entry.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = uma_zalloc(pvzone, M_NOWAIT)) != NULL) {
		pv_entry_count++;
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count++;
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * Add an ia64_lpte to the VHPT.
 */
static void
pmap_enter_vhpt(struct ia64_lpte *pte, vm_offset_t va)
{
	struct ia64_bucket *bckt;
	struct ia64_lpte *vhpte;
	uint64_t pte_pa;

	/* Can fault, so get it out of the way. */
	pte_pa = ia64_tpa((vm_offset_t)pte);

	vhpte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)vhpte->chain;

	mtx_lock_spin(&bckt->mutex);
	pte->chain = bckt->chain;
	ia64_mf();
	bckt->chain = pte_pa;

	pmap_vhpt_inserts++;
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
	uint64_t chain, tag;

	tag = ia64_ttag(va);
	vhpte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)vhpte->chain;

	lpte = NULL;
	mtx_lock_spin(&bckt->mutex);
	chain = bckt->chain;
	pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(chain);
	while (chain != 0 && pte->tag != tag) {
		lpte = pte;
		chain = pte->chain;
		pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(chain);
	}
	if (chain == 0) {
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
	uint64_t chain, tag;

	tag = ia64_ttag(va);
	pte = (struct ia64_lpte *)ia64_thash(va);
	bckt = (struct ia64_bucket *)pte->chain;

	mtx_lock_spin(&bckt->mutex);
	chain = bckt->chain;
	pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(chain);
	while (chain != 0 && pte->tag != tag) {
		chain = pte->chain;
		pte = (struct ia64_lpte *)IA64_PHYS_TO_RR7(chain);
	}
	mtx_unlock_spin(&bckt->mutex);
	return ((chain != 0) ? pte : NULL);
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

	pv = get_pv_entry(pmap);
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
	oldpmap = pmap_switch(pmap);
	pte = pmap_find_vhpt(va);
	if (pte != NULL && pmap_present(pte))
		pa = pmap_ppn(pte);
	pmap_switch(oldpmap);
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
	vm_paddr_t pa;

	pa = 0;
	m = NULL;
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);
retry:
	pte = pmap_find_vhpt(va);
	if (pte != NULL && pmap_present(pte) &&
	    (pmap_prot(pte) & prot) == prot) {
		m = PHYS_TO_VM_PAGE(pmap_ppn(pte));
		if (vm_page_pa_tryrelock(pmap, pmap_ppn(pte), &pa))
			goto retry;
		vm_page_hold(m);
	}
	PA_UNLOCK_COND(pa);
	pmap_switch(oldpmap);
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
	struct ia64_lpte **dir1;
	struct ia64_lpte *leaf;

	KASSERT((va >> 61) == 5,
		("kernel mapping 0x%lx not in region 5", va));
	KASSERT(va < kernel_vm_end,
		("kernel mapping 0x%lx out of range", va));

	dir1 = ia64_kptdir[KPTE_DIR0_INDEX(va)];
	leaf = dir1[KPTE_DIR1_INDEX(va)];
	return (&leaf[KPTE_PTE_INDEX(va)]);
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
	if (pte == NULL) {
		pte = uma_zalloc(ptezone, M_NOWAIT | M_ZERO);
		pte->tag = 1UL << 63;
	}
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
	static long prot2ar[4] = {
		PTE_AR_R,		/* VM_PROT_NONE */
		PTE_AR_RW,		/* VM_PROT_WRITE */
		PTE_AR_RX|PTE_ED,	/* VM_PROT_EXECUTE */
		PTE_AR_RWX|PTE_ED	/* VM_PROT_WRITE|VM_PROT_EXECUTE */
	};

	pte->pte &= ~(PTE_PROT_MASK | PTE_PL_MASK | PTE_AR_MASK | PTE_ED);
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

	pte->pte &= PTE_PROT_MASK | PTE_PL_MASK | PTE_AR_MASK | PTE_ED;
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

	/*
	 * First remove from the VHPT.
	 */
	error = pmap_remove_vhpt(va);
	if (error)
		return (error);

	pmap_invalidate_page(va);

	if (pmap_wired(pte))
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	if (pmap_managed(pte)) {
		m = PHYS_TO_VM_PAGE(pmap_ppn(pte));
		if (pmap_dirty(pte))
			vm_page_dirty(m);
		if (pmap_accessed(pte))
			vm_page_flag_set(m, PG_REFERENCED);

		error = pmap_remove_entry(pmap, m, va, pv);
	}
	if (freepte)
		pmap_free_pte(pte, va);

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
	if (va >= kernel_vm_end)
		return (0);

	pte = pmap_find_kpte(va);
	if (!pmap_present(pte))
		return (0);
	return (pmap_ppn(pte) | (va & PAGE_MASK));
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
		if (pmap_present(pte))
			pmap_invalidate_page(va);
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
		if (pmap_present(pte)) {
			pmap_remove_vhpt(va);
			pmap_invalidate_page(va);
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
	if (pmap_present(pte))
		pmap_invalidate_page(va);
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
	if (pmap_present(pte)) {
		pmap_remove_vhpt(va);
		pmap_invalidate_page(va);
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
	oldpmap = pmap_switch(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pte = pmap_find_vhpt(sva);
		if (pte != NULL)
			pmap_remove_pte(pmap, pte, sva, 0, 1);
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
		for (va = sva; va < eva; va += PAGE_SIZE) {
			pte = pmap_find_vhpt(va);
			if (pte != NULL)
				pmap_remove_pte(pmap, pte, va, 0, 1);
		}
	}

out:
	vm_page_unlock_queues();
	pmap_switch(oldpmap);
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

	KASSERT((m->flags & PG_FICTITIOUS) == 0,
	    ("pmap_remove_all: page %p is fictitious", m));
	vm_page_lock_queues();
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		struct ia64_lpte *pte;
		pmap_t pmap = pv->pv_pmap;
		vm_offset_t va = pv->pv_va;

		PMAP_LOCK(pmap);
		oldpmap = pmap_switch(pmap);
		pte = pmap_find_vhpt(va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_ppn(pte) != VM_PAGE_TO_PHYS(m))
			panic("pmap_remove_all: pv_table for %lx is inconsistent", VM_PAGE_TO_PHYS(m));
		pmap_remove_pte(pmap, pte, va, pv, 1);
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pmap);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
	vm_page_unlock_queues();
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

	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;

	if ((sva & PAGE_MASK) || (eva & PAGE_MASK))
		panic("pmap_protect: unaligned addresses");

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);
	for ( ; sva < eva; sva += PAGE_SIZE) {
		/* If page is invalid, skip this page */
		pte = pmap_find_vhpt(sva);
		if (pte == NULL)
			continue;

		/* If there's no change, skip it too */
		if (pmap_prot(pte) == prot)
			continue;

		if ((prot & VM_PROT_WRITE) == 0 &&
		    pmap_managed(pte) && pmap_dirty(pte)) {
			vm_paddr_t pa = pmap_ppn(pte);
			vm_page_t m = PHYS_TO_VM_PAGE(pa);

			vm_page_dirty(m);
			pmap_clear_dirty(pte);
		}

		if (prot & VM_PROT_EXECUTE)
			ia64_sync_icache(sva, PAGE_SIZE);

		pmap_pte_prot(pmap, pte, prot);
		pmap_invalidate_page(sva);
	}
	vm_page_unlock_queues();
	pmap_switch(oldpmap);
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
	pmap_t oldpmap;
	vm_offset_t pa;
	vm_offset_t opa;
	struct ia64_lpte origpte;
	struct ia64_lpte *pte;
	boolean_t icache_inval, managed;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);

	va &= ~PAGE_MASK;
 	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT((m->oflags & VPO_BUSY) != 0,
	    ("pmap_enter: page %p is not busy", m));

	/*
	 * Find (or create) a pte for the given mapping.
	 */
	while ((pte = pmap_find_pte(va)) == NULL) {
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pmap);
		vm_page_unlock_queues();
		VM_WAIT;
		vm_page_lock_queues();
		PMAP_LOCK(pmap);
		oldpmap = pmap_switch(pmap);
	}
	origpte = *pte;
	if (!pmap_present(pte)) {
		opa = ~0UL;
		pmap_enter_vhpt(pte, va);
	} else
		opa = pmap_ppn(pte);
	managed = FALSE;
	pa = VM_PAGE_TO_PHYS(m);

	icache_inval = (prot & VM_PROT_EXECUTE) ? TRUE : FALSE;

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
		if (wired && !pmap_wired(&origpte))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_wired(&origpte))
			pmap->pm_stats.wired_count--;

		managed = (pmap_managed(&origpte)) ? TRUE : FALSE;

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status. Otherwise,
		 * we can avoid I-cache invalidation if the page
		 * already allowed execution.
		 */
		if (managed && pmap_dirty(&origpte))
			vm_page_dirty(m);
		else if (pmap_exec(&origpte))
			icache_inval = FALSE;

		pmap_invalidate_page(va);
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
		KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva,
		    ("pmap_enter: managed mapping within the clean submap"));
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

	/* Invalidate the I-cache when needed. */
	if (icache_inval)
		ia64_sync_icache(va, PAGE_SIZE);

	if ((prot & VM_PROT_WRITE) != 0 && managed)
		vm_page_flag_set(m, PG_WRITEABLE);
	vm_page_unlock_queues();
	pmap_switch(oldpmap);
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
	pmap_t oldpmap;
	vm_page_t m;
	vm_pindex_t diff, psize;

	VM_OBJECT_LOCK_ASSERT(m_start->object, MA_OWNED);
	psize = atop(end - start);
	m = m_start;
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		pmap_enter_quick_locked(pmap, start + ptoa(diff), m, prot);
		m = TAILQ_NEXT(m, listq);
	}
	vm_page_unlock_queues();
	pmap_switch(oldpmap);
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
	pmap_t oldpmap;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);
	pmap_enter_quick_locked(pmap, va, m, prot);
	vm_page_unlock_queues();
	pmap_switch(oldpmap);
	PMAP_UNLOCK(pmap);
}

static void
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot)
{
	struct ia64_lpte *pte;
	boolean_t managed;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	if ((pte = pmap_find_pte(va)) == NULL)
		return;

	if (!pmap_present(pte)) {
		/* Enter on the PV list if the page is managed. */
		if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
			if (!pmap_try_insert_pv_entry(pmap, va, m)) {
				pmap_free_pte(pte, va);
				return;
			}
			managed = TRUE;
		} else
			managed = FALSE;

		/* Increment counters. */
		pmap->pm_stats.resident_count++;

		/* Initialise with R/O protection and enter into VHPT. */
		pmap_enter_vhpt(pte, va);
		pmap_pte_prot(pmap, pte,
		    prot & (VM_PROT_READ | VM_PROT_EXECUTE));
		pmap_set_pte(pte, va, VM_PAGE_TO_PHYS(m), FALSE, managed);

		if (prot & VM_PROT_EXECUTE)
			ia64_sync_icache(va, PAGE_SIZE);
	}
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
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	pmap_t oldpmap;
	struct ia64_lpte *pte;

	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);

	pte = pmap_find_vhpt(va);
	KASSERT(pte != NULL, ("pte"));
	if (wired && !pmap_wired(pte)) {
		pmap->pm_stats.wired_count++;
		pmap_set_wired(pte);
	} else if (!wired && pmap_wired(pte)) {
		pmap->pm_stats.wired_count--;
		pmap_clear_wired(pte);
	}

	pmap_switch(oldpmap);
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
 *	pmap_page_wired_mappings:
 *
 *	Return the number of managed mappings to the given physical page
 *	that are wired.
 */
int
pmap_page_wired_mappings(vm_page_t m)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap, pmap;
	pv_entry_t pv;
	int count;

	count = 0;
	if ((m->flags & PG_FICTITIOUS) != 0)
		return (count);
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap = pv->pv_pmap;
		PMAP_LOCK(pmap);
		oldpmap = pmap_switch(pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_wired(pte))
			count++;
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pmap);
	}
	vm_page_unlock_queues();
	return (count);
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
	pmap_t oldpmap;
	pv_entry_t pv, npv;

	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)) {
		printf("warning: %s called with non-current pmap\n",
		    __func__);
		return;
	}

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	oldpmap = pmap_switch(pmap);

	for (pv = TAILQ_FIRST(&pmap->pm_pvlist); pv; pv = npv) {
		struct ia64_lpte *pte;

		npv = TAILQ_NEXT(pv, pv_plist);

		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (!pmap_wired(pte))
			pmap_remove_pte(pmap, pte, pv->pv_va, pv, 1);
	}

	pmap_switch(oldpmap);
	PMAP_UNLOCK(pmap);
	vm_page_unlock_queues();
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

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_switch(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_accessed(pte)) {
			count++;
			pmap_clear_accessed(pte);
			pmap_invalidate_page(pv->pv_va);
		}
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
	}
	vm_page_unlock_queues();
	return (count);
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;
	boolean_t rv;

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_is_modified: page %p is not managed", m));
	rv = FALSE;

	/*
	 * If the page is not VPO_BUSY, then PG_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PG_WRITEABLE
	 * is clear, no PTEs can be dirty.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->flags & PG_WRITEABLE) == 0)
		return (rv);
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_switch(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		pmap_switch(oldpmap);
		KASSERT(pte != NULL, ("pte"));
		rv = pmap_dirty(pte) ? TRUE : FALSE;
		PMAP_UNLOCK(pv->pv_pmap);
		if (rv)
			break;
	}
	vm_page_unlock_queues();
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
	if (pte != NULL && pmap_present(pte))
		return (FALSE);
	return (TRUE);
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
	struct ia64_lpte *pte;
	pmap_t oldpmap;
	pv_entry_t pv;
	boolean_t rv;

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	rv = FALSE;
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_switch(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		pmap_switch(oldpmap);
		KASSERT(pte != NULL, ("pte"));
		rv = pmap_accessed(pte) ? TRUE : FALSE;
		PMAP_UNLOCK(pv->pv_pmap);
		if (rv)
			break;
	}
	vm_page_unlock_queues();
	return (rv);
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

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("pmap_clear_modify: page %p is busy", m));

	/*
	 * If the page is not PG_WRITEABLE, then no PTEs can be modified.
	 * If the object containing the page is locked and the page is not
	 * VPO_BUSY, then PG_WRITEABLE cannot be concurrently set.
	 */
	if ((m->flags & PG_WRITEABLE) == 0)
		return;
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_switch(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_dirty(pte)) {
			pmap_clear_dirty(pte);
			pmap_invalidate_page(pv->pv_va);
		}
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
	}
	vm_page_unlock_queues();
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

	KASSERT((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0,
	    ("pmap_clear_reference: page %p is not managed", m));
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		PMAP_LOCK(pv->pv_pmap);
		oldpmap = pmap_switch(pv->pv_pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		if (pmap_accessed(pte)) {
			pmap_clear_accessed(pte);
			pmap_invalidate_page(pv->pv_va);
		}
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pv->pv_pmap);
	}
	vm_page_unlock_queues();
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap, pmap;
	pv_entry_t pv;
	vm_prot_t prot;

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
	vm_page_lock_queues();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap = pv->pv_pmap;
		PMAP_LOCK(pmap);
		oldpmap = pmap_switch(pmap);
		pte = pmap_find_vhpt(pv->pv_va);
		KASSERT(pte != NULL, ("pte"));
		prot = pmap_prot(pte);
		if ((prot & VM_PROT_WRITE) != 0) {
			if (pmap_dirty(pte)) {
				vm_page_dirty(m);
				pmap_clear_dirty(pte);
			}
			prot &= ~VM_PROT_WRITE;
			pmap_pte_prot(pmap, pte, prot);
			pmap_invalidate_page(pv->pv_va);
		}
		pmap_switch(oldpmap);
		PMAP_UNLOCK(pmap);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
	vm_page_unlock_queues();
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
	vm_offset_t va;

	va = pa | IA64_RR_BASE(6);
	return ((void *)va);
}

/*
 * 'Unmap' a range mapped by pmap_mapdev().
 */
void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pmap_t oldpmap;
	struct ia64_lpte *pte, tpte;
	vm_paddr_t pa;
	int val;
	
	PMAP_LOCK(pmap);
retry:
	oldpmap = pmap_switch(pmap);
	pte = pmap_find_vhpt(addr);
	if (pte != NULL) {
		tpte = *pte;
		pte = &tpte;
	}
	pmap_switch(oldpmap);
	if (pte == NULL || !pmap_present(pte)) {
		val = 0;
		goto out;
	}
	val = MINCORE_INCORE;
	if (pmap_dirty(pte))
		val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
	if (pmap_accessed(pte))
		val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) &&
	    pmap_managed(pte)) {
		pa = pmap_ppn(pte);
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
	pmap_switch(vmspace_pmap(td->td_proc->p_vmspace));
}

pmap_t
pmap_switch(pmap_t pm)
{
	pmap_t prevpm;
	int i;

	critical_enter();
	prevpm = PCPU_GET(md.current_pmap);
	if (prevpm == pm)
		goto out;
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
	}
	PCPU_SET(md.current_pmap, pm);
	ia64_srlz_d();

out:
	critical_exit();
	return (prevpm);
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	pmap_t oldpm;
	struct ia64_lpte *pte;
	vm_offset_t lim;
	vm_size_t len;

	sz += va & 31;
	va &= ~31;
	sz = (sz + 31) & ~31;

	PMAP_LOCK(pm);
	oldpm = pmap_switch(pm);
	while (sz > 0) {
		lim = round_page(va);
		len = MIN(lim - va, sz);
		pte = pmap_find_vhpt(va);
		if (pte != NULL && pmap_present(pte))
			ia64_sync_icache(va, len);
		va += len;
		sz -= len;
	}
	pmap_switch(oldpm);
	PMAP_UNLOCK(pm);
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
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
		    (pmap_dirty(&buf)) ? 1 : 0,
		    (pmap_accessed(&buf)) ? 1 : 0,
		    manames[(buf.pte & PTE_MA_MASK) >> 2],
		    (pmap_present(&buf)) ? 1 : 0,
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
	pte = pmap_find_kpte(addr);
	db_printf("kpte at %p:\n", pte);
	db_printf("  pte  =%016lx\n", pte->pte);
	db_printf("  itir =%016lx\n", pte->itir);
	db_printf("  tag  =%016lx\n", pte->tag);
	db_printf("  chain=%016lx\n", pte->chain);
}

#endif
