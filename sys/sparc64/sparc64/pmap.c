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
#include <sys/kernel.h>
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

#include <machine/cache.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pv.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/tsb.h>

#define	PMAP_DEBUG

#ifndef	PMAP_SHPGPERPROC
#define	PMAP_SHPGPERPROC	200
#endif

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

int pmap_pagedaemon_waken;

/*
 * Map of physical memory reagions.
 */
vm_offset_t phys_avail[128];
static struct mem_region mra[128];
static struct ofw_map translations[128];
static int translations_size;

/*
 * First and last available kernel virtual addresses.
 */
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;

/*
 * The locked kernel page the kernel binary was loaded into. This will need
 * to become a list later.
 */
vm_offset_t kernel_page;

/*
 * Kernel pmap.
 */
struct pmap kernel_pmap_store;

/*
 * Map of free and in use hardware contexts and index of first potentially
 * free context.
 */
static char pmap_context_map[PMAP_CONTEXT_MAX];
static u_int pmap_context_base;

/*
 * Virtual addresses of free space for temporary mappings.  Used for copying,
 * zeroing and mapping physical pages for /dev/mem accesses.
 */
static vm_offset_t CADDR1;
static vm_offset_t CADDR2;

static boolean_t pmap_initialized = FALSE;

/* Convert a tte data field into a page mask */
static vm_offset_t pmap_page_masks[] = {
	PAGE_MASK_8K,
	PAGE_MASK_64K,
	PAGE_MASK_512K,
	PAGE_MASK_4M
};

#define	PMAP_TD_GET_MASK(d)	pmap_page_masks[TD_GET_SIZE((d))]

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
 * If user pmap is processed with pmap_remove and with pmap_remove and the
 * resident count drops to 0, there are no more pages to remove, so we
 * need not continue.
 */
#define	PMAP_REMOVE_DONE(pm) \
	((pm) != kernel_pmap && (pm)->pm_stats.resident_count == 0)

/*
 * The threshold (in bytes) above which tsb_foreach() is used in pmap_remove()
 * and pmap_protect() instead of trying each virtual address.
 */
#define	PMAP_TSB_THRESH	((TSB_SIZE / 2) * PAGE_SIZE)

/* Callbacks for tsb_foreach. */
tsb_callback_t pmap_remove_tte;
tsb_callback_t pmap_protect_tte;

/*
 * Quick sort callout for comparing memory regions.
 */
static int mr_cmp(const void *a, const void *b);
static int om_cmp(const void *a, const void *b);
static int
mr_cmp(const void *a, const void *b)
{
	return ((const struct mem_region *)a)->mr_start -
	    ((const struct mem_region *)b)->mr_start;
}
static int
om_cmp(const void *a, const void *b)
{
	return ((const struct ofw_map *)a)->om_start -
	    ((const struct ofw_map *)b)->om_start;
}

