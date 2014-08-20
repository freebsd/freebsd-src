/*-
 * Copyright (c) 2014 Andrew Turner
 * All rights reserved.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>

#include <machine/devmap.h>
#include <machine/machdep.h>
#include <machine/pcb.h>
#include <machine/vmparam.h>

/*
#define PMAP_DEBUG
*/

#if !defined(DIAGNOSTIC)
#ifdef __GNUC_GNU_INLINE__
#define PMAP_INLINE	__attribute__((__gnu_inline__)) inline
#else
#define PMAP_INLINE	extern inline
#endif
#else
#define PMAP_INLINE
#endif

/*
 * These are configured by the mair_el1 register. This is set up in locore.S
 */
#define	DEVICE_MEMORY	0
#define	UNCACHED_MEMORY	1
#define	CACHED_MEMORY	2

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t kernel_vm_end = 0;
vm_offset_t vm_max_kernel_address;

int unmapped_buf_allowed = 0;

struct pmap kernel_pmap_store;

struct msgbuf *msgbufp = NULL;

#define	pmap_l1_index(va)	(((va) >> L1_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l2_index(va)	(((va) >> L2_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l3_index(va)	(((va) >> L3_SHIFT) & Ln_ADDR_MASK)

static pd_entry_t *
pmap_l1(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_l1[pmap_l1_index(va)]);
}

static pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = (pd_entry_t *)PHYS_TO_DMAP(*l1 & ~ATTR_MASK);
	return (&l2[pmap_l2_index(va)]);
}

static pd_entry_t *
pmap_l2(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = pmap_l1(pmap, va);
	if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
		return (NULL);

	return (pmap_l1_to_l2(l1, va));
}

static pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
	pt_entry_t *l3;

	l3 = (pd_entry_t *)PHYS_TO_DMAP(*l2 & ~ATTR_MASK);
	return (&l3[pmap_l3_index(va)]);
}

static pt_entry_t *
pmap_l3(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = pmap_l2(pmap, va);
	if (l2 == NULL || (*l2 & ATTR_DESCR_MASK) != L2_TABLE)
		return (NULL);

	return (pmap_l2_to_l3(l2, va));
}


static pt_entry_t *
pmap_early_page_idx(vm_offset_t l1pt, vm_offset_t va, u_int *l1_slot,
    u_int *l2_slot)
{
	pt_entry_t *l2;
	pd_entry_t *l1;

	l1 = (pd_entry_t *)l1pt;
	*l1_slot = (va >> L1_SHIFT) & Ln_ADDR_MASK;

	/* Check locore has used a table L1 map */
	KASSERT((l1[*l1_slot] & ATTR_DESCR_MASK) == L1_TABLE,
	   ("Invalid bootstrap L1 table"));
	/* Find the address of the L2 table */
	l2 = (pt_entry_t *)PHYS_TO_DMAP(l1[*l1_slot] & ~ATTR_MASK);
	*l2_slot = pmap_l2_index(va);

	return (l2);
}

static vm_paddr_t
pmap_early_vtophys(vm_offset_t l1pt, vm_offset_t va)
{
	u_int l1_slot, l2_slot;
	pt_entry_t *l2;

	l2 = pmap_early_page_idx(l1pt, va, &l1_slot, &l2_slot);

	return ((l2[l2_slot] & ~ATTR_MASK) + (va & L2_OFFSET));
}

static void
pmap_bootstrap_dmap(vm_offset_t l1pt)
{
	vm_offset_t va;
	vm_paddr_t pa;
	pd_entry_t *l1;
	u_int l1_slot;

	va = DMAP_MIN_ADDRESS;
	l1 = (pd_entry_t *)l1pt;
	l1_slot = pmap_l1_index(DMAP_MIN_ADDRESS);

	for (pa = 0; va < DMAP_MAX_ADDRESS;
	    pa += L1_SIZE, va += L1_SIZE, l1_slot++) {
		KASSERT(l1_slot < Ln_ENTRIES, ("Invalid L1 index"));

		/*
		 * TODO: Turn the cache on here when we have cache
		 * flushing code.
		 */
		l1[l1_slot] = (pa & ~L1_OFFSET) | ATTR_AF | L1_BLOCK |
		    ATTR_IDX(UNCACHED_MEMORY);
	}
}

