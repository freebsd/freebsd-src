/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD$
 */

/*
 * Manages physical address maps.
 *
 * In addition to hardware address maps, this module is called upon to
 * provide software-use-only maps which may or may not be stored in the
 * same form as hardware maps.  These pseudo-maps are used to store
 * intermediate results from copy operations to and from address spaces.
 *
 * Since the information managed by this module is also stored by the
 * logical address mapping module, this module may throw away valid virtual
 * to physical mappings at almost any time.  However, invalidations of
 * mappings must be done as requested.
 *
 * In order to cope with hardware architectures which make virtual to
 * physical map invalidates expensive, this module may delay invalidate
 * reduced protection operations until such time as they are actually
 * necessary.  This module is given full information as to which processors
 * are currently using which maps, and to when physical maps must be made
 * correct.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <machine/frame.h>
#include <machine/pv.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/tsb.h>

#define	PMAP_DEBUG

#define	PMAP_LOCK(pm)
#define	PMAP_UNLOCK(pm)

#define	dcache_global_flush(pa)
#define	icache_global_flush(pa)

struct mem_region {
	vm_offset_t mr_start;
	vm_offset_t mr_size;
};

struct ofw_map {
	vm_offset_t om_start;
	vm_offset_t om_size;
	u_long	om_tte;
};

/*
 * Virtual address of message buffer.
 */
struct msgbuf *msgbufp;

/*
 * Physical addresses of first and last available physical page.
 */
vm_offset_t avail_start;
vm_offset_t avail_end;

/*
 * Map of physical memory reagions.
 */
vm_offset_t phys_avail[10];

/*
 * First and last available kernel virtual addresses.
 */
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;

/*
 * Kernel pmap handle and associated storage.
 */
pmap_t kernel_pmap;
static struct pmap kernel_pmap_store;

/*
 * Map of free and in use hardware contexts and index of first potentially
 * free context.
 */
static char pmap_context_map[PMAP_CONTEXT_MAX];
static u_int pmap_context_base;

/*
 * Virtual addresses of free space for temporary mappings.  Used for copying
 * and zeroing physical pages.
 */
static vm_offset_t CADDR1;
static vm_offset_t CADDR2;

/*
 * Allocate and free hardware context numbers.
 */
static u_int pmap_context_alloc(void);
static void pmap_context_destroy(u_int i);

/*
 * Allocate physical memory for use in pmap_bootstrap.
 */
static vm_offset_t pmap_bootstrap_alloc(vm_size_t size);

/*
 * Quick sort callout for comparing memory regions.
 */
static int mr_cmp(const void *a, const void *b);
static int
mr_cmp(const void *a, const void *b)
{
	return ((const struct mem_region *)a)->mr_start -
	    ((const struct mem_region *)b)->mr_start;
}