/*
 * Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t ekva)
{
	struct pmap *pm;
	struct tte tte;
	struct tte *tp;
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

	/* Look up the page the kernel binary was loaded into. */
	kernel_page = TD_GET_PA(ldxa(TLB_DAR_SLOT(TLB_SLOT_KERNEL),
	    ASI_DTLB_DATA_ACCESS_REG));

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
	qsort(mra, sz, sizeof (*mra), mr_cmp);
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
	tsb_kernel = (struct tte *)virtual_avail;
	virtual_avail += KVA_PAGES * PAGE_SIZE_4M;
	for (i = 0; i < KVA_PAGES; i++) {
		va = (vm_offset_t)tsb_kernel + i * PAGE_SIZE_4M;
		tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
		tte.tte_data = TD_V | TD_4M | TD_VA_LOW(va) | TD_PA(pa) |
		    TD_L | TD_CP | TD_CV | TD_P | TD_W;
		tlb_store_slot(TLB_DTLB, va, TLB_CTX_KERNEL, tte,
		    TLB_SLOT_TSB_KERNEL_MIN + i);
	}
	bzero(tsb_kernel, KVA_PAGES * PAGE_SIZE_4M);

	/*
	 * Load the tsb registers.
	 */
	stxa(AA_DMMU_TSB, ASI_DMMU, (vm_offset_t)tsb_kernel);
	stxa(AA_IMMU_TSB, ASI_IMMU, (vm_offset_t)tsb_kernel);
	membar(Sync);
	flush(tsb_kernel);

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
	if (sizeof(translations) < sz)
		panic("pmap_bootstrap: translations too small");
	bzero(translations, sz);
	if (OF_getprop(vmem, "translations", translations, sz) == -1)
		panic("pmap_bootstrap: getprop /virtual-memory/translations");
	sz /= sizeof(*translations);
	translations_size = sz;
	CTR0(KTR_PMAP, "pmap_bootstrap: translations");
	qsort(translations, sz, sizeof (*translations), om_cmp);
	for (i = 0; i < sz; i++) {
		CTR4(KTR_PMAP,
		    "translation: start=%#lx size=%#lx tte=%#lx pa=%#lx",
		    translations[i].om_start, translations[i].om_size,
		    translations[i].om_tte, TD_GET_PA(translations[i].om_tte));
		if (translations[i].om_start < 0xf0000000)	/* XXX!!! */
			continue;
		for (off = 0; off < translations[i].om_size;
		    off += PAGE_SIZE) {
			va = translations[i].om_start + off;
			tte.tte_data = translations[i].om_tte + off;
			tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
			tp = tsb_kvtotte(va);
			CTR4(KTR_PMAP,
			    "mapping: va=%#lx tp=%p tte=%#lx pa=%#lx",
			    va, tp, tte.tte_data, TD_GET_PA(tte.tte_data));
			*tp = tte;
		}
	}

	/*
	 * Calculate the first and last available physical addresses.
	 */
	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	avail_end = phys_avail[i + 1];
	Maxmem = sparc64_btop(avail_end);

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
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	pm = kernel_pmap;
	pm->pm_context = TLB_CTX_KERNEL;
	pm->pm_active = ~0;
	pm->pm_count = 1;
	TAILQ_INIT(&pm->pm_pvlist);

	/*
	 * Set the secondary context to be the kernel context (needed for
	 * fp block operations in the kernel and the cache code).
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
 * Initialize the pmap module.
 */
void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
	vm_offset_t addr;
	vm_size_t size;
	int result;
	int i;

	for (i = 0; i < vm_page_array_size; i++) {
		vm_page_t m;

		m = &vm_page_array[i];
		TAILQ_INIT(&m->md.pv_list);
		m->md.pv_list_count = 0;
	}

	for (i = 0; i < translations_size; i++) {
		addr = translations[i].om_start;
		size = translations[i].om_size;
		if (addr < 0xf0000000)	/* XXX */
			continue;
		result = vm_map_find(kernel_map, NULL, 0, &addr, size, TRUE,
		    VM_PROT_ALL, VM_PROT_ALL, 0);
		if (result != KERN_SUCCESS || addr != translations[i].om_start)
			panic("pmap_init: vm_map_find");
	}

	pvzone = &pvzone_store;
	pvinit = (struct pv_entry *)kmem_alloc(kernel_map,
	    vm_page_array_size * sizeof (struct pv_entry));
	zbootinit(pvzone, "PV ENTRY", sizeof (struct pv_entry), pvinit,
	    vm_page_array_size);
	pmap_initialized = TRUE;
}

/*
 * Initialize the address space (zone) for the pv_entries.  Set a
 * high water mark so that the system can recover from excessive
 * numbers of pv entries.
 */
void
pmap_init2(void)
{
	int shpgperproc;

	shpgperproc = PMAP_SHPGPERPROC;
	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + vm_page_array_size;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	zinitna(pvzone, &pvzone_obj, NULL, 0, pv_entry_max, ZONE_INTERRUPT, 1);
}

/*
 * Extract the physical page address associated with the given
 * map/virtual_address pair.
 */
vm_offset_t
pmap_extract(pmap_t pm, vm_offset_t va)
{
	struct tte *tp;
	u_long d;

	if (pm == kernel_pmap)
		return (pmap_kextract(va));
	tp = tsb_tte_lookup(pm, va);
	if (tp == NULL)
		return (0);
	else {
		d = tp->tte_data;
		return (TD_GET_PA(d) | (va & PMAP_TD_GET_MASK(d)));
	}
}