static vm_offset_t
pmap_bootstrap_l2(vm_offset_t l1pt, vm_offset_t va, vm_offset_t l2_start)
{
	vm_offset_t l2pt;
	vm_paddr_t pa;
	pd_entry_t *l1;
	u_int l1_slot;

	KASSERT((va & L1_OFFSET) == 0, ("Invalid virtual address"));

	l1 = (pd_entry_t *)l1pt;
	l1_slot = pmap_l1_index(va);
	l2pt = l2_start;

	for (; va < VM_MAX_KERNEL_ADDRESS; l1_slot++, va += L1_SIZE) {
		KASSERT(l1_slot < Ln_ENTRIES, ("Invalid L1 index"));

		pa = pmap_early_vtophys(l1pt, l2pt);
		l1[l1_slot] = (pa & ~Ln_TABLE_MASK) | ATTR_AF | L1_TABLE;
		l2pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l2_start, 0, l2pt - l2_start);

	return l2pt;
}

static vm_offset_t
pmap_bootstrap_l3(vm_offset_t l1pt, vm_offset_t va, vm_offset_t l3_start)
{
	vm_offset_t l2pt, l3pt;
	vm_paddr_t pa;
	pd_entry_t *l2;
	u_int l2_slot;

	KASSERT((va & L2_OFFSET) == 0, ("Invalid virtual address"));

	l2 = pmap_l2(kernel_pmap, va);
	l2 = (pd_entry_t *)((uintptr_t)l2 & ~(PAGE_SIZE - 1));
	l2pt = (vm_offset_t)l2;
	l2_slot = pmap_l2_index(va);
	l3pt = l3_start;

	for (; va < VM_MAX_KERNEL_ADDRESS; l2_slot++, va += L2_SIZE) {
		KASSERT(l2_slot < Ln_ENTRIES, ("Invalid L2 index"));

		pa = pmap_early_vtophys(l1pt, l3pt);
		l2[l2_slot] = (pa & ~Ln_TABLE_MASK) | ATTR_AF | L2_TABLE;
		l3pt += PAGE_SIZE;
	}

	/* Clean the L2 page table */
	memset((void *)l3_start, 0, l3pt - l3_start);

	return l3pt;
}

