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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      from:   @(#)pmap.c      7.7 (Berkeley)  5/12/91
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

#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
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
 * Virtual and physical address of message buffer.
 */
struct msgbuf *msgbufp;
vm_offset_t msgbuf_phys;

/*
 * Physical addresses of first and last available physical page.
 */
vm_offset_t avail_start;
vm_offset_t avail_end;

/*
 * Map of physical memory reagions.
 */
vm_offset_t phys_avail[128];
static struct mem_region mra[128];
static struct ofw_map oma[128];

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
pmap_bootstrap(vm_offset_t ekva)
{
	struct pmap *pm;
	struct stte *stp;
	struct tte tte;
	vm_offset_t off;
	vm_offset_t pa;
	vm_offset_t va;
	ihandle_t pmem;
	ihandle_t vmem;
	int sz;
	int i;
	int j;

	/*
	 * Set the start and end of kva.  The kernel is loaded at the first
	 * available 4 meg super page, so round up to the end of the page.
	 */
	virtual_avail = roundup2(ekva, PAGE_SIZE_4M);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Find out what physical memory is available from the prom and
	 * initialize the phys_avail array.  This must be done before
	 * pmap_bootstrap_alloc is called.
	 */
	if ((pmem = OF_finddevice("/memory")) == -1)
		panic("pmap_bootstrap: finddevice /memory");
	if ((sz = OF_getproplen(pmem, "available")) == -1)
		panic("pmap_bootstrap: getproplen /memory/available");
	if (sizeof(phys_avail) < sz)
		panic("pmap_bootstrap: phys_avail too small");
	if (sizeof(mra) < sz)
		panic("pmap_bootstrap: mra too small");
	bzero(mra, sz);
	if (OF_getprop(pmem, "available", mra, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/available");
	sz /= sizeof(*mra);
	CTR0(KTR_PMAP, "pmap_bootstrap: physical memory");
	qsort(mra, sz, sizeof *mra, mr_cmp);
	for (i = 0, j = 0; i < sz; i++, j += 2) {
		CTR2(KTR_PMAP, "start=%#lx size=%#lx", mra[i].mr_start,
		    mra[i].mr_size);
		phys_avail[j] = mra[i].mr_start;
		phys_avail[j + 1] = mra[i].mr_start + mra[i].mr_size;
	}

	/*
	 * Allocate the kernel tsb and lock it in the tlb.
	 */
	pa = pmap_bootstrap_alloc(KVA_PAGES * PAGE_SIZE_4M);
	if (pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: tsb unaligned\n");
	tsb_kernel_phys = pa;
	tsb_kernel = (struct stte *)virtual_avail;
	virtual_avail += KVA_PAGES * PAGE_SIZE_4M;
	for (i = 0; i < KVA_PAGES; i++) {
		va = (vm_offset_t)tsb_kernel + i * PAGE_SIZE_4M;
		tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
		tte.tte_data = TD_V | TD_4M | TD_VA_LOW(va) | TD_PA(pa) |
		    TD_L | TD_CP | TD_P | TD_W;
		tlb_store_slot(TLB_DTLB, va, TLB_CTX_KERNEL, tte,
		    TLB_SLOT_TSB_KERNEL_MIN + i);
	}
	bzero(tsb_kernel, KVA_PAGES * PAGE_SIZE_4M);

	/*
	 * Load the tsb registers.
	 */
	stxa(AA_DMMU_TSB, ASI_DMMU,
	    (vm_offset_t)tsb_kernel >> (STTE_SHIFT - TTE_SHIFT));
	stxa(AA_IMMU_TSB, ASI_IMMU,
	    (vm_offset_t)tsb_kernel >> (STTE_SHIFT - TTE_SHIFT));
	membar(Sync);
	flush(va);

	/*
	 * Allocate the message buffer.
	 */
	msgbuf_phys = pmap_bootstrap_alloc(MSGBUF_SIZE);

	/*
	 * Add the prom mappings to the kernel tsb.
	 */
	if ((vmem = OF_finddevice("/virtual-memory")) == -1)
		panic("pmap_bootstrap: finddevice /virtual-memory");
	if ((sz = OF_getproplen(vmem, "translations")) == -1)
		panic("pmap_bootstrap: getproplen translations");
	if (sizeof(oma) < sz)
		panic("pmap_bootstrap: oma too small");
	bzero(oma, sz);
	if (OF_getprop(vmem, "translations", oma, sz) == -1)
		panic("pmap_bootstrap: getprop /virtual-memory/translations");
	sz /= sizeof(*oma);
	CTR0(KTR_PMAP, "pmap_bootstrap: translations");
	for (i = 0; i < sz; i++) {
		CTR4(KTR_PMAP,
		    "translation: start=%#lx size=%#lx tte=%#lx pa=%#lx",
		    oma[i].om_start, oma[i].om_size, oma[i].om_tte,
		    TD_PA(oma[i].om_tte));
		if (oma[i].om_start < 0xf0000000)	/* XXX!!! */
			continue;
		for (off = 0; off < oma[i].om_size; off += PAGE_SIZE) {
			va = oma[i].om_start + off;
			tte.tte_data = oma[i].om_tte + off;
			tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
			stp = tsb_kvtostte(va);
			CTR4(KTR_PMAP,
			    "mapping: va=%#lx stp=%p tte=%#lx pa=%#lx",
			    va, stp, tte.tte_data, TD_PA(tte.tte_data));
			stp->st_tte = tte;
		}
	}

	/*
	 * Calculate the first and last available physical addresses.
	 */
	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	avail_end = phys_avail[i + 1];

	/*
	 * Allocate virtual address space for copying and zeroing pages of
	 * physical memory.
	 */
	CADDR1 = virtual_avail;
	virtual_avail += PAGE_SIZE;
	CADDR2 = virtual_avail;
	virtual_avail += PAGE_SIZE;

	/*
	 * Allocate virtual address space for the message buffer.
	 */
	msgbufp = (struct msgbuf *)virtual_avail;
	virtual_avail += round_page(MSGBUF_SIZE);

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
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	pm = &kernel_pmap_store;
	pm->pm_context = TLB_CTX_KERNEL;
	pm->pm_active = ~0;
	pm->pm_count = 1;
	kernel_pmap = pm;

	/*
	 * Set the secondary context to be the kernel context (needed for
	 * fp block operations in the kernel).
	 */
	stxa(AA_DMMU_SCXR, ASI_DMMU, TLB_CTX_KERNEL);
	membar(Sync);
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
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
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
	struct stte *stp;
	struct tte tte;

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_REF | TD_SW | TD_CP | TD_P | TD_W;
	stp = tsb_kvtostte(va);
	CTR4(KTR_PMAP, "pmap_kenter: va=%#lx pa=%#lx stp=%p data=%#lx",
	    va, pa, stp, stp->st_tte.tte_data);
	stp->st_tte = tte;
}

/*
 * Map a wired page into kernel virtual address space. This additionally
 * takes a flag argument wich is or'ed to the TTE data. This is used by
 * bus_space_map().
 */
void
pmap_kenter_flags(vm_offset_t va, vm_offset_t pa, u_long flags)
{
	struct stte *stp;
	struct tte tte;

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	if ((flags & TD_W) != 0)
		flags |= TD_SW;
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_REF | TD_P | flags;
	stp = tsb_kvtostte(va);
	CTR4(KTR_PMAP, "pmap_kenter: va=%#lx pa=%#lx stp=%p data=%#lx",
	    va, pa, stp, stp->st_tte.tte_data);
	stp->st_tte = tte;
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_offset_t pa, int i)
{

	TODO;
}

/*
 * Remove a wired page from kernel virtual address space.
 */
void
pmap_kremove(vm_offset_t va)
{
	struct stte *stp;

	stp = tsb_kvtostte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx stp=%p data=%#lx", va, stp,
	    stp->st_tte.tte_data);
	tsb_stte_remove(stp);
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
	u_long data;

	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_enter: non current pmap"));
	pa = VM_PAGE_TO_PHYS(m);
	CTR5(KTR_PMAP, "pmap_enter: ctx=%p va=%#lx pa=%#lx prot=%#x wired=%d",
	    pm->pm_context, va, pa, prot, wired);
	tte.tte_tag = TT_CTX(pm->pm_context) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_CP | TD_CV;
	if (pm->pm_context == TLB_CTX_KERNEL)
		tte.tte_data |= TD_P;
	if (wired == TRUE) {
		tte.tte_data |= TD_REF;
		if (prot & VM_PROT_WRITE)
			tte.tte_data |= TD_W;
	}
	if (prot & VM_PROT_WRITE)
		tte.tte_data |= TD_SW;
	if (prot & VM_PROT_EXECUTE) {
		tte.tte_data |= TD_EXEC;
		icache_global_flush(pa);
	}
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0)
		tte.tte_data |= TD_MNG;

	PMAP_LOCK(pm);
	if ((stp = tsb_stte_lookup(pm, va)) != NULL) {
		data = stp->st_tte.tte_data;
		if (TD_PA(data) == pa) {
			if (prot & VM_PROT_WRITE)
				tte.tte_data |= TD_W;
			CTR3(KTR_PMAP,
			    "pmap_enter: update pa=%#lx data=%#lx to %#lx",
			    pa, data, tte.tte_data);	
		}
		if (stp->st_tte.tte_data & TD_MNG)
			pv_remove_virt(stp);
		tsb_stte_remove(stp);
		if (tte.tte_data & TD_MNG)
			pv_insert(pm, pa, va, stp);
		stp->st_tte = tte;
	} else {
		tsb_tte_enter(pm, va, tte);
	}
	PMAP_UNLOCK(pm);
}

void
pmap_remove(pmap_t pm, vm_offset_t start, vm_offset_t end)
{
	struct stte *stp;

	CTR3(KTR_PMAP, "pmap_remove: pm=%p start=%#lx end=%#lx",
	    pm, start, end);
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_remove: non current pmap"));
	PMAP_LOCK(pm);
	for (; start < end; start += PAGE_SIZE) {
		if ((stp = tsb_stte_lookup(pm, start)) == NULL)
			continue;
		if (stp->st_tte.tte_data & TD_MNG)
			pv_remove_virt(stp);
		tsb_stte_remove(stp);
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

	pm->pm_object = vm_object_allocate(OBJT_DEFAULT, 16);
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
 * Make the specified page pageable (or not).  Unneeded.
 */
void
pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
	      boolean_t pageable)
{
}

/*
 * Create the user structure for a new process.  This
 * routine directly affects the performance of fork().
 */
void
pmap_new_proc(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	u_int i;

	upobj = p->p_upages_obj;
	if (upobj == NULL) {
		upobj = vm_object_allocate(OBJT_DEFAULT, UAREA_PAGES);
		p->p_upages_obj = upobj;
	}
	up = (vm_offset_t)p->p_uarea;
	if (up == 0) {
		up = kmem_alloc_nofault(kernel_map, UAREA_PAGES * PAGE_SIZE);
		if (up == 0)
			panic("pmap_new_proc: upage allocation failed");
		p->p_uarea = (struct user *)up;
	}
	for (i = 0; i < UAREA_PAGES; i++) {
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		m->wire_count++;
		cnt.v_wire_count++;
		pmap_kenter(up + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void
pmap_dispose_proc(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	int i;

	upobj = p->p_upages_obj;
	up = (vm_offset_t)p->p_uarea;
	for (i = 0; i < UAREA_PAGES; i++) {
		m = vm_page_lookup(upobj, i);
		if (m == NULL)
			panic("pmap_dispose_proc: upage already missing?");
		vm_page_busy(m);
		pmap_kremove(up + i * PAGE_SIZE);
		vm_page_unwire(m, 0);
		vm_page_free(m);
	}
}

/*
 * Allow the UPAGES for a process to be prejudicially paged out.
 */
void
pmap_swapout_proc(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	int i;

	upobj = p->p_upages_obj;
	up = (vm_offset_t)p->p_uarea;
	for (i = 0; i < UAREA_PAGES; i++) {
		m = vm_page_lookup(upobj, i);
		if (m == NULL)
			panic("pmap_swapout_proc: upage already missing?");
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		pmap_kremove(up + i * PAGE_SIZE);
	}
}

/*
 * Bring the UPAGES for a specified process back in.
 */
void
pmap_swapin_proc(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	int rv;
	int i;

	upobj = p->p_upages_obj;
	up = (vm_offset_t)p->p_uarea;
	for (i = 0; i < UAREA_PAGES; i++) {
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		pmap_kenter(up + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(upobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_proc: cannot get upage");
			m = vm_page_lookup(upobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
	}
}

/*
 * Create the kernel stack for a new thread.  This
 * routine directly affects the performance of fork().
 */
void
pmap_new_thread(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	u_int i;

	ksobj = td->td_kstack_obj;
	if (ksobj == NULL) {
		ksobj = vm_object_allocate(OBJT_DEFAULT, KSTACK_PAGES);
		td->td_kstack_obj = ksobj;
	}
	ks = td->td_kstack;
	if (ks == 0) {
		ks = kmem_alloc_nofault(kernel_map,
		   (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE);
		if (ks == 0)
			panic("pmap_new_thread: kstack allocation failed");
		/* XXX remove from tlb */
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
		td->td_kstack = ks;
	}
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		m->wire_count++;
		cnt.v_wire_count++;
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void
pmap_dispose_thread(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	int i;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("pmap_dispose_proc: upage already missing?");
		vm_page_busy(m);
		pmap_kremove(ks + i * PAGE_SIZE);
		vm_page_unwire(m, 0);
		vm_page_free(m);
	}
}

/*
 * Allow the kernel stack for a thread to be prejudicially paged out.
 */
void
pmap_swapout_thread(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	int i;

	ksobj = td->td_kstack_obj;
	ks = (vm_offset_t)td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("pmap_swapout_thread: kstack already missing?");
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		pmap_kremove(ks + i * PAGE_SIZE);
	}
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
void
pmap_swapin_thread(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	int rv;
	int i;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(ksobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_proc: cannot get kstack");
			m = vm_page_lookup(ksobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
	}
}

void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{

	if (m->flags & PG_FICTITIOUS || prot & VM_PROT_WRITE)
		return;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
		pv_bit_clear(m, TD_W | TD_SW);
	else
		pv_global_remove_all(m);
}

void
pmap_clear_modify(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return;
	pv_bit_clear(m, TD_W);
}

boolean_t
pmap_is_modified(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return FALSE;
	return (pv_bit_test(m, TD_W));
}

void
pmap_clear_reference(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return;
	pv_bit_clear(m, TD_REF);
}

int
pmap_ts_referenced(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return (0);
	return (pv_bit_count(m, TD_REF));
}

void
pmap_activate(struct thread *td)
{
	struct vmspace *vm;
	struct stte *stp;
	struct proc *p;
	pmap_t pm;

	p = td->td_proc;
	vm = p->p_vmspace;
	pm = &vm->vm_pmap;
	stp = &pm->pm_stte;
	KASSERT(stp->st_tte.tte_data & TD_V,
	    ("pmap_copy: dst_pmap not initialized"));
	tlb_store_slot(TLB_DTLB, (vm_offset_t)tsb_base(0), TLB_CTX_KERNEL,
	    stp->st_tte, tsb_tlb_slot(0));
	if ((stp->st_tte.tte_data & TD_INIT) == 0) {
		tsb_page_init(tsb_base(0), 0);
		stp->st_tte.tte_data |= TD_INIT;
	}
	stxa(AA_DMMU_PCXR, ASI_DMMU, pm->pm_context);
	membar(Sync);
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{

	return (va);
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	/* XXX */
}

void
pmap_collect(void)
{
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	/* XXX */
}

/*
 * Copy a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	struct tte tte;

	CTR2(KTR_PMAP, "pmap_copy_page: src=%#lx dst=%#lx", src, dst);

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(CADDR1);
	tte.tte_data = TD_V | TD_8K | TD_PA(src) | TD_L | TD_CP | TD_P | TD_W;
	tlb_store(TLB_DTLB, CADDR1, TLB_CTX_KERNEL, tte);

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(CADDR2);
	tte.tte_data = TD_V | TD_8K | TD_PA(dst) | TD_L | TD_CP | TD_P | TD_W;
	tlb_store(TLB_DTLB, CADDR2, TLB_CTX_KERNEL, tte);

	bcopy((void *)CADDR1, (void *)CADDR2, PAGE_SIZE);

	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, CADDR1);
	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, CADDR2);
}

/*
 * Zero a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_zero_page(vm_offset_t pa)
{
	struct tte tte;

	CTR1(KTR_PMAP, "pmap_zero_page: pa=%#lx", pa);

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(CADDR2);
	tte.tte_data = TD_V | TD_8K | TD_PA(pa) | TD_L | TD_CP | TD_P | TD_W;
	tlb_store(TLB_DTLB, CADDR2, TLB_CTX_KERNEL, tte);
	bzero((void *)CADDR2, PAGE_SIZE);
	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, CADDR2);
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{
	struct tte tte;

	CTR3(KTR_PMAP, "pmap_zero_page_area: pa=%#lx off=%#x size=%#x",
	    pa, off, size);

	KASSERT(off + size <= PAGE_SIZE, ("pmap_zero_page_area: bad off/size"));
	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(CADDR2);
	tte.tte_data = TD_V | TD_8K | TD_PA(pa) | TD_L | TD_CP | TD_P | TD_W;
	tlb_store(TLB_DTLB, CADDR2, TLB_CTX_KERNEL, tte);
	bzero((char *)CADDR2 + off, size);
	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, CADDR2);
}

vm_offset_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	struct stte *stp;

	stp = tsb_stte_lookup(pmap, va);
	if (stp == NULL)
		return (0);
	else
		return (TD_PA(stp->st_tte.tte_data) | (va & PAGE_MASK));
}

vm_offset_t
pmap_kextract(vm_offset_t va)
{
	struct stte *stp;

	stp = tsb_kvtostte(va);
	KASSERT((stp->st_tte.tte_data & TD_V) != 0,
	    ("pmap_kextract: invalid virtual address 0x%lx", va));
	return (TD_PA(stp->st_tte.tte_data) | (va & PAGE_MASK));
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
	/* XXX */
}

boolean_t
pmap_page_exists(pmap_t pm, vm_page_t m)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;
	u_long tag;

	if (m->flags & PG_FICTITIOUS)
		return (FALSE);
	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		tag = pv_get_tte_tag(pstp);
		if (TT_GET_CTX(tag) == pm->pm_context) {
			PV_UNLOCK();
			return (TRUE);
		}
	}
	PV_UNLOCK();
	return (FALSE);
}

void
pmap_prefault(pmap_t pm, vm_offset_t va, vm_map_entry_t entry)
{
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_prefault: non current pmap"));
	/* XXX */
}

void
pmap_protect(pmap_t pm, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	struct stte *stp;
	vm_page_t m;
	u_long data;

	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_protect: non current pmap"));

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pm, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	for (; sva < eva; sva += PAGE_SIZE) {
		if ((stp = tsb_stte_lookup(pm, sva)) != NULL) {
			data = stp->st_tte.tte_data;
			if ((data & TD_MNG) != 0) {
				m = NULL;
				if ((data & TD_REF) != 0) {
					m = PHYS_TO_VM_PAGE(TD_PA(data));
					vm_page_flag_set(m, PG_REFERENCED);
					data &= ~TD_REF;
				}
				if ((data & TD_W) != 0 &&
				    pmap_track_modified(sva)) {
					if (m == NULL)
						m = PHYS_TO_VM_PAGE(TD_PA(data));
					vm_page_dirty(m);
					data &= ~TD_W;
				}
			}
	
			data &= ~TD_SW;
	
			if (data != stp->st_tte.tte_data) {
				stp->st_tte.tte_data = data;
				tsb_tte_local_remove(&stp->st_tte);
			}
		}
	}
}

vm_offset_t
pmap_phys_address(int ppn)
{

	return (sparc64_ptob(ppn));
}

void
pmap_reference(pmap_t pm)
{
	if (pm != NULL)
		pm->pm_count++;
}

void
pmap_release(pmap_t pm)
{
	/* XXX */
}

void
pmap_remove_pages(pmap_t pm, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_remove_pages: non current pmap"));
	/* XXX */
}