/*
 * Extract the physical page address associated with the given kernel virtual
 * address.
 */
vm_offset_t
pmap_kextract(vm_offset_t va)
{
	struct tte *tp;
	u_long d;

	if (va >= KERNBASE && va < KERNBASE + PAGE_SIZE_4M)
		return (kernel_page + (va & PAGE_MASK_4M));
	tp = tsb_kvtotte(va);
	d = tp->tte_data;
	if ((d & TD_V) == 0)
		return (0);
	return (TD_GET_PA(d) | (va & PMAP_TD_GET_MASK(d)));
}

int
pmap_cache_enter(vm_page_t m, vm_offset_t va)
{
	struct tte *tp;
	vm_offset_t pa;
	pv_entry_t pv;
	int c;
	int i;

	CTR2(KTR_PMAP, "pmap_cache_enter: m=%p va=%#lx", m, va);
	for (i = 0, c = 0; i < DCACHE_COLORS; i++) {
		if (i != DCACHE_COLOR(va))
			c += m->md.colors[i];
	}
	m->md.colors[DCACHE_COLOR(va)]++;
	if (c == 0) {
		CTR0(KTR_PMAP, "pmap_cache_enter: cacheable");
		return (1);
	}
	else if (c != 1) {
		CTR0(KTR_PMAP, "pmap_cache_enter: already uncacheable");
		return (0);
	}
	CTR0(KTR_PMAP, "pmap_cache_enter: marking uncacheable");
	if ((m->flags & PG_UNMANAGED) != 0)
		panic("pmap_cache_enter: non-managed page");
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if ((tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va)) != NULL) {
			atomic_clear_long(&tp->tte_data, TD_CV);
			tlb_page_demap(TLB_DTLB | TLB_ITLB,
			    pv->pv_pmap->pm_context, pv->pv_va);
		}
	}
	pa = VM_PAGE_TO_PHYS(m);
	dcache_inval_phys(pa, pa + PAGE_SIZE - 1);
	return (0);
}

void
pmap_cache_remove(vm_page_t m, vm_offset_t va)
{

	CTR3(KTR_PMAP, "pmap_cache_remove: m=%p va=%#lx c=%d", m, va,
	    m->md.colors[DCACHE_COLOR(va)]);
	KASSERT(m->md.colors[DCACHE_COLOR(va)] > 0,
	    ("pmap_cache_remove: no mappings %d <= 0",
	    m->md.colors[DCACHE_COLOR(va)]));
	m->md.colors[DCACHE_COLOR(va)]--;
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	struct tte tte;
	struct tte *tp;

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_REF | TD_SW | TD_CP | TD_CV | TD_P | TD_W;
	tp = tsb_kvtotte(va);
	CTR4(KTR_PMAP, "pmap_kenter: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, pa, tp, tp->tte_data);
	if ((tp->tte_data & TD_V) != 0)
		tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, va);
	*tp = tte;
}

/*
 * Map a wired page into kernel virtual address space. This additionally
 * takes a flag argument wich is or'ed to the TTE data. This is used by
 * bus_space_map().
 * NOTE: if the mapping is non-cacheable, it's the caller's responsibility
 * to flush entries that might still be in the cache, if applicable.
 */
void
pmap_kenter_flags(vm_offset_t va, vm_offset_t pa, u_long flags)
{
	struct tte tte;
	struct tte *tp;

	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) |
	    TD_REF | TD_P | flags;
	tp = tsb_kvtotte(va);
	CTR4(KTR_PMAP, "pmap_kenter_flags: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, pa, tp, tp->tte_data);
	if ((tp->tte_data & TD_V) != 0)
		tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, va);
	*tp = tte;
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
	struct tte *tp;

	tp = tsb_kvtotte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx tp=%p data=%#lx", va, tp,
	    tp->tte_data);
	atomic_clear_long(&tp->tte_data, TD_V);
	tp->tte_tag = 0;
	tp->tte_data = 0;
	tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, va);
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
 * As above, but take an additional flags argument and call
 * pmap_kenter_flags().
 */
