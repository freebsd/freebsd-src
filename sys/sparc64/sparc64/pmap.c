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
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
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
#include <vm/uma.h>

#include <machine/cache.h>
#include <machine/frame.h>
#include <machine/instr.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/smp.h>
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

vm_offset_t vm_max_kernel_address;

/*
 * Kernel pmap.
 */
struct pmap kernel_pmap_store;

static boolean_t pmap_initialized = FALSE;

/*
 * Allocate physical memory for use in pmap_bootstrap.
 */
static vm_offset_t pmap_bootstrap_alloc(vm_size_t size);

static vm_offset_t pmap_map_direct(vm_page_t m);

extern int tl1_immu_miss_load_tsb[];
extern int tl1_immu_miss_load_tsb_mask[];
extern int tl1_dmmu_miss_load_tsb[];
extern int tl1_dmmu_miss_load_tsb_mask[];
extern int tl1_dmmu_prot_load_tsb[];
extern int tl1_dmmu_prot_load_tsb_mask[];

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

#ifdef PMAP_STATS
static long pmap_enter_nupdate;
static long pmap_enter_nreplace;
static long pmap_enter_nnew;
static long pmap_ncache_enter;
static long pmap_ncache_enter_c;
static long pmap_ncache_enter_cc;
static long pmap_ncache_enter_nc;
static long pmap_ncache_remove;
static long pmap_ncache_remove_c;
static long pmap_ncache_remove_cc;
static long pmap_ncache_remove_nc;
static long pmap_niflush;

SYSCTL_NODE(_debug, OID_AUTO, pmap_stats, CTLFLAG_RD, 0, "Statistics");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_enter_nupdate, CTLFLAG_RD,
    &pmap_enter_nupdate, 0, "Number of pmap_enter() updates");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_enter_nreplace, CTLFLAG_RD,
    &pmap_enter_nreplace, 0, "Number of pmap_enter() replacements");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_enter_nnew, CTLFLAG_RD,
    &pmap_enter_nnew, 0, "Number of pmap_enter() additions");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_enter, CTLFLAG_RD,
    &pmap_ncache_enter, 0, "Number of pmap_cache_enter() calls");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_enter_c, CTLFLAG_RD,
    &pmap_ncache_enter_c, 0, "Number of pmap_cache_enter() cacheable");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_enter_cc, CTLFLAG_RD,
    &pmap_ncache_enter_cc, 0, "Number of pmap_cache_enter() change color");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_enter_nc, CTLFLAG_RD,
    &pmap_ncache_enter_nc, 0, "Number of pmap_cache_enter() noncacheable");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_remove, CTLFLAG_RD,
    &pmap_ncache_remove, 0, "Number of pmap_cache_remove() calls");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_remove_c, CTLFLAG_RD,
    &pmap_ncache_remove_c, 0, "Number of pmap_cache_remove() cacheable");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_remove_cc, CTLFLAG_RD,
    &pmap_ncache_remove_cc, 0, "Number of pmap_cache_remove() change color");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_ncache_remove_nc, CTLFLAG_RD,
    &pmap_ncache_remove_nc, 0, "Number of pmap_cache_remove() noncacheable");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, pmap_niflush, CTLFLAG_RD,
    &pmap_niflush, 0, "Number of pmap I$ flushes");

#define	PMAP_STATS_INC(var)	atomic_add_long(&var, 1)
#else
#define	PMAP_STATS_INC(var)
#endif

/*
 * Quick sort callout for comparing memory regions.
 */
static int mr_cmp(const void *a, const void *b);
static int om_cmp(const void *a, const void *b);
static int
mr_cmp(const void *a, const void *b)
{
	const struct mem_region *mra;
	const struct mem_region *mrb;

	mra = a;
	mrb = b;
	if (mra->mr_start < mrb->mr_start)
		return (-1);
	else if (mra->mr_start > mrb->mr_start)
		return (1);
	else
		return (0);
}
static int
om_cmp(const void *a, const void *b)
{
	const struct ofw_map *oma;
	const struct ofw_map *omb;

	oma = a;
	omb = b;
	if (oma->om_start < omb->om_start)
		return (-1);
	else if (oma->om_start > omb->om_start)
		return (1);
	else
		return (0);
}