void
pmap_bootstrap(vm_offset_t l1pt, vm_paddr_t kernstart, vm_size_t kernlen)
{
	u_int l1_slot, l2_slot, avail_slot, map_slot, used_map_slot;
	uint64_t kern_delta;
	pt_entry_t *l2;
	vm_offset_t va, freemempos;
	vm_offset_t dpcpu;
	vm_paddr_t pa;

	kern_delta = KERNBASE - kernstart;
	physmem = 0;

	printf("pmap_bootstrap %llx %llx %llx\n", l1pt, kernstart, kernlen);
	printf("%llx\n", l1pt);
	printf("%lx\n", (KERNBASE >> L1_SHIFT) & Ln_ADDR_MASK);

	/* Set this early so we can use the pagetable walking functions */
	kernel_pmap_store.pm_l1 = (pd_entry_t *)l1pt;
	PMAP_LOCK_INIT(kernel_pmap);

	/* Create a direct map region early so we can use it for pa -> va */
	pmap_bootstrap_dmap(l1pt);

	va = KERNBASE;
	pa = KERNBASE - kern_delta;

	/*
	 * Start to initialise phys_avail by copying from physmap
	 * up to the physical address KERNBASE points at.
	 */
	map_slot = avail_slot = 0;
	for (; map_slot < (physmap_idx * 2); map_slot += 2) {
		if (physmap[map_slot] == physmap[map_slot + 1])
			continue;

		if (physmap[map_slot] <= pa &&
		    physmap[map_slot + 1] > pa)
			break;

		phys_avail[avail_slot] = physmap[map_slot];
		phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;
		avail_slot += 2;
	}

	/* Add the memory before the kernel */
	if (physmap[avail_slot] < pa) {
		phys_avail[avail_slot] = physmap[map_slot];
		phys_avail[avail_slot + 1] = pa;
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;
		avail_slot += 2;
	}
	used_map_slot = map_slot;

	/*
	 * Read the page table to find out what is already mapped.
	 * This assumes we have mapped a block of memory from KERNBASE
	 * using a single L1 entry.
	 */
	l2 = pmap_early_page_idx(l1pt, KERNBASE, &l1_slot, &l2_slot);

	/* Sanity check the index, KERNBASE should be the first VA */
	KASSERT(l2_slot == 0, ("The L2 index is non-zero"));

	/* Find how many pages we have mapped */
	for (; l2_slot < Ln_ENTRIES; l2_slot++) {
		if ((l2[l2_slot] & ATTR_DESCR_MASK) == 0)
			break;

		/* Check locore used L2 blocks */
		KASSERT((l2[l2_slot] & ATTR_DESCR_MASK) == L2_BLOCK,
		    ("Invalid bootstrap L2 table"));
		KASSERT((l2[l2_slot] & ~ATTR_MASK) == pa,
		    ("Incorrect PA in L2 table"));

		va += L2_SIZE;
		pa += L2_SIZE;
	}

	/* And map the rest of L2 table */
	for (; l2_slot < Ln_ENTRIES; l2_slot++) {
		KASSERT(l2[l2_slot] == 0, ("Invalid bootstrap L2 table"));
		KASSERT(((va >> L2_SHIFT) & Ln_ADDR_MASK) == l2_slot,
		    ("VA inconsistency detected"));

		/*
		 * Check if we can use the current pa, some of it
		 * may fall out side the current physmap slot.
		 */
		if (pa + L2_SIZE > physmap[map_slot] + physmap[map_slot + 1]) {
			map_slot += 2;
			pa = physmap[map_slot];
			pa = roundup2(pa, L2_SIZE);

			/* TODO: should we wrap if we hit this? */
			KASSERT(map_slot < physmap_idx,
			    ("Attempting to use invalid physical memory"));
			/* TODO: This should be easy to fix */
			KASSERT(pa + L2_SIZE <
			    physmap[map_slot] + physmap[map_slot + 1],
			    ("Physical slot too small"));
		}

		/*
		 * TODO: Turn the cache on here when we have cache
		 * flushing code.
		 */
		l2[l2_slot] = (pa & ~L2_OFFSET) | ATTR_AF | L2_BLOCK |
		    ATTR_IDX(UNCACHED_MEMORY);

		va += L2_SIZE;
		pa += L2_SIZE;
	}

	freemempos = KERNBASE + kernlen;
	freemempos = roundup2(freemempos, PAGE_SIZE);
	/* Create the l2 tables up to VM_MAX_KERNEL_ADDRESS */
	freemempos = pmap_bootstrap_l2(l1pt, va, freemempos);
	/* And the l3 tables for the early devmap */
	freemempos = pmap_bootstrap_l3(l1pt,
	    VM_MAX_KERNEL_ADDRESS - L2_SIZE, freemempos);

	/* Flush the cache and tlb to ensure the new entries are valid */
	/* TODO: Flush the cache, we are relying on it being off */
	/* TODO: Move this to a function */
	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vmalle1is	\n"
	    "dsb  sy		\n"
	    "isb		\n");

#define alloc_pages(var, np)						\
	(var) = freemempos;						\
	freemempos += (np * PAGE_SIZE);					\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	/* Allocate dynamic per-cpu area. */
	alloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu, 0);

	virtual_avail = roundup2(freemempos, L1_SIZE);
	virtual_end = VM_MAX_KERNEL_ADDRESS - L2_SIZE;
	kernel_vm_end = virtual_avail;
	
	pa = pmap_early_vtophys(l1pt, freemempos);

	/* Finish initialising physmap */
	map_slot = used_map_slot;
	for (; avail_slot < (PHYS_AVAIL_SIZE - 2) &&
	    map_slot < (physmap_idx * 2); map_slot += 2) {
		if (physmap[map_slot] == physmap[map_slot + 1])
			continue;

		/* Have we used the current range? */
		if (physmap[map_slot + 1] <= pa)
			continue;

		/* Do we need to split the entry? */
		if (physmap[map_slot] < pa) {
			phys_avail[avail_slot] = pa;
			phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		} else {
			phys_avail[avail_slot] = physmap[map_slot];
			phys_avail[avail_slot + 1] = physmap[map_slot + 1];
		}
		physmem += (phys_avail[avail_slot + 1] -
		    phys_avail[avail_slot]) >> PAGE_SHIFT;

		avail_slot += 2;
	}
	phys_avail[avail_slot] = 0;
	phys_avail[avail_slot + 1] = 0;

	/* Flush the cache and tlb to ensure the new entries are valid */
	/* TODO: Flush the cache, we are relying on it being off */
	/* TODO: Move this to a function */
	__asm __volatile(
	    "dsb  sy		\n"
	    "tlbi vmalle1is	\n"
	    "dsb  sy		\n"
	    "isb		\n");
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
}