void
pmap_qenter_flags(vm_offset_t va, vm_page_t *m, int count, u_long fl)
{
	int i;

	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kenter_flags(va, VM_PAGE_TO_PHYS(m[i]), fl);
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
 * Create the uarea for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
pmap_new_proc(struct proc *p)
{
	vm_object_t upobj;
	vm_offset_t up;
	vm_page_t m;
	u_int i;

	/*
	 * Allocate object for the upages.
	 */
	upobj = p->p_upages_obj;
	if (upobj == NULL) {
		upobj = vm_object_allocate(OBJT_DEFAULT, UAREA_PAGES);
		p->p_upages_obj = upobj;
	}

	/*
	 * Get a kernel virtual address for the U area for this process.
	 */
	up = (vm_offset_t)p->p_uarea;
	if (up == 0) {
		up = kmem_alloc_nofault(kernel_map, UAREA_PAGES * PAGE_SIZE);
		if (up == 0)
			panic("pmap_new_proc: upage allocation failed");
		p->p_uarea = (struct user *)up;
	}

	for (i = 0; i < UAREA_PAGES; i++) {
		/*
		 * Get a uarea page.
		 */
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page.
		 */
		m->wire_count++;
		cnt.v_wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		pmap_kenter(up + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

/*
 * Dispose the uarea for a process that has exited.
 * This routine directly impacts the exit perf of a process.
 */
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
 * Allow the uarea for a process to be prejudicially paged out.
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
 * Bring the uarea for a specified process back in.
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
 * Create the kernel stack and pcb for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
void
pmap_new_thread(struct thread *td)
{
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	u_int i;

	/*
	 * Allocate object for the kstack,
	 */
	ksobj = td->td_kstack_obj;
	if (ksobj == NULL) {
		ksobj = vm_object_allocate(OBJT_DEFAULT, KSTACK_PAGES);
		td->td_kstack_obj = ksobj;
	}

	/*
	 * Get a kernel virtual address for the kstack for this thread.
	 */
	ks = td->td_kstack;
	if (ks == 0) {
		ks = kmem_alloc_nofault(kernel_map,
		   (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE);
		if (ks == 0)
			panic("pmap_new_thread: kstack allocation failed");
		tlb_page_demap(TLB_DTLB, TLB_CTX_KERNEL, ks);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
		td->td_kstack = ks;
	}

	for (i = 0; i < KSTACK_PAGES; i++) {
		/*
		 * Get a kernel stack page.
		 */
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page.
		 */
		m->wire_count++;
		cnt.v_wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

/*
 * Dispose the kernel stack for a thread that has exited.
 * This routine directly impacts the exit perf of a process and thread.
 */
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
			panic("pmap_dispose_proc: kstack already missing?");
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

/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pm)
{

	pm->pm_context = pmap_context_alloc();
	pm->pm_active = 0;
	pm->pm_count = 1;
	pm->pm_tsb = NULL;
	pm->pm_tsb_obj = NULL;
	pm->pm_tsb_tte = NULL;
	TAILQ_INIT(&pm->pm_pvlist);
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

/*
 * Initialize a preallocated and zeroed pmap structure, uch as one in a
 * vmspace structure.
 */
void
pmap_pinit(pmap_t pm)
{
	vm_page_t m;

	/*
	 * Allocate kva space for the tsb.
	 */
	if (pm->pm_tsb == NULL) {
		pm->pm_tsb = (struct tte *)kmem_alloc_pageable(kernel_map,
		    PAGE_SIZE_8K);
		pm->pm_tsb_tte = tsb_kvtotte((vm_offset_t)pm->pm_tsb);
	}

	/*
	 * Allocate an object for it.
	 */
	if (pm->pm_tsb_obj == NULL)
		pm->pm_tsb_obj = vm_object_allocate(OBJT_DEFAULT, 1);

	/*
	 * Allocate the tsb page.
	 */
	m = vm_page_grab(pm->pm_tsb_obj, 0, VM_ALLOC_RETRY | VM_ALLOC_ZERO);
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(VM_PAGE_TO_PHYS(m));

	m->wire_count++;
	cnt.v_wire_count++;

	vm_page_flag_clear(m, PG_MAPPED | PG_BUSY);
	m->valid = VM_PAGE_BITS_ALL;

	pmap_kenter((vm_offset_t)pm->pm_tsb, VM_PAGE_TO_PHYS(m));

	pm->pm_active = 0;
	pm->pm_context = pmap_context_alloc();
	pm->pm_count = 1;
	TAILQ_INIT(&pm->pm_pvlist);
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

void
pmap_pinit2(pmap_t pmap)
{
	/* XXX: Remove this stub when no longer called */
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pm)
{
#ifdef INVARIANTS
	pv_entry_t pv;
#endif
	vm_object_t obj;
	vm_page_t m;

	CTR2(KTR_PMAP, "pmap_release: ctx=%#x tsb=%p", pm->pm_context,
	    pm->pm_tsb);
	obj = pm->pm_tsb_obj;
	KASSERT(obj->ref_count == 1, ("pmap_release: tsbobj ref count != 1"));
#ifdef INVARIANTS
	if (!TAILQ_EMPTY(&pm->pm_pvlist)) {
		TAILQ_FOREACH(pv, &pm->pm_pvlist, pv_plist) {
			CTR3(KTR_PMAP, "pmap_release: m=%p va=%#lx pa=%#lx",
			    pv->pv_m, pv->pv_va, VM_PAGE_TO_PHYS(pv->pv_m));
		}
		panic("pmap_release: leaking pv entries");
	}
#endif
	KASSERT(pmap_resident_count(pm) == 0,
	    ("pmap_release: resident pages %ld != 0",
	    pmap_resident_count(pm)));
	m = TAILQ_FIRST(&obj->memq);
	pmap_context_destroy(pm->pm_context);
	if (vm_page_sleep_busy(m, FALSE, "pmaprl"))
		return;
	vm_page_busy(m);
	KASSERT(m->hold_count == 0, ("pmap_release: freeing held tsb page"));
	pmap_kremove((vm_offset_t)pm->pm_tsb);
	m->wire_count--;
	cnt.v_wire_count--;
	vm_page_free_zero(m);
}

/*
 * Grow the number of kernel page table entries.  Unneeded.
 */
void
pmap_growkernel(vm_offset_t addr)
{
}

/*
 * Retire the given physical map from service.  Pmaps are always allocated
 * as part of a larger structure, so this never happens.
 */
void
pmap_destroy(pmap_t pm)
{
	panic("pmap_destroy: unimplemented");
}

/*
 * Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pm)
{
	if (pm != NULL)
		pm->pm_count++;
}

/*
 * This routine is very drastic, but can save the system
 * in a pinch.
 */
void
pmap_collect(void)
{
	static int warningdone;
	vm_page_t m;
	int i;

	if (pmap_pagedaemon_waken == 0)
		return;
	if (warningdone++ < 5)
		printf("pmap_collect: collecting pv entries -- suggest"
		    "increasing PMAP_SHPGPERPROC\n");
	for (i = 0; i < vm_page_array_size; i++) {
		m = &vm_page_array[i];
		if (m->wire_count || m->hold_count || m->busy ||
		    (m->flags & (PG_BUSY | PG_UNMANAGED)))
			continue;
		pv_remove_all(m);
	}
	pmap_pagedaemon_waken = 0;
}

int
pmap_remove_tte(struct pmap *pm, struct tte *tp, vm_offset_t va)
{
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(TD_GET_PA(tp->tte_data));
	if ((tp->tte_data & TD_PV) != 0) {
		if ((tp->tte_data & TD_W) != 0 &&
		    pmap_track_modified(pm, va))
			vm_page_dirty(m);
		if ((tp->tte_data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		pv_remove(pm, m, va);
		pmap_cache_remove(m, va);
	}
	atomic_clear_long(&tp->tte_data, TD_V);
	tp->tte_tag = 0;
	tp->tte_data = 0;
	tlb_page_demap(TLB_ITLB | TLB_DTLB,
	    pm->pm_context, va);
	if (PMAP_REMOVE_DONE(pm))
		return (0);
	return (1);
}

/*
 * Remove the given range of addresses from the specified map.
 */
void
pmap_remove(pmap_t pm, vm_offset_t start, vm_offset_t end)
{
	struct tte *tp;

	CTR3(KTR_PMAP, "pmap_remove: ctx=%#lx start=%#lx end=%#lx",
	    pm->pm_context, start, end);
	if (PMAP_REMOVE_DONE(pm))
		return;
	if (end - start > PMAP_TSB_THRESH)
		tsb_foreach(pm, start, end, pmap_remove_tte);
	else {
		for (; start < end; start += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, start)) != NULL) {
				if (!pmap_remove_tte(pm, tp, start))
					break;
			}
		}
	}
}

int
pmap_protect_tte(struct pmap *pm, struct tte *tp, vm_offset_t va)
{
	vm_page_t m;
	u_long data;

	data = tp->tte_data;
	if ((data & TD_PV) != 0) {
		m = PHYS_TO_VM_PAGE(TD_GET_PA(data));
		if ((data & TD_REF) != 0) {
			vm_page_flag_set(m, PG_REFERENCED);
			data &= ~TD_REF;
		}
		if ((data & TD_W) != 0 &&
		    pmap_track_modified(pm, va)) {
			vm_page_dirty(m);
		}
	}

	data &= ~(TD_W | TD_SW);

	CTR2(KTR_PMAP, "pmap_protect: new=%#lx old=%#lx",
	    data, tp->tte_data);

	if (data != tp->tte_data) {
		CTR0(KTR_PMAP, "pmap_protect: demap");
		tlb_page_demap(TLB_DTLB | TLB_ITLB,
		    pm->pm_context, va);
		tp->tte_data = data;
	}
	return (0);
}

/*
 * Set the physical protection on the specified range of this map as requested.
 */
void
pmap_protect(pmap_t pm, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	struct tte *tp;

	CTR4(KTR_PMAP, "pmap_protect: ctx=%#lx sva=%#lx eva=%#lx prot=%#lx",
	    pm->pm_context, sva, eva, prot);

	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_protect: non current pmap"));

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pm, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	if (eva - sva > PMAP_TSB_THRESH)
		tsb_foreach(pm, sva, eva, pmap_protect_tte);
	else {
		for (; sva < eva; sva += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, sva)) != NULL)
				pmap_protect_tte(pm, tp, sva);
		}
	}
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
	struct tte otte;
	struct tte tte;
	struct tte *tp;
	vm_offset_t pa;
	vm_page_t om;

	pa = VM_PAGE_TO_PHYS(m);
	CTR6(KTR_PMAP,
	    "pmap_enter: ctx=%p m=%p va=%#lx pa=%#lx prot=%#x wired=%d",
	    pm->pm_context, m, va, pa, prot, wired);

	tte.tte_tag = TT_CTX(pm->pm_context) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) | TD_CP;

	/*
	 * If there is an existing mapping, and the physical address has not
	 * changed, must be protection or wiring change.
	 */
	if ((tp = tsb_tte_lookup(pm, va)) != NULL) {
		otte = *tp;
		om = PHYS_TO_VM_PAGE(TD_GET_PA(otte.tte_data));

		if (TD_GET_PA(otte.tte_data) == pa) {
			CTR0(KTR_PMAP, "pmap_enter: update");

			/*
			 * Wiring change, just update stats.
			 */
			if (wired) {
				if ((otte.tte_data & TD_WIRED) == 0)
					pm->pm_stats.wired_count++;
			} else {
				if ((otte.tte_data & TD_WIRED) != 0)
					pm->pm_stats.wired_count--;
			}

			if ((otte.tte_data & TD_CV) != 0)	
				tte.tte_data |= TD_CV;
			if ((otte.tte_data & TD_REF) != 0)
				tte.tte_data |= TD_REF;
			if ((otte.tte_data & TD_PV) != 0) {
				KASSERT((m->flags &
				    (PG_FICTITIOUS|PG_UNMANAGED)) == 0,
				    ("pmap_enter: unmanaged pv page"));
				tte.tte_data |= TD_PV;
			}
			/*
			 * If we're turning off write protection, sense modify
			 * status and remove the old mapping.
			 */
			if ((prot & VM_PROT_WRITE) == 0 &&
			    (otte.tte_data & (TD_W | TD_SW)) != 0) {
				if ((otte.tte_data & TD_PV) != 0) {
					if (pmap_track_modified(pm, va))
						vm_page_dirty(m);
				}
				tlb_page_demap(TLB_DTLB | TLB_ITLB,
				    TT_GET_CTX(otte.tte_tag), va);
			}
		} else {
			CTR0(KTR_PMAP, "pmap_enter: replace");

			/*
			 * Mapping has changed, invalidate old range.
			 */
			if (!wired && (otte.tte_data & TD_WIRED) != 0)
				pm->pm_stats.wired_count--;

			/*
			 * Enter on the pv list if part of our managed memory.
			 */
			if ((otte.tte_data & TD_PV) != 0) {
				KASSERT((m->flags &
				    (PG_FICTITIOUS|PG_UNMANAGED)) == 0,
				    ("pmap_enter: unmanaged pv page"));
				if ((otte.tte_data & TD_REF) != 0)
					vm_page_flag_set(om, PG_REFERENCED);
				if ((otte.tte_data & TD_W) != 0 &&
				    pmap_track_modified(pm, va))
					vm_page_dirty(om);
				pv_remove(pm, om, va);
				pv_insert(pm, m, va);
				tte.tte_data |= TD_PV;
				pmap_cache_remove(om, va);
				if (pmap_cache_enter(m, va) != 0)
					tte.tte_data |= TD_CV;
			}
			tlb_page_demap(TLB_DTLB | TLB_ITLB,
			    TT_GET_CTX(otte.tte_tag), va);
		}
	} else {
		CTR0(KTR_PMAP, "pmap_enter: new");

		/*
		 * Enter on the pv list if part of our managed memory.
		 */
		if (pmap_initialized &&
		    (m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0) {
			pv_insert(pm, m, va);
			tte.tte_data |= TD_PV;
			if (pmap_cache_enter(m, va) != 0)
				tte.tte_data |= TD_CV;
		}

		/*
		 * Increment counters.
		 */
		if (wired)
			pm->pm_stats.wired_count++;

	}

	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	if (wired) {
		tte.tte_data |= TD_REF | TD_WIRED;
		if ((prot & VM_PROT_WRITE) != 0)
			tte.tte_data |= TD_W;
	}
	if (pm->pm_context == TLB_CTX_KERNEL)
		tte.tte_data |= TD_P;
	if (prot & VM_PROT_WRITE)
		tte.tte_data |= TD_SW;
	if (prot & VM_PROT_EXECUTE) {
		tte.tte_data |= TD_EXEC;
		icache_inval_phys(pa, pa + PAGE_SIZE - 1);
	}

	if (tp != NULL)
		*tp = tte;
	else
		tsb_tte_enter(pm, m, va, tte);
}

void
pmap_object_init_pt(pmap_t pm, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size, int limit)
{
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_object_init_pt: non current pmap"));
	/* XXX */
}

void
pmap_prefault(pmap_t pm, vm_offset_t va, vm_map_entry_t entry)
{
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_prefault: non current pmap"));
	/* XXX */
}

/*
 * Change the wiring attribute for a map/virtual-address pair.
 * The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap_t pm, vm_offset_t va, boolean_t wired)
{
	struct tte *tp;

	if ((tp = tsb_tte_lookup(pm, va)) != NULL) {
		if (wired) {
			if ((tp->tte_data & TD_WIRED) == 0)
				pm->pm_stats.wired_count++;
			tp->tte_data |= TD_WIRED;
		} else {
			if ((tp->tte_data & TD_WIRED) != 0)
				pm->pm_stats.wired_count--;
			tp->tte_data &= ~TD_WIRED;
		}
	}
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	/* XXX */
}

/*
 * Zero a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_zero_page(vm_offset_t pa)
{

	CTR1(KTR_PMAP, "pmap_zero_page: pa=%#lx", pa);
	dcache_inval_phys(pa, pa + PAGE_SIZE);
	aszero(ASI_PHYS_USE_EC, pa, PAGE_SIZE);
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{

	CTR3(KTR_PMAP, "pmap_zero_page_area: pa=%#lx off=%#x size=%#x",
	    pa, off, size);
	KASSERT(off + size <= PAGE_SIZE, ("pmap_zero_page_area: bad off/size"));
	dcache_inval_phys(pa + off, pa + off + size);
	aszero(ASI_PHYS_USE_EC, pa + off, size);
}

/*
 * Copy a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{

	CTR2(KTR_PMAP, "pmap_copy_page: src=%#lx dst=%#lx", src, dst);
	dcache_inval_phys(dst, dst + PAGE_SIZE);
	ascopy(ASI_PHYS_USE_EC, src, dst, PAGE_SIZE);
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
 * Return true of a physical page resided in the given pmap.
 */
boolean_t
pmap_page_exists(pmap_t pm, vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return (FALSE);
	return (pv_page_exists(pm, m));
}

/*
 * Remove all pages from specified address space, this aids process exit
 * speeds.  This is much faster than pmap_remove n the case of running down
 * an entire address space.  Only works for the current pmap.
 */
void
pmap_remove_pages(pmap_t pm, vm_offset_t sva, vm_offset_t eva)
{
	struct tte *tp;
	pv_entry_t npv;
	pv_entry_t pv;
	vm_page_t m;

	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_remove_pages: non current pmap"));
	npv = NULL;
	for (pv = TAILQ_FIRST(&pm->pm_pvlist); pv != NULL; pv = npv) {
		npv = TAILQ_NEXT(pv, pv_plist);
		if (pv->pv_va >= eva || pv->pv_va < sva)
			continue;
		if ((tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va)) == NULL)
			continue;

		/*
		 * We cannot remove wired pages at this time.
		 */
		if ((tp->tte_data & TD_WIRED) != 0)
			continue;

		atomic_clear_long(&tp->tte_data, TD_V);
		tp->tte_tag = 0;
		tp->tte_data = 0;

		m = pv->pv_m;

		pv->pv_pmap->pm_stats.resident_count--;
		m->md.pv_list_count--;
		pmap_cache_remove(m, pv->pv_va);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		if (TAILQ_EMPTY(&m->md.pv_list))
			vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
		pv_free(pv);
	}
	tlb_context_primary_demap(TLB_DTLB | TLB_ITLB);
}

/*
 * Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{

	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
			pv_bit_clear(m, TD_W | TD_SW);
		else
			pv_remove_all(m);
	}
}

vm_offset_t
pmap_phys_address(int ppn)
{

	return (sparc64_ptob(ppn));
}

int
pmap_ts_referenced(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return (0);
	return (pv_bit_count(m, TD_REF));
}

boolean_t
pmap_is_modified(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return FALSE;
	return (pv_bit_test(m, TD_W));
}

void
pmap_clear_modify(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return;
	pv_bit_clear(m, TD_W);
}

void
pmap_clear_reference(vm_page_t m)
{

	if (m->flags & PG_FICTITIOUS)
		return;
	pv_bit_clear(m, TD_REF);
}

int
pmap_mincore(pmap_t pm, vm_offset_t addr)
{
	TODO;
	return (0);
}

/*
 * Activate a user pmap.  The pmap must be activated before its address space
 * can be accessed in any way.
 */
void
pmap_activate(struct thread *td)
{
	vm_offset_t tsb;
	u_long context;
	u_long data;
	pmap_t pm;

	/*
	 * Load all the data we need up front to encourage the compiler to
	 * not issue any loads while we have interrupts disable below.
	 */
	pm = &td->td_proc->p_vmspace->vm_pmap;
	context = pm->pm_context;
	data = pm->pm_tsb_tte->tte_data;
	tsb = (vm_offset_t)pm->pm_tsb;

	KASSERT(context != 0, ("pmap_activate: activating nucleus context"));
	KASSERT(context != -1, ("pmap_activate: steal context"));
	KASSERT(pm->pm_active == 0, ("pmap_activate: pmap already active?"));

	pm->pm_active |= PCPU_GET(cpumask);

	wrpr(pstate, 0, PSTATE_MMU);
	__asm __volatile("mov %0, %%g7" : : "r" (tsb));
	wrpr(pstate, 0, PSTATE_NORMAL);
	stxa(TLB_DEMAP_VA(tsb) | TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE,
	    ASI_DMMU_DEMAP, 0);
	membar(Sync);
	stxa(AA_DMMU_TAR, ASI_DMMU, tsb);
	stxa(0, ASI_DTLB_DATA_IN_REG, data | TD_L);
	membar(Sync);
	stxa(AA_DMMU_PCXR, ASI_DMMU, context);
	membar(Sync);
	wrpr(pstate, 0, PSTATE_KERNEL);
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{

	return (va);
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