/*
 * Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t skpa, vm_offset_t ekva)
{
	struct mem_region mra[8];
	ihandle_t pmem;
	struct pmap *pm;
	vm_offset_t pa;
	vm_offset_t va;
	struct tte tte;
	int sz;
	int i;
	int j;

	/*
	 * Find out what physical memory is available from the prom and
	 * initialize the phys_avail array.
	 */
	if ((pmem = OF_finddevice("/memory")) == -1)
		panic("pmap_bootstrap: finddevice /memory");
	if ((sz = OF_getproplen(pmem, "available")) == -1)
		panic("pmap_bootstrap: getproplen /memory/available");
	if (sizeof(phys_avail) < sz)
		panic("pmap_bootstrap: phys_avail too small");
	bzero(mra, sz);
	if (OF_getprop(pmem, "available", mra, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/available");
	sz /= sizeof(*mra);
	qsort(mra, sz, sizeof *mra, mr_cmp);
	for (i = 0, j = 0; i < sz; i++, j += 2) {
		phys_avail[j] = mra[i].mr_start;
		phys_avail[j + 1] = mra[i].mr_start + mra[i].mr_size;
	}

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	pm = &kernel_pmap_store;
	pm->pm_context = TLB_CTX_KERNEL;
	pm->pm_active = ~0;
	pm->pm_count = 1;
	kernel_pmap = pm;

	/*
	 * Allocate the kernel tsb and lock it in the tlb.
	 */
	pa = pmap_bootstrap_alloc(TSB_KERNEL_SIZE);
	if (pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: tsb unaligned\n");
	tsb_kernel_phys = pa;
	for (i = 0; i < TSB_KERNEL_PAGES; i++) {
		va = TSB_KERNEL_MIN_ADDRESS + i * PAGE_SIZE_4M;
		tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
		tte.tte_data = TD_V | TD_4M | TD_VA_LOW(va) | TD_PA(pa) |
		    TD_MOD | TD_REF | TD_TSB | TD_L | TD_CP | TD_P | TD_W;
		tlb_store_slot(TLB_DTLB, va, tte, TLB_SLOT_TSB_KERNEL_MIN + i);
	}
	bzero((void *)va, TSB_KERNEL_SIZE);
	stxa(AA_IMMU_TSB, ASI_IMMU, 
	    (va >> (STTE_SHIFT - TTE_SHIFT)) | TSB_SIZE_REG);
	stxa(AA_DMMU_TSB, ASI_DMMU,
	    (va >> (STTE_SHIFT - TTE_SHIFT)) | TSB_SIZE_REG);
	membar(Sync);

	/*
	 * Calculate the first and last available physical addresses.
	 */
	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	avail_end = phys_avail[i + 1];

	/*
	 * Allocate physical memory for the heads of the stte alias chains.
	 */
	sz = round_page(((avail_end - avail_start) >> PAGE_SHIFT) *
	    sizeof(struct pv_head));
	pv_table = pmap_bootstrap_alloc(sz);
	/* XXX */
	avail_start += sz;
	for (i = 0; i < sz; i += sizeof(struct pv_head))
		pvh_set_first(pv_table + i, 0);

	/*
	 * Set the start and end of kva.  The kernel is loaded at the first
	 * available 4 meg super page, so round up to the end of the page.
	 */
	virtual_avail = roundup(ekva, PAGE_SIZE_4M);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Allocate virtual address space for copying and zeroing pages of
	 * physical memory.
	 */
	CADDR1 = virtual_avail;
	virtual_avail += PAGE_SIZE;
	CADDR2 = virtual_avail;
	virtual_avail += PAGE_SIZE;
}

/*
 * Allocate a physical page of memory directly from the phys_avail map.
 * Can only be called from pmap_bootstrap before avail start and end are
 * calculated.
 */
static vm_offset_t
pmap_bootstrap_alloc(vm_size_t size)
{
	vm_offset_t pa;
	int i;

	size = round_page(size);
	for (i = 0; phys_avail[i] != 0; i += 2) {
		if (phys_avail[i + 1] - phys_avail[i] < size)
			continue;
		pa = phys_avail[i];
		phys_avail[i] += size;
		return (pa);
	}
	panic("pmap_bootstrap_alloc");
}

/*
 * Allocate a hardware context number from the context map.
 */
static u_int
pmap_context_alloc(void)
{
	u_int i;

	i = pmap_context_base;
	do {
		if (pmap_context_map[i] == 0) {
			pmap_context_map[i] = 1;
			pmap_context_base = (i + 1) & (PMAP_CONTEXT_MAX - 1);
			return (i);
		}
	} while ((i = (i + 1) & (PMAP_CONTEXT_MAX - 1)) != pmap_context_base);
	panic("pmap_context_alloc");
}

/*
 * Free a hardware context number back to the context map.
 */
static void
pmap_context_destroy(u_int i)
{

	pmap_context_map[i] = 0;
}

/*
 * Map a range of physical addresses into kernel virtual address space.
 *
 * The value passed in *virt is a suggested virtual address for the mapping.
 * Architectures which can support a direct-mapped physical to virtual region
 * can return the appropriate address within that region, leaving '*virt'
 * unchanged.  We cannot and therefore do not; *virt is updated with the
 * first usable address after the mapped region.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t pa_start, vm_offset_t pa_end, int prot)
{
	vm_offset_t sva;
	vm_offset_t va;

	sva = *virt;
	va = sva;
	for (; pa_start < pa_end; pa_start += PAGE_SIZE, va += PAGE_SIZE)
		pmap_kenter(va, pa_start);
	*virt = va;
	return (sva);
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	struct tte tte;

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_MOD | TD_REF | TD_CP | TD_P | TD_W;
	tsb_tte_enter_kernel(va, tte);
}

/*
 * Remove a wired page from kernel virtual address space.
 */
void
pmap_kremove(vm_offset_t va)
{
	tsb_remove_kernel(va);
}

/*
 * Map a list of wired pages into kernel virtual address space.  This is
 * intended for temporary mappings which do not need page modification or
 * references recorded.  Existing mappings in the region are overwritten.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	int i;

	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by pmap_qenter.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	int i;

	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kremove(va);
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 */
void
pmap_enter(pmap_t pm, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	struct stte *stp;
	struct tte tte;
	vm_offset_t pa;

	pa = VM_PAGE_TO_PHYS(m);
	tte.tte_tag = TT_CTX(pm->pm_context) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_CP | TD_CV;
	if (pm->pm_context == TLB_CTX_KERNEL)
		tte.tte_data |= TD_P;
	if (wired == TRUE) {
		tte.tte_data |= TD_REF;
		if (prot & VM_PROT_WRITE)
			tte.tte_data |= TD_MOD;
	}
	if (prot & VM_PROT_WRITE)
		tte.tte_data |= TD_W;
	if (prot & VM_PROT_EXECUTE) {
		tte.tte_data |= TD_EXEC;
		icache_global_flush(&pa);
	}

	if (pm == kernel_pmap) {
		tsb_tte_enter_kernel(va, tte);
		return;
	}

	PMAP_LOCK(pm);
	if ((stp = tsb_stte_lookup(pm, va)) != NULL) {
		pv_remove_virt(stp);
		tsb_stte_remove(stp);
		pv_insert(pm, pa, va, stp);
		stp->st_tte = tte;
	} else {
		tsb_tte_enter(pm, va, tte);
	}
	PMAP_UNLOCK(pm);
}

/*
 * Initialize the pmap module.
 */
void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
}

void
pmap_init2(void)
{
}

/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pm)
{

	pm = &kernel_pmap_store;
	pm->pm_context = pmap_context_alloc();
	pm->pm_active = 0;
	pm->pm_count = 1;
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

/*
 * Initialize a preallocated and zeroed pmap structure.
 */
void
pmap_pinit(pmap_t pm)
{
	struct stte *stp;

	pm->pm_context = pmap_context_alloc();
	pm->pm_active = 0;
	pm->pm_count = 1;
	stp = &pm->pm_stte;
	stp->st_tte = tsb_page_alloc(pm, (vm_offset_t)tsb_base(0));
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

void
pmap_pinit2(pmap_t pmap)
{
}

/*
 * Grow the number of kernel page table entries.  Unneeded.
 */
void
pmap_growkernel(vm_offset_t addr)
{
}

/*
 * Zero a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_zero_page(vm_offset_t pa)
{
	struct tte tte;
	vm_offset_t va;

	va = CADDR2;
	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_PA(pa) | TD_L | TD_CP | TD_P | TD_W;
	tlb_store(TLB_DTLB, va, tte);
	bzero((void *)va, PAGE_SIZE);
	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, va);
}

/*
 * Make the specified page pageable (or not).  Unneeded.
 */
void
pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
	      boolean_t pageable)
{
}

/*
 * Create the kernel stack and user structure for a new process.  This
 * routine directly affects the performance of fork().
 */
void
pmap_new_proc(struct proc *p)
{
	struct user *u;
	vm_object_t o;
	vm_page_t m;
	u_int i;

	if ((o = p->p_upages_obj) == NULL) {
		o = vm_object_allocate(OBJT_DEFAULT, UPAGES);
		p->p_upages_obj = o;
	}
	if ((u = p->p_addr) == NULL) {
		u = (struct user *)kmem_alloc_nofault(kernel_map,
		    UPAGES * PAGE_SIZE);
		KASSERT(u != NULL, ("pmap_new_proc: u area\n"));
		p->p_addr = u;
	}
	for (i = 0; i < UPAGES; i++) {
		m = vm_page_grab(o, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		m->wire_count++;
		cnt.v_wire_count++;

		pmap_kenter((vm_offset_t)u + i * PAGE_SIZE,
		    VM_PAGE_TO_PHYS(m));
		
		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{

	if (m->flags & PG_FICTITIOUS || prot & VM_PROT_WRITE)
		return;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
		pv_bit_clear(m, TD_W);
	else
		pv_global_remove_all(m);
}

void
pmap_clear_modify(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return;
	pv_bit_clear(m, TD_MOD);
}

void
pmap_activate(struct proc *p)
{
	TODO;
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{
	TODO;
	return (0);
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	TODO;
}

void
pmap_collect(void)
{
	TODO;
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	TODO;
}

void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	TODO;
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{
	TODO;
}

vm_offset_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	TODO;
	return (0);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_clear_reference(vm_page_t m)
{
	TODO;
}

int
pmap_ts_referenced(vm_page_t m)
{
	TODO;
	return (0);
}

vm_offset_t
pmap_kextract(vm_offset_t va)
{
	TODO;
	return (0);
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	TODO;
	return (0);
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size, int limit)
{
	TODO;
}

boolean_t
pmap_page_exists(pmap_t pmap, vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_prefault(pmap_t pmap, vm_offset_t va, vm_map_entry_t entry)
{
	TODO;
}

void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	TODO;
}

vm_offset_t
pmap_phys_address(int ppn)
{
	TODO;
	return (0);
}

void
pmap_reference(pmap_t pm)
{
	if (pm != NULL)
		pm->pm_count++;
}

void
pmap_release(pmap_t pmap)
{
	TODO;
}

void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	TODO;
}

void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	TODO;
}

void
pmap_swapin_proc(struct proc *p)
{
	TODO;
}

void
pmap_swapout_proc(struct proc *p)
{
	TODO;
}