/***************************************************
 * Low level helper routines.....
 ***************************************************/

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	pd_entry_t *l1, *l2;
	pt_entry_t *l3;
	pmap_t pmap;

	pmap = pmap_kernel();

	l1 = pmap_l1(pmap, va);
	if ((*l1 & ATTR_DESCR_MASK) == L1_BLOCK)
		return ((*l1 & ~ATTR_MASK) | (va & L1_OFFSET));

	if ((*l1 & ATTR_DESCR_MASK) == L1_TABLE) {
		l2 = pmap_l1_to_l2(l1, va);
		if (l2 == NULL)
			return (0);

		if ((*l2 & ATTR_DESCR_MASK) == L2_BLOCK)
			return ((*l2 & ~ATTR_MASK) | (va & L2_OFFSET));

		if ((*l2 & ATTR_DESCR_MASK) == L2_TABLE) {
			l3 = pmap_l2_to_l3(l2, va);
			if (l3 == NULL)
				return (0);

			if ((*l3 & ATTR_DESCR_MASK) == L3_PAGE)
				return ((*l3 & ~ATTR_MASK) | (va & L3_OFFSET));
		}
	}

	return (0);
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

	panic("pmap_extract");
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

	panic("pmap_extract_and_hold");
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
PMAP_INLINE void 
pmap_kenter_internal(vm_offset_t va, vm_paddr_t pa, int type)
{
	pt_entry_t *l3;

#ifdef PMAP_DEBUG
	printf("pmap_kenter:  va: %p -> pa: %p\n", (void *)va, (void *)pa);
#endif

	l3 = pmap_l3(kernel_pmap, va);
	KASSERT(l3 != NULL, ("Invalid page table"));
	*l3 = (pa & ~L3_OFFSET) | ATTR_AF | L3_PAGE | ATTR_IDX(type);
}

void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{

	/*
	 * TODO: Turn the cache on here when we have cache flushing code.
	 */
	pmap_kenter_internal(va, pa, UNCACHED_MEMORY);
}

void
pmap_kenter_device(vm_offset_t va, vm_paddr_t pa)
{

	pmap_kenter_internal(va, pa, DEVICE_MEMORY);
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *l3;

	l3 = pmap_l3(kernel_pmap, va);
	KASSERT(l3 != NULL, ("Invalid page table"));
	*l3 = 0;
}

/*
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	The wired attribute of the page table entry is not a hardware feature,
 *	so there is no need to invalidate any TLB entries.
 */