/*
 * Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t ekva)
{
	struct pmap *pm;
	struct tte *tp;
	vm_offset_t off;
	vm_offset_t pa;
	vm_offset_t va;
	vm_size_t physsz;
	vm_size_t virtsz;
	ihandle_t pmem;
	ihandle_t vmem;
	int sz;
	int i;
	int j;

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
	physsz = 0;
	for (i = 0, j = 0; i < sz; i++, j += 2) {
		CTR2(KTR_PMAP, "start=%#lx size=%#lx", mra[i].mr_start,
		    mra[i].mr_size);
		phys_avail[j] = mra[i].mr_start;
		phys_avail[j + 1] = mra[i].mr_start + mra[i].mr_size;
		physsz += mra[i].mr_size;
	}
	physmem = btoc(physsz);

	virtsz = roundup(physsz, PAGE_SIZE_4M << (PAGE_SHIFT - TTE_SHIFT));
	vm_max_kernel_address = VM_MIN_KERNEL_ADDRESS + virtsz;
	tsb_kernel_size = virtsz >> (PAGE_SHIFT - TTE_SHIFT);
	tsb_kernel_mask = (tsb_kernel_size >> TTE_SHIFT) - 1;

	/*
	 * Set the start and end of kva.  The kernel is loaded at the first
	 * available 4 meg super page, so round up to the end of the page.
	 */
	virtual_avail = roundup2(ekva, PAGE_SIZE_4M);
	virtual_end = vm_max_kernel_address;
	kernel_vm_end = vm_max_kernel_address;

	/*
	 * Allocate the kernel tsb.
	 */
	pa = pmap_bootstrap_alloc(tsb_kernel_size);
	if (pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: tsb unaligned\n");
	tsb_kernel_phys = pa;
	tsb_kernel = (struct tte *)virtual_avail;
	virtual_avail += tsb_kernel_size;

	/*
	 * Patch the virtual address and the tsb mask into the trap table.
	 */
#define	SETHI_G4(x) \
	EIF_OP(IOP_FORM2) | EIF_F2_RD(4) | EIF_F2_OP2(INS0_SETHI) | \
	    EIF_IMM((x) >> 10, 22)
#define	OR_G4_I_G4(x) \
	EIF_OP(IOP_MISC) | EIF_F3_RD(4) | EIF_F3_OP3(INS2_OR) | \
	    EIF_F3_RS1(4) | EIF_F3_I(1) | EIF_IMM(x, 10)

	tl1_immu_miss_load_tsb[0] = SETHI_G4((vm_offset_t)tsb_kernel);
	tl1_immu_miss_load_tsb_mask[0] = SETHI_G4(tsb_kernel_mask);
	tl1_immu_miss_load_tsb_mask[1] = OR_G4_I_G4(tsb_kernel_mask);
	flush(tl1_immu_miss_load_tsb);
	flush(tl1_immu_miss_load_tsb_mask);
	flush(tl1_immu_miss_load_tsb_mask + 1);

	tl1_dmmu_miss_load_tsb[0] = SETHI_G4((vm_offset_t)tsb_kernel);
	tl1_dmmu_miss_load_tsb_mask[0] = SETHI_G4(tsb_kernel_mask);
	tl1_dmmu_miss_load_tsb_mask[1] = OR_G4_I_G4(tsb_kernel_mask);
	flush(tl1_dmmu_miss_load_tsb);
	flush(tl1_dmmu_miss_load_tsb_mask);
	flush(tl1_dmmu_miss_load_tsb_mask + 1);

	tl1_dmmu_prot_load_tsb[0] = SETHI_G4((vm_offset_t)tsb_kernel);
	tl1_dmmu_prot_load_tsb_mask[0] = SETHI_G4(tsb_kernel_mask);
	tl1_dmmu_prot_load_tsb_mask[1] = OR_G4_I_G4(tsb_kernel_mask);
	flush(tl1_dmmu_prot_load_tsb);
	flush(tl1_dmmu_prot_load_tsb_mask);
	flush(tl1_dmmu_prot_load_tsb_mask + 1);

	/*
	 * Lock it in the tlb.
	 */
	pmap_map_tsb();
	bzero(tsb_kernel, tsb_kernel_size);

	/*
	 * Enter fake 8k pages for the 4MB kernel pages, so that
	 * pmap_kextract() will work for them.
	 */
	for (i = 0; i < kernel_tlb_slots; i++) {
		pa = kernel_tlbs[i].te_pa;
		va = kernel_tlbs[i].te_va;
		for (off = 0; off < PAGE_SIZE_4M; off += PAGE_SIZE) {
			tp = tsb_kvtotte(va + off);
			tp->tte_vpn = TV_VPN(va + off);
			tp->tte_data = TD_V | TD_8K | TD_PA(pa + off) |
			    TD_REF | TD_SW | TD_CP | TD_CV | TD_P | TD_W;
		}
	}

	/*
	 * Allocate a kernel stack with guard page for thread0 and map it into
	 * the kernel tsb.
	 */
	pa = pmap_bootstrap_alloc(KSTACK_PAGES * PAGE_SIZE);
	kstack0_phys = pa;
	kstack0 = virtual_avail + (KSTACK_GUARD_PAGES * PAGE_SIZE);
	virtual_avail += (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE;
	for (i = 0; i < KSTACK_PAGES; i++) {
		pa = kstack0_phys + i * PAGE_SIZE;
		va = kstack0 + i * PAGE_SIZE;
		tp = tsb_kvtotte(va);
		tp->tte_vpn = TV_VPN(va);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_SW |
		    TD_CP | TD_CV | TD_P | TD_W;
	}

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
		CTR3(KTR_PMAP,
		    "translation: start=%#lx size=%#lx tte=%#lx",
		    translations[i].om_start, translations[i].om_size,
		    translations[i].om_tte);
		if (translations[i].om_start < VM_MIN_PROM_ADDRESS ||
		    translations[i].om_start > VM_MAX_PROM_ADDRESS)
			continue;
		for (off = 0; off < translations[i].om_size;
		    off += PAGE_SIZE) {
			va = translations[i].om_start + off;
			tp = tsb_kvtotte(va);
			tp->tte_vpn = TV_VPN(va);
			tp->tte_data = translations[i].om_tte + off;
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
	 * Allocate virtual address space for the message buffer.
	 */
	msgbufp = (struct msgbuf *)virtual_avail;
	virtual_avail += round_page(MSGBUF_SIZE);

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	pm = kernel_pmap;
	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = TLB_CTX_KERNEL;
	pm->pm_active = ~0;

	/* XXX flush all non-locked tlb entries */
}

void
pmap_map_tsb(void)
{
	vm_offset_t va;
	vm_offset_t pa;
	u_long data;
	u_long s;
	int i;

	s = intr_disable();

	/*
	 * Map the 4mb tsb pages.
	 */
	for (i = 0; i < tsb_kernel_size; i += PAGE_SIZE_4M) {
		va = (vm_offset_t)tsb_kernel + i;
		pa = tsb_kernel_phys + i;
		/* XXX - cheetah */
		data = TD_V | TD_4M | TD_PA(pa) | TD_L | TD_CP | TD_CV |
		    TD_P | TD_W;
		stxa(AA_DMMU_TAR, ASI_DMMU, TLB_TAR_VA(va) |
		    TLB_TAR_CTX(TLB_CTX_KERNEL));
		stxa_sync(0, ASI_DTLB_DATA_IN_REG, data);
	}

	/*
	 * Set the secondary context to be the kernel context (needed for
	 * fp block operations in the kernel and the cache code).
	 */
	stxa(AA_DMMU_SCXR, ASI_DMMU, TLB_CTX_KERNEL);
	membar(Sync);

	intr_restore(s);
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

void
pmap_context_rollover(void)
{
	u_long data;
	u_long tag;
	int i;

	mtx_assert(&sched_lock, MA_OWNED);
	CTR0(KTR_PMAP, "pmap_context_rollover");
	for (i = 0; i < tlb_dtlb_entries; i++) {
		/* XXX - cheetah */
		data = ldxa(TLB_DAR_SLOT(i), ASI_DTLB_DATA_ACCESS_REG);
		tag = ldxa(TLB_DAR_SLOT(i), ASI_DTLB_TAG_READ_REG);
		if ((data & TD_V) != 0 && (data & TD_L) == 0 &&
		    TLB_TAR_CTX(tag) != TLB_CTX_KERNEL)
			stxa_sync(TLB_DAR_SLOT(i), ASI_DTLB_DATA_ACCESS_REG, 0);
		data = ldxa(TLB_DAR_SLOT(i), ASI_ITLB_DATA_ACCESS_REG);
		tag = ldxa(TLB_DAR_SLOT(i), ASI_ITLB_TAG_READ_REG);
		if ((data & TD_V) != 0 && (data & TD_L) == 0 &&
		    TLB_TAR_CTX(tag) != TLB_CTX_KERNEL)
			stxa_sync(TLB_DAR_SLOT(i), ASI_ITLB_DATA_ACCESS_REG, 0);
	}
	PCPU_SET(tlb_ctx, PCPU_GET(tlb_ctx_min));
}

static __inline u_int
pmap_context_alloc(void)
{
	u_int context;

	mtx_assert(&sched_lock, MA_OWNED);
	context = PCPU_GET(tlb_ctx);
	if (context + 1 == PCPU_GET(tlb_ctx_max))
		pmap_context_rollover();
	else
		PCPU_SET(tlb_ctx, context + 1);
	return (context);
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
		STAILQ_INIT(&m->md.tte_list);
		m->md.flags = 0;
		m->md.color = 0;
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
}

/*
 * Extract the physical page address associated with the given
 * map/virtual_address pair.
 */
vm_offset_t
pmap_extract(pmap_t pm, vm_offset_t va)
{
	struct tte *tp;

	if (pm == kernel_pmap)
		return (pmap_kextract(va));
	tp = tsb_tte_lookup(pm, va);
	if (tp == NULL)
		return (0);
	else
		return (TTE_GET_PA(tp) | (va & TTE_GET_PAGE_MASK(tp)));
}

/*
 * Extract the physical page address associated with the given kernel virtual
 * address.
 */
vm_offset_t
pmap_kextract(vm_offset_t va)
{
	struct tte *tp;

	tp = tsb_kvtotte(va);
	if ((tp->tte_data & TD_V) == 0)
		return (0);
	return (TTE_GET_PA(tp) | (va & TTE_GET_PAGE_MASK(tp)));
}

int
pmap_cache_enter(vm_page_t m, vm_offset_t va)
{
	struct tte *tp;
	int color;

	PMAP_STATS_INC(pmap_ncache_enter);

	/*
	 * Find the color for this virtual address and note the added mapping.
	 */
	color = DCACHE_COLOR(va);
	m->md.colors[color]++;

	/*
	 * If all existing mappings have the same color, the mapping is
	 * cacheable.
	 */
	if (m->md.color == color) {
		KASSERT(m->md.colors[DCACHE_OTHER_COLOR(color)] == 0,
		    ("pmap_cache_enter: cacheable, mappings of other color"));
		PMAP_STATS_INC(pmap_ncache_enter_c);
		return (1);
	}

	/*
	 * If there are no mappings of the other color, and the page still has
	 * the wrong color, this must be a new mapping.  Change the color to
	 * match the new mapping, which is cacheable.  We must flush the page
	 * from the cache now.
	 */
	if (m->md.colors[DCACHE_OTHER_COLOR(color)] == 0) {
		KASSERT(m->md.colors[color] == 1,
		    ("pmap_cache_enter: changing color, not new mapping"));
		dcache_page_inval(VM_PAGE_TO_PHYS(m));
		m->md.color = color;
		PMAP_STATS_INC(pmap_ncache_enter_cc);
		return (1);
	}

	PMAP_STATS_INC(pmap_ncache_enter_nc);

	/*
	 * If the mapping is already non-cacheable, just return.
	 */	
	if (m->md.color == -1)
		return (0);

	/*
	 * Mark all mappings as uncacheable, flush any lines with the other
	 * color out of the dcache, and set the color to none (-1).
	 */
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		tp->tte_data &= ~TD_CV;
		tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
	dcache_page_inval(VM_PAGE_TO_PHYS(m));
	m->md.color = -1;
	return (0);
}

void
pmap_cache_remove(vm_page_t m, vm_offset_t va)
{
	struct tte *tp;
	int color;

	CTR3(KTR_PMAP, "pmap_cache_remove: m=%p va=%#lx c=%d", m, va,
	    m->md.colors[DCACHE_COLOR(va)]);
	KASSERT(m->md.colors[DCACHE_COLOR(va)] > 0,
	    ("pmap_cache_remove: no mappings %d <= 0",
	    m->md.colors[DCACHE_COLOR(va)]));
	PMAP_STATS_INC(pmap_ncache_remove);

	/*
	 * Find the color for this virtual address and note the removal of
	 * the mapping.
	 */
	color = DCACHE_COLOR(va);
	m->md.colors[color]--;

	/*
	 * If the page is cacheable, just return and keep the same color, even
	 * if there are no longer any mappings.
	 */
	if (m->md.color != -1) {
		PMAP_STATS_INC(pmap_ncache_remove_c);
		return;
	}

	KASSERT(m->md.colors[DCACHE_OTHER_COLOR(color)] != 0,
	    ("pmap_cache_remove: uncacheable, no mappings of other color"));

	/*
	 * If the page is not cacheable (color is -1), and the number of
	 * mappings for this color is not zero, just return.  There are
	 * mappings of the other color still, so remain non-cacheable.
	 */
	if (m->md.colors[color] != 0) {
		PMAP_STATS_INC(pmap_ncache_remove_nc);
		return;
	}

	PMAP_STATS_INC(pmap_ncache_remove_cc);

	/*
	 * The number of mappings for this color is now zero.  Recache the
	 * other colored mappings, and change the page color to the other
	 * color.  There should be no lines in the data cache for this page,
	 * so flushing should not be needed.
	 */
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		tp->tte_data |= TD_CV;
		tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
	}
	m->md.color = DCACHE_OTHER_COLOR(color);
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	vm_offset_t ova;
	struct tte *tp;
	vm_page_t om;
	vm_page_t m;
	u_long data;

	tp = tsb_kvtotte(va);
	m = PHYS_TO_VM_PAGE(pa);
	CTR4(KTR_PMAP, "pmap_kenter: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, pa, tp, tp->tte_data);
	if ((tp->tte_data & TD_V) != 0) {
		om = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		ova = TTE_GET_VA(tp);
		STAILQ_REMOVE(&om->md.tte_list, tp, tte, tte_link);
		pmap_cache_remove(om, ova);
		if (va != ova)
			tlb_page_demap(kernel_pmap, ova);
	}
	data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_SW | TD_CP | TD_P | TD_W;
	if (pmap_cache_enter(m, va) != 0)
		data |= TD_CV;
	tp->tte_vpn = TV_VPN(va);
	tp->tte_data = data;
	STAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
	tp->tte_pmap = kernel_pmap;
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
	struct tte *tp;

	tp = tsb_kvtotte(va);
	CTR4(KTR_PMAP, "pmap_kenter_flags: va=%#lx pa=%#lx tp=%p data=%#lx",
	    va, pa, tp, tp->tte_data);
	tp->tte_vpn = TV_VPN(va);
	tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_P | flags;
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
	vm_page_t m;

	tp = tsb_kvtotte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx tp=%p data=%#lx", va, tp,
	    tp->tte_data);
	m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
	STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
	pmap_cache_remove(m, va);
	TTE_ZERO(tp);
}

/*
 * Inverse of pmap_kenter_flags, used by bus_space_unmap().
 */
void
pmap_kremove_flags(vm_offset_t va)
{
	struct tte *tp;

	tp = tsb_kvtotte(va);
	CTR3(KTR_PMAP, "pmap_kremove: va=%#lx tp=%p data=%#lx", va, tp,
	    tp->tte_data);
	TTE_ZERO(tp);
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
	struct tte *tp;
	vm_offset_t sva;
	vm_offset_t va;
	vm_offset_t pa;

	pa = pa_start;
	sva = *virt;
	va = sva;
	for (; pa < pa_end; pa += PAGE_SIZE, va += PAGE_SIZE) {
		tp = tsb_kvtotte(va);
		tp->tte_vpn = TV_VPN(va);
		tp->tte_data = TD_V | TD_8K | TD_PA(pa) | TD_REF | TD_SW |
		    TD_CP | TD_CV | TD_P | TD_W;
	}
	tlb_range_demap(kernel_pmap, sva, sva + (pa_end - pa_start) - 1);
	*virt = va;
	return (sva);
}

static vm_offset_t
pmap_map_direct(vm_page_t m)
{
	vm_offset_t pa;
	vm_offset_t va;

	pa = VM_PAGE_TO_PHYS(m);
	if (m->md.color == -1) {
		KASSERT(m->md.colors[0] != 0 && m->md.colors[1] != 0,
		    ("pmap_map_direct: non-cacheable, only 1 color"));
		va = TLB_DIRECT_MASK | pa | TLB_DIRECT_UNCACHEABLE;
	} else {
		KASSERT(m->md.colors[DCACHE_OTHER_COLOR(m->md.color)] == 0,
		    ("pmap_map_direct: cacheable, mappings of other color"));
		va = TLB_DIRECT_MASK | pa |
		    (m->md.color << TLB_DIRECT_COLOR_SHIFT);
	}
	return (va << TLB_DIRECT_SHIFT);
}

/*
 * Map a list of wired pages into kernel virtual address space.  This is
 * intended for temporary mappings which do not need page modification or
 * references recorded.  Existing mappings in the region are overwritten.
 */
void
pmap_qenter(vm_offset_t sva, vm_page_t *m, int count)
{
	vm_offset_t va;
	int i;

	va = sva;
	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
	tlb_range_demap(kernel_pmap, sva, sva + (count * PAGE_SIZE) - 1);
}

/*
 * As above, but take an additional flags argument and call
 * pmap_kenter_flags().
 */
void
pmap_qenter_flags(vm_offset_t sva, vm_page_t *m, int count, u_long fl)
{
	vm_offset_t va;
	int i;

	va = sva;
	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kenter_flags(va, VM_PAGE_TO_PHYS(m[i]), fl);
	tlb_range_demap(kernel_pmap, sva, sva + (count * PAGE_SIZE) - 1);
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by pmap_qenter.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;
	int i;

	va = sva;
	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kremove(va);
	tlb_range_demap(kernel_pmap, sva, sva + (count * PAGE_SIZE) - 1);
}

/*
 * Create the kernel stack and pcb for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
void
pmap_new_thread(struct thread *td)
{
	vm_page_t ma[KSTACK_PAGES];
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	u_int i;

	/*
	 * Allocate object for the kstack,
	 */
	ksobj = vm_object_allocate(OBJT_DEFAULT, KSTACK_PAGES);
	td->td_kstack_obj = ksobj;

	/*
	 * Get a kernel virtual address for the kstack for this thread.
	 */
	ks = kmem_alloc_nofault(kernel_map,
	   (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE);
	if (ks == 0)
		panic("pmap_new_thread: kstack allocation failed");
	if (KSTACK_GUARD_PAGES != 0) {
		tlb_page_demap(kernel_pmap, ks);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
	}
	td->td_kstack = ks;

	for (i = 0; i < KSTACK_PAGES; i++) {
		/*
		 * Get a kernel stack page.
		 */
		m = vm_page_grab(ksobj, i,
		    VM_ALLOC_NORMAL | VM_ALLOC_RETRY | VM_ALLOC_WIRED);
		ma[i] = m;

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		m->valid = VM_PAGE_BITS_ALL;
	}

	/*
	 * Enter the page into the kernel address space.
	 */
	pmap_qenter(ks, ma, KSTACK_PAGES);
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
			panic("pmap_dispose_thread: kstack already missing?");
		vm_page_lock_queues();
		vm_page_busy(m);
		vm_page_unwire(m, 0);
		vm_page_free(m);
		vm_page_unlock_queues();
	}
	pmap_qremove(ks, KSTACK_PAGES);
	kmem_free(kernel_map, ks - (KSTACK_GUARD_PAGES * PAGE_SIZE),
	    (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE);
	vm_object_deallocate(ksobj);
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
		vm_page_lock_queues();
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		vm_page_unlock_queues();
	}
	pmap_qremove(ks, KSTACK_PAGES);
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
void
pmap_swapin_thread(struct thread *td)
{
	vm_page_t ma[KSTACK_PAGES];
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;
	int rv;
	int i;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(ksobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_thread: cannot get kstack");
			m = vm_page_lookup(ksobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		ma[i] = m;
		vm_page_lock_queues();
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_unlock_queues();
	}
	pmap_qenter(ks, ma, KSTACK_PAGES);
}

/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pm)
{
	int i;

	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = 0;
	pm->pm_active = 0;
	pm->pm_tsb = NULL;
	pm->pm_tsb_obj = NULL;
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

/*
 * Initialize a preallocated and zeroed pmap structure, uch as one in a
 * vmspace structure.
 */
void
pmap_pinit(pmap_t pm)
{
	vm_page_t ma[TSB_PAGES];
	vm_page_t m;
	int i;

	/*
	 * Allocate kva space for the tsb.
	 */
	if (pm->pm_tsb == NULL) {
		pm->pm_tsb = (struct tte *)kmem_alloc_pageable(kernel_map,
		    TSB_BSIZE);
	}

	/*
	 * Allocate an object for it.
	 */
	if (pm->pm_tsb_obj == NULL)
		pm->pm_tsb_obj = vm_object_allocate(OBJT_DEFAULT, TSB_PAGES);

	for (i = 0; i < TSB_PAGES; i++) {
		m = vm_page_grab(pm->pm_tsb_obj, i,
		    VM_ALLOC_RETRY | VM_ALLOC_ZERO);
		if ((m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);

		m->wire_count++;
		cnt.v_wire_count++;

		vm_page_flag_clear(m, PG_BUSY);
		m->valid = VM_PAGE_BITS_ALL;

		ma[i] = m;
	}
	pmap_qenter((vm_offset_t)pm->pm_tsb, ma, TSB_PAGES);

	for (i = 0; i < MAXCPU; i++)
		pm->pm_context[i] = -1;
	pm->pm_active = 0;
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
	vm_object_t obj;
	vm_page_t m;

	CTR2(KTR_PMAP, "pmap_release: ctx=%#x tsb=%p",
	    pm->pm_context[PCPU_GET(cpuid)], pm->pm_tsb);
	obj = pm->pm_tsb_obj;
	KASSERT(obj->ref_count == 1, ("pmap_release: tsbobj ref count != 1"));
	KASSERT(pmap_resident_count(pm) == 0,
	    ("pmap_release: resident pages %ld != 0",
	    pmap_resident_count(pm)));
	while (!TAILQ_EMPTY(&obj->memq)) {
		m = TAILQ_FIRST(&obj->memq);
		if (vm_page_sleep_busy(m, FALSE, "pmaprl"))
			continue;
		vm_page_busy(m);
		KASSERT(m->hold_count == 0,
		    ("pmap_release: freeing held tsb page"));
		m->wire_count--;
		cnt.v_wire_count--;
		vm_page_free_zero(m);
	}
	pmap_qremove((vm_offset_t)pm->pm_tsb, TSB_PAGES);
}

/*
 * Grow the number of kernel page table entries.  Unneeded.
 */
void
pmap_growkernel(vm_offset_t addr)
{

	panic("pmap_growkernel: can't grow kernel");
}

/*
 * This routine is very drastic, but can save the system
 * in a pinch.
 */
void
pmap_collect(void)
{
}

int
pmap_remove_tte(struct pmap *pm, struct pmap *pm2, struct tte *tp,
		vm_offset_t va)
{
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
	STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
	if ((tp->tte_data & TD_WIRED) != 0)
		pm->pm_stats.wired_count--;
	if ((tp->tte_data & TD_PV) != 0) {
		if ((tp->tte_data & TD_W) != 0 &&
		    pmap_track_modified(pm, va))
			vm_page_dirty(m);
		if ((tp->tte_data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if (STAILQ_EMPTY(&m->md.tte_list))
			vm_page_flag_clear(m, PG_WRITEABLE);
		pm->pm_stats.resident_count--;
	}
	pmap_cache_remove(m, va);
	TTE_ZERO(tp);
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
	vm_offset_t va;

	CTR3(KTR_PMAP, "pmap_remove: ctx=%#lx start=%#lx end=%#lx",
	    pm->pm_context[PCPU_GET(cpuid)], start, end);
	if (PMAP_REMOVE_DONE(pm))
		return;
	if (end - start > PMAP_TSB_THRESH) {
		tsb_foreach(pm, NULL, start, end, pmap_remove_tte);
		tlb_context_demap(pm);
	} else {
		for (va = start; va < end; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, va)) != NULL) {
				if (!pmap_remove_tte(pm, NULL, tp, va))
					break;
			}
		}
		tlb_range_demap(pm, start, end - 1);
	}
}

void
pmap_remove_all(vm_page_t m)
{
	struct pmap *pm;
	struct tte *tpn;
	struct tte *tp;
	vm_offset_t va;

	KASSERT((m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0,
	   ("pv_remove_all: illegal for unmanaged page %#lx",
	   VM_PAGE_TO_PHYS(m)));
	for (tp = STAILQ_FIRST(&m->md.tte_list); tp != NULL; tp = tpn) {
		tpn = STAILQ_NEXT(tp, tte_link);
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		pm = TTE_GET_PMAP(tp);
		va = TTE_GET_VA(tp);
		if ((tp->tte_data & TD_WIRED) != 0)
			pm->pm_stats.wired_count--;
		if ((tp->tte_data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if ((tp->tte_data & TD_W) != 0 &&
		    pmap_track_modified(pm, va))
			vm_page_dirty(m);
		tp->tte_data &= ~TD_V;
		tlb_page_demap(pm, va);
		STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
		pm->pm_stats.resident_count--;
		pmap_cache_remove(m, va);
		TTE_ZERO(tp);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
}

int
pmap_protect_tte(struct pmap *pm, struct pmap *pm2, struct tte *tp,
		 vm_offset_t va)
{
	vm_page_t m;

	if ((tp->tte_data & TD_PV) != 0) {
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		if ((tp->tte_data & TD_REF) != 0) {
			vm_page_flag_set(m, PG_REFERENCED);
			tp->tte_data &= ~TD_REF;
		}
		if ((tp->tte_data & TD_W) != 0 &&
		    pmap_track_modified(pm, va)) {
			vm_page_dirty(m);
		}
	}
	tp->tte_data &= ~(TD_W | TD_SW);
	return (0);
}

/*
 * Set the physical protection on the specified range of this map as requested.
 */
void
pmap_protect(pmap_t pm, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t va;
	struct tte *tp;

	CTR4(KTR_PMAP, "pmap_protect: ctx=%#lx sva=%#lx eva=%#lx prot=%#lx",
	    pm->pm_context[PCPU_GET(cpuid)], sva, eva, prot);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pm, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	if (eva - sva > PMAP_TSB_THRESH) {
		tsb_foreach(pm, NULL, sva, eva, pmap_protect_tte);
		tlb_context_demap(pm);
	} else {
		for (va = sva; va < eva; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(pm, va)) != NULL)
				pmap_protect_tte(pm, NULL, tp, va);
		}
		tlb_range_demap(pm, sva, eva - 1);
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
	struct tte *tp;
	vm_offset_t pa;
	u_long data;

	pa = VM_PAGE_TO_PHYS(m);
	CTR6(KTR_PMAP,
	    "pmap_enter: ctx=%p m=%p va=%#lx pa=%#lx prot=%#x wired=%d",
	    pm->pm_context[PCPU_GET(cpuid)], m, va, pa, prot, wired);

	/*
	 * If there is an existing mapping, and the physical address has not
	 * changed, must be protection or wiring change.
	 */
	if ((tp = tsb_tte_lookup(pm, va)) != NULL && TTE_GET_PA(tp) == pa) {
		CTR0(KTR_PMAP, "pmap_enter: update");
		PMAP_STATS_INC(pmap_enter_nupdate);

		/*
		 * Wiring change, just update stats.
		 */
		if (wired) {
			if ((tp->tte_data & TD_WIRED) == 0) {
				tp->tte_data |= TD_WIRED;
				pm->pm_stats.wired_count++;
			}
		} else {
			if ((tp->tte_data & TD_WIRED) != 0) {
				tp->tte_data &= ~TD_WIRED;
				pm->pm_stats.wired_count--;
			}
		}

		/*
		 * Save the old bits and clear the ones we're interested in.
		 */
		data = tp->tte_data;
		tp->tte_data &= ~(TD_EXEC | TD_SW | TD_W);

		/*
		 * If we're turning off write permissions, sense modify status.
		 */
		if ((prot & VM_PROT_WRITE) != 0) {
			tp->tte_data |= TD_SW;
			if (wired) {
				tp->tte_data |= TD_W;
			}
		} else if ((data & TD_W) != 0 &&
		    pmap_track_modified(pm, va)) {
			vm_page_dirty(m);
		}

		/*
		 * If we're turning on execute permissions, flush the icache.
		 */
		if ((prot & VM_PROT_EXECUTE) != 0) {
			if ((data & TD_EXEC) == 0) {
				PMAP_STATS_INC(pmap_niflush);
				icache_page_inval(pa);
			}
			tp->tte_data |= TD_EXEC;
		}

		/*
		 * Delete the old mapping.
		 */
		tlb_page_demap(pm, TTE_GET_VA(tp));
	} else {
		/*
		 * If there is an existing mapping, but its for a different
		 * phsyical address, delete the old mapping.
		 */
		if (tp != NULL) {
			CTR0(KTR_PMAP, "pmap_enter: replace");
			PMAP_STATS_INC(pmap_enter_nreplace);
			pmap_remove_tte(pm, NULL, tp, va);
			tlb_page_demap(pm, va);
		} else {
			CTR0(KTR_PMAP, "pmap_enter: new");
			PMAP_STATS_INC(pmap_enter_nnew);
		}

		/*
		 * Now set up the data and install the new mapping.
		 */
		data = TD_V | TD_8K | TD_PA(pa) | TD_CP;
		if (pm == kernel_pmap)
			data |= TD_P;
		if (prot & VM_PROT_WRITE)
			data |= TD_SW;
		if (prot & VM_PROT_EXECUTE) {
			data |= TD_EXEC;
			PMAP_STATS_INC(pmap_niflush);
			icache_page_inval(pa);
		}

		/*
		 * If its wired update stats.  We also don't need reference or
		 * modify tracking for wired mappings, so set the bits now.
		 */
		if (wired) {
			pm->pm_stats.wired_count++;
			data |= TD_REF | TD_WIRED;
			if ((prot & VM_PROT_WRITE) != 0)
				data |= TD_W;
		}

		tsb_tte_enter(pm, m, va, data);
	}
}

void
pmap_object_init_pt(pmap_t pm, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size, int limit)
{
	/* XXX */
}

void
pmap_prefault(pmap_t pm, vm_offset_t va, vm_map_entry_t entry)
{
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

static int
pmap_copy_tte(pmap_t src_pmap, pmap_t dst_pmap, struct tte *tp, vm_offset_t va)
{
	vm_page_t m;
	u_long data;

	if (tsb_tte_lookup(dst_pmap, va) == NULL) {
		data = tp->tte_data &
		    ~(TD_PV | TD_REF | TD_SW | TD_CV | TD_W);
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tp));
		tsb_tte_enter(dst_pmap, m, va, data);
	}
	return (1);
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	struct tte *tp;
	vm_offset_t va;

	if (dst_addr != src_addr)
		return;
	if (len > PMAP_TSB_THRESH) {
		tsb_foreach(src_pmap, dst_pmap, src_addr, src_addr + len,
		    pmap_copy_tte);
		tlb_context_demap(dst_pmap);
	} else {
		for (va = src_addr; va < src_addr + len; va += PAGE_SIZE) {
			if ((tp = tsb_tte_lookup(src_pmap, va)) != NULL)
				pmap_copy_tte(src_pmap, dst_pmap, tp, va);
		}
		tlb_range_demap(dst_pmap, src_addr, src_addr + len - 1);
	}
}

/*
 * Zero a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va;

	va = pmap_map_direct(m);
	CTR2(KTR_PMAP, "pmap_zero_page: pa=%#lx va=%#lx",
	    VM_PAGE_TO_PHYS(m), va);
	bzero((void *)va, PAGE_SIZE);
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va;

	KASSERT(off + size <= PAGE_SIZE, ("pmap_zero_page_area: bad off/size"));
	va = pmap_map_direct(m);
	CTR4(KTR_PMAP, "pmap_zero_page_area: pa=%#lx va=%#lx off=%#x size=%#x",
	    VM_PAGE_TO_PHYS(m), va, off, size);
	bzero((void *)(va + off), size);
}

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va;

	va = pmap_map_direct(m);
	CTR2(KTR_PMAP, "pmap_zero_page_idle: pa=%#lx va=%#lx",
	    VM_PAGE_TO_PHYS(m), va);
	bzero((void *)va, PAGE_SIZE);
}

/*
 * Copy a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t dst;
	vm_offset_t src;

	src = pmap_map_direct(msrc);
	dst = pmap_map_direct(mdst);
	CTR4(KTR_PMAP, "pmap_zero_page: src=%#lx va=%#lx dst=%#lx va=%#lx",
	    VM_PAGE_TO_PHYS(msrc), src, VM_PAGE_TO_PHYS(mdst), dst);
	bcopy((void *)src, (void *)dst, PAGE_SIZE);
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
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pm, vm_page_t m)
{
	struct tte *tp;
	int loops;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (FALSE);
	loops = 0;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if (TTE_GET_PMAP(tp) == pm)
			return (TRUE);
		if (++loops >= 16)
			break;
	}
	return (FALSE);
}

/*
 * Remove all pages from specified address space, this aids process exit
 * speeds.  This is much faster than pmap_remove n the case of running down
 * an entire address space.  Only works for the current pmap.
 */
void
pmap_remove_pages(pmap_t pm, vm_offset_t sva, vm_offset_t eva)
{
}

/*
 * Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{

	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ | VM_PROT_EXECUTE))
			pmap_clear_write(m);
		else
			pmap_remove_all(m);
	}
}

vm_offset_t
pmap_phys_address(int ppn)
{

	return (sparc64_ptob(ppn));
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
	struct tte *tpf;
	struct tte *tpn;
	struct tte *tp;
	int count;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return (0);
	count = 0;
	if ((tp = STAILQ_FIRST(&m->md.tte_list)) != NULL) {
		tpf = tp;
		do {
			tpn = STAILQ_NEXT(tp, tte_link);
			STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
			STAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
			if ((tp->tte_data & TD_PV) == 0 ||
			    !pmap_track_modified(TTE_GET_PMAP(tp),
			     TTE_GET_VA(tp)))
				continue;
			if ((tp->tte_data & TD_REF) != 0) {
				tp->tte_data &= ~TD_REF;
				if (++count > 4)
					break;
			}
		} while ((tp = tpn) != NULL && tp != tpf);
	}
	return (count);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	struct tte *tp;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return FALSE;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0 ||
		    !pmap_track_modified(TTE_GET_PMAP(tp), TTE_GET_VA(tp)))
			continue;
		if ((tp->tte_data & TD_W) != 0)
			return (TRUE);
	}
	return (FALSE);
}

void
pmap_clear_modify(vm_page_t m)
{
	struct tte *tp;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if ((tp->tte_data & TD_W) != 0) {
			tp->tte_data &= ~TD_W;
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
		}
	}
}

void
pmap_clear_reference(vm_page_t m)
{
	struct tte *tp;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if ((tp->tte_data & TD_REF) != 0) {
			tp->tte_data &= ~TD_REF;
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
		}
	}
}

void
pmap_clear_write(vm_page_t m)
{
	struct tte *tp;

	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) != 0)
		return;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & TD_PV) == 0)
			continue;
		if ((tp->tte_data & (TD_SW | TD_W)) != 0) {
			if ((tp->tte_data & TD_W) != 0 &&
			    pmap_track_modified(TTE_GET_PMAP(tp),
			    TTE_GET_VA(tp)))
				vm_page_dirty(m);
			tp->tte_data &= ~(TD_SW | TD_W);
			tlb_page_demap(TTE_GET_PMAP(tp), TTE_GET_VA(tp));
		}
	}
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
	struct vmspace *vm;
	vm_offset_t tsb;
	u_long context;
	pmap_t pm;

	/*
	 * Load all the data we need up front to encourage the compiler to
	 * not issue any loads while we have interrupts disable below.
	 */
	vm = td->td_proc->p_vmspace;
	pm = &vm->vm_pmap;
	tsb = (vm_offset_t)pm->pm_tsb;

	KASSERT(pm->pm_active == 0, ("pmap_activate: pmap already active?"));
	KASSERT(pm->pm_context[PCPU_GET(cpuid)] != 0,
	    ("pmap_activate: activating nucleus context?"));

	mtx_lock_spin(&sched_lock);
	wrpr(pstate, 0, PSTATE_MMU);
	mov(tsb, TSB_REG);
	wrpr(pstate, 0, PSTATE_KERNEL);
	context = pmap_context_alloc();
	pm->pm_context[PCPU_GET(cpuid)] = context;
	pm->pm_active |= PCPU_GET(cpumask);
	PCPU_SET(vmspace, vm);
	stxa(AA_DMMU_PCXR, ASI_DMMU, context);
	membar(Sync);
	mtx_unlock_spin(&sched_lock);
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{

	return (va);
}