void
pmap_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{

	panic("pmap_unwire");
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

	return (start | DMAP_MIN_ADDRESS);
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
	int i;

	va = sva;
	for (i = 0; i < count; i++) {
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
		va += PAGE_SIZE;
	}
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
	int i;

	va = sva;
	for (i = 0; i < count; i++) {
		if (vtophys(va))
			pmap_kremove(va);

		va += PAGE_SIZE;
	}
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

/*
 * Initialize the pmap for the swapper process.
 */
void
pmap_pinit0(pmap_t pmap)
{

	printf("TODO: pmap_pinit0\n");
	bcopy(kernel_pmap, pmap, sizeof(*pmap));
	bzero(&pmap->pm_mtx, sizeof(pmap->pm_mtx));
	PMAP_LOCK_INIT(pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit(pmap_t pmap)
{
	vm_paddr_t l1phys;
	vm_page_t l1pt;

	while ((l1pt = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL)
		VM_WAIT;

	l1phys = VM_PAGE_TO_PHYS(l1pt);
	pmap->pm_l1 = (pd_entry_t *)PHYS_TO_DMAP(l1phys);

	if ((l1pt->flags & PG_ZERO) == 0)
		bzero(pmap->pm_l1, PAGE_SIZE);

	bzero(&pmap->pm_stats, sizeof(pmap->pm_stats));

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

	panic("pmap_release");
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	pd_entry_t *l2;
	vm_paddr_t pa;
	vm_page_t m;

	while (kernel_vm_end < addr) {
		/* Allocate a page for the l3 table */
		m = vm_page_alloc(NULL, 0,
		    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		if (m == NULL)
			panic("pmap_growkernel: no memory to grow kernel");
		if ((m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		pa = VM_PAGE_TO_PHYS(m);

		l2 = pmap_l2(kernel_pmap, kernel_vm_end);
		*l2 = pa | ATTR_AF | L2_TABLE;
		kernel_vm_end += L2_SIZE;
	}
}

/***************************************************
 * page management routines.
 ***************************************************/

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pt_entry_t *l3;
	vm_offset_t va_next;

	KASSERT(pmap == pmap_kernel(), ("Only kernel mappings for now"));
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		va_next = sva + L3_SIZE;
		l3 = pmap_l3(pmap, sva);
		if (l3 != NULL)
			*l3 = 0;
	}
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

	panic("pmap_remove_all");
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va_next;
	pd_entry_t *l1, *l2;
	pt_entry_t *l3;
	uint64_t mask;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	/* TODO: Set the PXN / UXN bits */
	mask = 0;
	if ((prot & VM_PROT_WRITE) != 0)
		mask |= ATTR_AP(ATTR_AP_RW);

	for (; sva < eva; sva = va_next) {
		l1 = pmap_l1(pmap, sva);
		if ((*l1 & ATTR_DESCR_MASK) == L1_BLOCK) {
			*l1 &= ~ATTR_AP_MASK;
			*l1 |= mask;
			va_next = sva + L1_SIZE;
			continue;
		}

		l2 = pmap_l1_to_l2(l1, sva);
		if (l2 == NULL || (*l2 & ATTR_DESCR_MASK) == L2_BLOCK) {
			*l2 &= ~ATTR_AP_MASK;
			*l2 |= mask;
			va_next = sva + L2_SIZE;
			continue;
		}

		l3 = pmap_l2_to_l3(l2, sva);
		if (l3 != NULL && (*l3 & ATTR_DESCR_MASK) == L3_PAGE) {
			*l3 &= ~ATTR_AP_MASK;
			*l3 |= mask;
		}
		va_next = sva + L3_SIZE;
	}
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
	vm_paddr_t pa;
	pt_entry_t *l3, opte;

	PMAP_LOCK(pmap);
	pa = VM_PAGE_TO_PHYS(m);
	l3 = pmap_l3(pmap, va);
	KASSERT(l3 != NULL, ("TODO: grow va"));
	KASSERT(pmap == pmap_kernel(), ("Only kernel mappings for now"));

	opte = *l3;
	KASSERT(opte == 0, ("TODO: Update the entry"));
	*l3 = (pa & ~L3_OFFSET) | ATTR_AF | L3_PAGE;

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

	panic("pmap_enter_object");
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

	panic("pmap_enter_quick");
}

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{

	panic("pmap_object_init_pt");
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

	panic("pmap_copy");
}

/*
 *	pmap_zero_page zeros the specified hardware page by mapping 
 *	the page into KVM and using bzero to clear its contents.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va = VM_PAGE_TO_PHYS(m) | DMAP_MIN_ADDRESS;

	bzero((void *)va, PAGE_SIZE);
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

	panic("pmap_zero_page_area");
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

	panic("pmap_zero_page_idle");
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

	panic("pmap_copy_page");
}

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{

	panic("pmap_copy_pages");
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

	panic("pmap_page_exists_quick");
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

	panic("pmap_page_wired_mappings");
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

	panic("pmap_remove_pages");
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

	panic("pmap_is_modified");
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

	panic("pmap_is_prefaultable");
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

	panic("pmap_is_referenced");
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{

	panic("pmap_remove_write");
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

	panic("pmap_ts_referenced");
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{

	panic("pmap_advise");
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{

	panic("pmap_clear_modify");
}

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	panic("pmap_page_set_memattr");
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{

	panic("pmap_mincore");
}

void
pmap_activate(struct thread *td)
{
	struct pcb *pcb;
	pmap_t pmap;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	pcb = td->td_pcb;

	pcb->pcb_l1addr = vtophys(pmap->pm_l1);

	critical_exit();
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{

	panic("pmap_sync_icache");
}

/*
 * Increase the starting virtual address of the given mapping if a
 * different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{

}

