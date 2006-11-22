/*-
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>
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

#include "opt_kstack_pages.h"
#include "opt_msgbuf.h"
#include "opt_pmap.h"
#include "opt_trap_trace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h> 
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

#include <machine/cpu.h>
#include <machine/cache.h>
#include <machine/frame.h>
#include <machine/instr.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_mem.h>
#include <machine/mmu.h>
#include <machine/smp.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/tte_hash.h>
#include <machine/pcb.h>
#include <machine/pstate.h>
#include <machine/tsb.h>

#include <machine/hypervisor_api.h>

#ifdef TRAP_TRACING
void trap_trace_report(int);
#endif

#if 1
#define	PMAP_DEBUG
#endif
#ifndef	PMAP_SHPGPERPROC
#define	PMAP_SHPGPERPROC	200
#endif

cache_enable_t *cache_enable;
cache_flush_t *cache_flush;
dcache_page_inval_t *dcache_page_inval;
icache_page_inval_t *icache_page_inval;

/*
 * Virtual and physical address of message buffer.
 */
struct msgbuf *msgbufp;
vm_paddr_t msgbuf_phys;

/*
 * Map of physical memory reagions.
 */
vm_paddr_t phys_avail[128];
vm_paddr_t phys_avail_tmp[128];
static struct ofw_mem_region mra[128];
static struct ofw_map translations[128];
static int translations_size;


struct ofw_mem_region sparc64_memreg[128];
int sparc64_nmemreg;

extern vm_paddr_t mmu_fault_status_area;

/*
 * First and last available kernel virtual addresses.
 */
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;
vm_offset_t vm_max_kernel_address;

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif
/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
static struct vm_object pvzone_obj;
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
int pmap_debug = 0;

static struct mtx pmap_ctx_lock;
static uint16_t ctx_stack[PMAP_CONTEXT_MAX];
static int ctx_stack_top; 

static int permanent_mappings = 0;
static uint64_t nucleus_memory;
static uint64_t nucleus_mappings[4];
/*
 * Kernel pmap.
 */
struct pmap kernel_pmap_store;

hv_tsb_info_t kernel_td[MAX_TSB_INFO];

/*
 * This should be determined at boot time
 * with tiny TLBS it doesn't make sense to try and selectively
 * invalidate more than this 
 */
#define MAX_INVALIDATES   32
#define MAX_TSB_CLEARS   128

/*
 * Allocate physical memory for use in pmap_bootstrap.
 */
static vm_paddr_t pmap_bootstrap_alloc(vm_size_t size);

/*
 * If user pmap is processed with pmap_remove and with pmap_remove and the
 * resident count drops to 0, there are no more pages to remove, so we
 * need not continue.
 */
#define	PMAP_REMOVE_DONE(pm) \
	((pm) != kernel_pmap && (pm)->pm_stats.resident_count == 0)

/*
 * Kernel MMU interface
 */

#ifdef PMAP_DEBUG
#define KDPRINTF if (pmap_debug) printf
#define DPRINTF \
	if (PCPU_GET(curpmap) && (PCPU_GET(curpmap)->pm_context != 0) && ((PCPU_GET(cpumask) & PCPU_GET(curpmap)->pm_active)) == 0) \
   	panic("cpumask(0x%x) & active (0x%x) == 0 pid == %d\n",  \
	      PCPU_GET(cpumask), PCPU_GET(curpmap)->pm_active, curthread->td_proc->p_pid); \
if (pmap_debug) printf


#else
#define DPRINTF(...)
#define KDPRINTF(...)
#endif


static void free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t locked_pmap);

static void pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t m);
static void pmap_remove_entry(struct pmap *pmap, vm_page_t m, vm_offset_t va);
static void pmap_remove_tte(pmap_t pmap, tte_t tte_data, vm_offset_t va);
static void pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot);
static void pmap_tte_hash_resize(pmap_t pmap);

void pmap_set_ctx_panic(uint64_t error, vm_paddr_t tsb_ra, pmap_t pmap);


/*
 * Quick sort callout for comparing memory regions.
 */
static int mr_cmp(const void *a, const void *b);
static int om_cmp(const void *a, const void *b);
static int
mr_cmp(const void *a, const void *b)
{
	const struct ofw_mem_region *mra;
	const struct ofw_mem_region *mrb;

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

static __inline void
free_context(uint16_t ctx)
{
	mtx_lock_spin(&pmap_ctx_lock);
	ctx_stack[ctx_stack_top++] = ctx;
	mtx_unlock_spin(&pmap_ctx_lock);

	KASSERT(ctx_stack_top < PMAP_CONTEXT_MAX, 
		("context stack overrun - system error"));
}

static __inline uint16_t
get_context(void)
{
	uint16_t ctx;

	mtx_lock_spin(&pmap_ctx_lock);
	ctx = ctx_stack[--ctx_stack_top];
	mtx_unlock_spin(&pmap_ctx_lock);

	KASSERT(ctx_stack_top > 0,
		("context stack underrun - need to implement context stealing"));

	return ctx;
}

static __inline void
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
	uint64_t tte_data;
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
	sched_pin();
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

			tte_data = tte_hash_delete(pmap->pm_hash, va);

			KASSERT((tte_data & VTD_WIRED) == 0,
			    ("get_pv_entry: wired pte %#jx", (uintmax_t)tte_data));
			if (tte_data & VTD_REF)
				vm_page_flag_set(m, PG_REFERENCED);
			if (tte_data & VTD_W) {
				KASSERT((tte_data & VTD_SW_W),
				("get_pv_entry: modified page not writable: va: %lx, tte: %lx",
				    va, tte_data));
				vm_page_dirty(m);
			}

			pmap_invalidate_page(pmap, va, TRUE);
			TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
			if (TAILQ_EMPTY(&m->md.pv_list))
				vm_page_flag_clear(m, PG_WRITEABLE);
			m->md.pv_list_count--;

			if (pmap != locked_pmap)
				PMAP_UNLOCK(pmap);
			if (allocated_pv == NULL)
				allocated_pv = pv;
			else
				free_pv_entry(pv);
		}
	}
	sched_unpin();
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
 * Allocate a physical page of memory directly from the phys_avail map.
 * Can only be called from pmap_bootstrap before avail start and end are
 * calculated.
 */
static vm_paddr_t
pmap_bootstrap_alloc(vm_size_t size)
{
	vm_paddr_t pa;
	int i;

	size = round_page(size);

	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] - phys_avail[i] < size)
			continue;
		pa = phys_avail[i];
		phys_avail[i] += size;
		pmap_scrub_pages(pa, size);
		return (pa);
	}
	panic("pmap_bootstrap_alloc");
}

/*
 * Activate a user pmap.  The pmap must be activated before its address space
 * can be accessed in any way.
 */
void
pmap_activate(struct thread *td)
{
	pmap_t pmap, oldpmap;
	int err;
	
	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	oldpmap = PCPU_GET(curpmap);
#if defined(SMP)
	atomic_clear_int(&oldpmap->pm_active, PCPU_GET(cpumask));
	atomic_set_int(&pmap->pm_tlbactive, PCPU_GET(cpumask));
	atomic_set_int(&pmap->pm_active, PCPU_GET(cpumask));
#else
	oldpmap->pm_active &= ~1;
	pmap->pm_active |= 1;
	pmap->pm_tlbactive |= 1;
#endif

	pmap->pm_hashscratch = tte_hash_set_scratchpad_user(pmap->pm_hash, pmap->pm_context);
	pmap->pm_tsbscratch = tsb_set_scratchpad_user(&pmap->pm_tsb);
	PCPU_SET(curpmap, pmap);
	if (pmap->pm_context != 0)
		if ((err = hv_set_ctxnon0(1, pmap->pm_tsb_ra)) != H_EOK)
			panic("failed to set TSB 0x%lx - context == %ld\n", 
			      pmap->pm_tsb_ra, pmap->pm_context);
	stxa(MMU_CID_S, ASI_MMU_CONTEXTID, pmap->pm_context);
	membar(Sync);
	critical_exit();
}

vm_offset_t 
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{
	return (va);
}

/*
 * Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap(vm_offset_t ekva)
{
	struct pmap *pm;
	vm_offset_t off, va;
	vm_paddr_t pa, kernel_hash_pa, phys_avail_start, nucleus_memory_start;
	vm_size_t physsz, virtsz, kernel_hash_shift;
	ihandle_t pmem, vmem;
	int i, sz, j;
	uint64_t tsb_8k_size, tsb_4m_size, error, physmem_tunable;

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
	nucleus_memory_start = 0;
	CTR0(KTR_PMAP, "pmap_bootstrap: translations");
	qsort(translations, sz, sizeof (*translations), om_cmp);

	for (i = 0; i < sz; i++) {
		KDPRINTF("om_size=%ld om_start=%lx om_tte=%lx\n", 
			translations[i].om_size, translations[i].om_start, 
			translations[i].om_tte);
		if (translations[i].om_size == PAGE_SIZE_4M && 
		    (translations[i].om_start >= KERNBASE && 
		     translations[i].om_start <= KERNBASE + 3*PAGE_SIZE_4M)) {
			KDPRINTF("mapping permanent translation\n");
			pa = TTE_GET_PA(translations[i].om_tte);
			error = hv_mmu_map_perm_addr((char *)translations[i].om_start, 
				KCONTEXT, pa | TTE_KERNEL | VTD_4M, MAP_ITLB | MAP_DTLB);
			if (error != H_EOK)
				panic("map_perm_addr returned error=%ld", error);
			
			if ((nucleus_memory_start == 0) || (pa < nucleus_memory_start))
				nucleus_memory_start = pa;
			nucleus_mappings[permanent_mappings++] = pa;
			nucleus_memory += PAGE_SIZE_4M;
#ifdef SMP
			mp_add_nucleus_mapping(translations[i].om_start, 
					       pa | TTE_KERNEL | VTD_4M);
#endif
		}  
	}

	/*
	 * Find out what physical memory is available from the prom and
	 * initialize the phys_avail array.  This must be done before
	 * pmap_bootstrap_alloc is called.
	 */
	if ((pmem = OF_finddevice("/memory")) == -1)
		panic("pmap_bootstrap: finddevice /memory");
	if ((sz = OF_getproplen(pmem, "available")) == -1)
		panic("pmap_bootstrap: getproplen /memory/available");
	if (sizeof(vm_paddr_t)*128 < sz) /* FIXME */
		panic("pmap_bootstrap: phys_avail too small");
	if (sizeof(mra) < sz)
		panic("pmap_bootstrap: mra too small");
	bzero(mra, sz);
	if (OF_getprop(pmem, "available", mra, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/available");

	sz /= sizeof(*mra);
	CTR0(KTR_PMAP, "pmap_bootstrap: physical memory");

	qsort(mra, sz, sizeof (*mra), mr_cmp);
	physmem_tunable = physmem = physsz = 0;
	
        if (TUNABLE_ULONG_FETCH("hw.physmem", &physmem_tunable)) {
                physmem = atop(physmem_tunable);
		KDPRINTF("desired physmem=0x%lx\n", physmem_tunable);
	} 
	for (i = 0, j = 0; i < sz; i++) {
		vm_paddr_t start = mra[i].mr_start;
		uint64_t size = mra[i].mr_size;
		CTR2(KTR_PMAP, "start=%#lx size=%#lx\n", mra[i].mr_start, mra[i].mr_size);
		KDPRINTF("start=%#lx size=%#lx\n", mra[i].mr_start, mra[i].mr_size);
		/* 
		 * Is kernel memory at the beginning of range?
		 */
		if (nucleus_memory_start == mra[i].mr_start) {
			mra[i].mr_start += nucleus_memory;
			mra[i].mr_size -= nucleus_memory;
		}
		/* 
		 * Is kernel memory at the end of range?
		 */
		if (nucleus_memory_start == (start + size - nucleus_memory)) 
			mra[i].mr_size -= nucleus_memory;

		/* 
		 * Is kernel memory in the middle somewhere?
		 */
		if ((nucleus_memory_start > start) && (nucleus_memory_start < (start + size))) {
			uint64_t firstsize = (nucleus_memory_start - start);
			phys_avail[j] = start;
			if ((physmem_tunable != 0) && ((physsz + firstsize) > physmem_tunable)) {
				phys_avail[j+1] = start + (physmem_tunable - physsz);
				physsz = physmem_tunable;
				break;
			}
			phys_avail[j+1] = nucleus_memory_start;
			size = size - firstsize - nucleus_memory;
			mra[i].mr_start =  nucleus_memory_start + nucleus_memory;
			mra[i].mr_size = size;
			physsz += firstsize + nucleus_memory;
			j += 2;
		}
		if (mra[i].mr_size < PAGE_SIZE_4M)
			continue;
		if ((mra[i].mr_start & PAGE_MASK_4M) && (mra[i].mr_size < nucleus_memory))
			continue;
		if (mra[i].mr_start & PAGE_MASK_4M) {
			uint64_t newstart, roundup;
			newstart = ((mra[i].mr_start + (PAGE_SIZE_4M-1)) & ~PAGE_MASK_4M);
			roundup = newstart - mra[i].mr_start;
			mra[i].mr_size -= roundup;
			mra[i].mr_start = newstart;
		}
		mra[i].mr_size &= ~PAGE_MASK_4M;
		phys_avail[j] = mra[i].mr_start;
		if (physmem_tunable != 0 && ((physsz + mra[i].mr_size) >= physmem_tunable)) {
			size = physmem_tunable - physsz;
			phys_avail[j + 1] = mra[i].mr_start + size;
			physsz = physmem_tunable;
			break;
		}
		phys_avail[j + 1] = mra[i].mr_start + mra[i].mr_size;
		physsz += mra[i].mr_size;
		j += 2;
	}
	phys_avail_start = phys_avail[0];
	physmem = btoc(physsz);
	

	for (i = 0; phys_avail[i] != 0; i += 2)
		KDPRINTF("phys_avail[%d]=0x%lx phys_avail[%d]=0x%lx\n",
			i, phys_avail[i], i+1, phys_avail[i+1]);
	/*
	 * Calculate the size of kernel virtual memory, and the size and mask
	 * for the kernel tsb.
	 */
	virtsz = roundup(physsz, PAGE_SIZE_4M << (PAGE_SHIFT - TTE_SHIFT));
	vm_max_kernel_address = VM_MIN_KERNEL_ADDRESS + virtsz;

	/*
	 * Set the start and end of kva.  The kernel is loaded at the first
	 * available 4 meg super page, so round up to the end of the page.
	 */
	virtual_avail = roundup2(ekva, PAGE_SIZE_4M);
	virtual_end = vm_max_kernel_address;
	kernel_vm_end = vm_max_kernel_address;

	/*
	 * Allocate and map a 4MB page for the kernel hashtable 
	 *
	 */
#ifndef SIMULATOR
	kernel_hash_shift = 10; /* PAGE_SIZE_4M*2 */
#else
	kernel_hash_shift = 6; /* PAGE_SIZE_8K*64 */
#endif

	kernel_hash_pa = pmap_bootstrap_alloc((1<<(kernel_hash_shift + PAGE_SHIFT)));
	if (kernel_hash_pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: hashtable pa unaligned\n");
	/*
	 * Set up TSB descriptors for the hypervisor
	 *
	 */
#ifdef notyet
	tsb_8k_size = virtsz >> (PAGE_SHIFT - TTE_SHIFT);
#else
	/* avoid alignment complaints from the hypervisor */
	tsb_8k_size = PAGE_SIZE_4M;
#endif

	pa = pmap_bootstrap_alloc(tsb_8k_size);
	if (pa & PAGE_MASK_4M)
		panic("pmap_bootstrap: tsb unaligned\n");
	KDPRINTF("tsb_8k_size is 0x%lx, tsb_8k_pa is 0x%lx\n", tsb_8k_size, pa);
	kernel_td[TSB8K_INDEX].hvtsb_idxpgsz = TTE8K;
	kernel_td[TSB8K_INDEX].hvtsb_assoc = 1;
	kernel_td[TSB8K_INDEX].hvtsb_ntte = (tsb_8k_size >> TTE_SHIFT);
	kernel_td[TSB8K_INDEX].hvtsb_ctx_index = 0;
	kernel_td[TSB8K_INDEX].hvtsb_pgszs = TSB8K;
	kernel_td[TSB8K_INDEX].hvtsb_rsvd = 0;
	kernel_td[TSB8K_INDEX].hvtsb_pa = pa;

	/*
	 * Initialize kernel's private TSB from 8K page TSB
	 *
	 */
	kernel_pmap->pm_tsb.hvtsb_idxpgsz = TTE8K;
	kernel_pmap->pm_tsb.hvtsb_assoc = 1;
	kernel_pmap->pm_tsb.hvtsb_ntte = (tsb_8k_size >> TTE_SHIFT);
	kernel_pmap->pm_tsb.hvtsb_ctx_index = 0;
	kernel_pmap->pm_tsb.hvtsb_pgszs = TSB8K;
	kernel_pmap->pm_tsb.hvtsb_rsvd = 0;
	kernel_pmap->pm_tsb.hvtsb_pa = pa;
	
	kernel_pmap->pm_tsb_ra = vtophys((vm_offset_t)&kernel_pmap->pm_tsb);
	tsb_set_scratchpad_kernel(&kernel_pmap->pm_tsb);
	
	/*
	 * Initialize kernel TSB for 4M pages
	 * currently (not by design) used for permanent mappings
	 */
	
	tsb_4m_size = (virtsz >> (PAGE_SHIFT_4M - TTE_SHIFT)) << 3;
	pa = pmap_bootstrap_alloc(tsb_4m_size);

	KDPRINTF("tsb_4m_pa is 0x%lx tsb_4m_size is 0x%lx\n", pa, tsb_4m_size);
	kernel_td[TSB4M_INDEX].hvtsb_idxpgsz = TTE4M;
	kernel_td[TSB4M_INDEX].hvtsb_assoc = 1;
	kernel_td[TSB4M_INDEX].hvtsb_ntte = (tsb_4m_size >> TTE_SHIFT);
	kernel_td[TSB4M_INDEX].hvtsb_ctx_index = 0;
	kernel_td[TSB4M_INDEX].hvtsb_pgszs = TSB4M; 
	kernel_td[TSB4M_INDEX].hvtsb_rsvd = 0;
	kernel_td[TSB4M_INDEX].hvtsb_pa = pa;

	/*
	 * allocate MMU fault status areas for all CPUS
	 */
	mmu_fault_status_area = pmap_bootstrap_alloc(MMFSA_SIZE*MAXCPU);

	/*
	 * Allocate and map the message buffer.
	 */
	msgbuf_phys = pmap_bootstrap_alloc(MSGBUF_SIZE);
	msgbufp = (struct msgbuf *)TLB_PHYS_TO_DIRECT(msgbuf_phys);

	/*
	 * Allocate a kernel stack with guard page for thread0 and map it into
	 * the kernel tsb.  
	 */
	pa = pmap_bootstrap_alloc(KSTACK_PAGES*PAGE_SIZE);
	kstack0_phys = pa;
	virtual_avail += KSTACK_GUARD_PAGES * PAGE_SIZE;
	kstack0 = virtual_avail;
	virtual_avail += KSTACK_PAGES * PAGE_SIZE;
	for (i = 0; i < KSTACK_PAGES; i++) {
		pa = kstack0_phys + i * PAGE_SIZE;
		va = kstack0 + i * PAGE_SIZE;
		tsb_set_tte_real(&kernel_td[TSB8K_INDEX], va,
			    pa | TTE_KERNEL | VTD_8K, 0);
	}
	/*
	 * Calculate the last available physical address.
	 */
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		KDPRINTF("phys_avail[%d]=0x%lx phys_avail[%d]=0x%lx\n",
			i, phys_avail[i], i+1, phys_avail[i+1]);
	KDPRINTF("phys_avail[%d]=0x%lx phys_avail[%d]=0x%lx\n",
			i, phys_avail[i], i+1, phys_avail[i+1]);

	Maxmem = sparc64_btop(phys_avail[i + 1]);
	
	/*
	 * Add the prom mappings to the kernel tsb.
	 */
	for (i = 0; i < sz; i++) {
		CTR3(KTR_PMAP,
		    "translation: start=%#lx size=%#lx tte=%#lx",
		    translations[i].om_start, translations[i].om_size,
		    translations[i].om_tte);
		KDPRINTF("om_size=%ld om_start=%lx om_tte=%lx\n", 
		       translations[i].om_size, translations[i].om_start, 
		       translations[i].om_tte);

		if (translations[i].om_start < VM_MIN_PROM_ADDRESS ||
		    translations[i].om_start > VM_MAX_PROM_ADDRESS) 
			continue;

		for (off = 0; off < translations[i].om_size;
		     off += PAGE_SIZE) {
			va = translations[i].om_start + off;
			pa = TTE_GET_PA(translations[i].om_tte) + off;
			tsb_assert_invalid(&kernel_td[TSB8K_INDEX], va);
			tsb_set_tte_real(&kernel_td[TSB8K_INDEX], va, pa | 
				    TTE_KERNEL | VTD_8K, 0);
		}
	}

	error = hv_set_ctx0(MAX_TSB_INFO, vtophys((vm_offset_t)&kernel_td));
	if (error != H_EOK)
		panic("failed to set ctx0 TSBs error: %ld", error);

#ifdef SMP
	mp_set_tsb_desc_ra(vtophys((vm_offset_t)&kernel_td));
#endif
	/*
	 * setup direct mappings
	 * 
	 */
	i = 0;
	pa = phys_avail_start;
	do {
		for (; pa < phys_avail[i + 1]; pa += PAGE_SIZE_4M) {
			tsb_assert_invalid(&kernel_td[TSB4M_INDEX], TLB_PHYS_TO_DIRECT(pa));
			tsb_set_tte_real(&kernel_td[TSB4M_INDEX], TLB_PHYS_TO_DIRECT(pa), 
					 pa | TTE_KERNEL | VTD_4M, 0);
				
		}
		i += 2;
		pa = phys_avail[i];
	} while (pa != 0);

	for (i = 0; i < permanent_mappings; i++) {
                pa = nucleus_mappings[i];
		tsb_assert_invalid(&kernel_td[TSB4M_INDEX], TLB_PHYS_TO_DIRECT(pa));
                tsb_set_tte_real(&kernel_td[TSB4M_INDEX], TLB_PHYS_TO_DIRECT(pa),
				 pa | TTE_KERNEL | VTD_4M, 0);
	}
	/*
	 * Get the available physical memory ranges from /memory/reg. These
	 * are only used for kernel dumps, but it may not be wise to do prom
	 * calls in that situation.
	 */
	if ((sz = OF_getproplen(pmem, "reg")) == -1)
		panic("pmap_bootstrap: getproplen /memory/reg");
	if (sizeof(sparc64_memreg) < sz)
		panic("pmap_bootstrap: sparc64_memreg too small");
	if (OF_getprop(pmem, "reg", sparc64_memreg, sz) == -1)
		panic("pmap_bootstrap: getprop /memory/reg");
	sparc64_nmemreg = sz / sizeof(*sparc64_memreg);

	pm = kernel_pmap;
	pm->pm_active = ~0;
	pm->pm_tlbactive = ~0;

	PMAP_LOCK_INIT(kernel_pmap);

	TAILQ_INIT(&kernel_pmap->pm_pvlist);

	/* 
	 * This could happen earlier - but I put it here to avoid 
	 * attempts to do updates until they're legal
	 */
	pm->pm_hash = tte_hash_kernel_create(TLB_PHYS_TO_DIRECT(kernel_hash_pa), kernel_hash_shift, 
					     pmap_bootstrap_alloc(PAGE_SIZE));
	pm->pm_hashscratch = tte_hash_set_scratchpad_kernel(pm->pm_hash);

	for (i = 0; i < translations_size; i++) {
		KDPRINTF("om_size=%ld om_start=%lx om_tte=%lx\n", 
		       translations[i].om_size, translations[i].om_start, 
		       translations[i].om_tte);

		if (translations[i].om_start < VM_MIN_PROM_ADDRESS ||
		    translations[i].om_start > VM_MAX_PROM_ADDRESS) {
			KDPRINTF("skipping\n");
			continue;
		}
		for (off = 0; off < translations[i].om_size; off += PAGE_SIZE) {
			va = translations[i].om_start + off;
			pa = TTE_GET_PA(translations[i].om_tte) + off;
			tte_hash_insert(pm->pm_hash, va, pa | TTE_KERNEL | VTD_8K);
		}
		KDPRINTF("set om_size=%ld om_start=%lx om_tte=%lx\n", 
		       translations[i].om_size, translations[i].om_start, 
		       translations[i].om_tte);
	}
	for (i = 0; i < KSTACK_PAGES; i++) {
		pa = kstack0_phys + i * PAGE_SIZE;
		va = kstack0 + i * PAGE_SIZE;
		tte_hash_insert(pm->pm_hash, va, pa | TTE_KERNEL | VTD_8K);
	}
	/*
	 * Add direct mappings to hash
	 *
	 */
#ifdef notyet
	/* hash only supports 8k pages */
	for (pa = PAGE_SIZE_4M; pa < phys_avail[2]; pa += PAGE_SIZE_4M)
		tte_hash_insert(pm->pm_hash, TLB_PHYS_TO_DIRECT(pa), 
				pa | TTE_KERNEL | VTD_4M);
#endif
        /* XXX relies on the fact that memory ranges only get smaller */
        for (i = 0; phys_avail[i + 2] != 0; i += 2)
                if (phys_avail[i + 1] - phys_avail[i] < PAGE_SIZE_4M)
                        phys_avail[i] = phys_avail[i+1] = 0;

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
	boolean_t iswired;
	PMAP_LOCK(pmap);
	iswired = tte_get_virt_bit(pmap, va, VTD_WIRED);

	if (wired && !iswired) {
		pmap->pm_stats.wired_count++;
		tte_set_virt_bit(pmap, va, VTD_WIRED);
	} else if (!wired && iswired) {
		pmap->pm_stats.wired_count--;
		tte_clear_virt_bit(pmap, va, VTD_WIRED);
	}
	PMAP_UNLOCK(pmap);
}

void
pmap_clear_modify(vm_page_t m)
{
	KDPRINTF("pmap_clear_modify(0x%lx)\n", VM_PAGE_TO_PHYS(m));
	tte_clear_phys_bit(m, VTD_W);
}

void
pmap_clear_reference(vm_page_t m)
{
	KDPRINTF("pmap_clear_reference(0x%lx)\n", VM_PAGE_TO_PHYS(m));
	tte_clear_phys_bit(m, VTD_REF);
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	vm_offset_t addr, end_addr;

	end_addr = src_addr + len;
	
	/*
	 * Don't let optional prefaulting of pages make us go
	 * way below the low water mark of free pages or way
	 * above high water mark of used pv entries.
	 */
	if (cnt.v_free_count < cnt.v_free_reserved ||
	    pv_entry_count > pv_entry_high_water)
		return;
	

	vm_page_lock_queues();
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}
	sched_pin();
	for (addr = src_addr; addr < end_addr; addr += PAGE_SIZE) {
		tte_t tte_data;
		vm_page_t m;

		tte_data = tte_hash_lookup(src_pmap->pm_hash, addr);

		if ((tte_data & VTD_MANAGED) != 0) {
			if (tte_hash_lookup(dst_pmap->pm_hash, addr) == 0) {
				m = PHYS_TO_VM_PAGE(TTE_GET_PA(tte_data));

				tte_hash_insert(dst_pmap->pm_hash, addr, tte_data & ~(VTD_W|VTD_REF));
				dst_pmap->pm_stats.resident_count++;
				pmap_insert_entry(dst_pmap, addr, m);
			} 
		}		
	}
	PMAP_UNLOCK(src_pmap);

	if (tte_hash_needs_resize(dst_pmap->pm_hash))
		pmap_tte_hash_resize(dst_pmap);
	sched_unpin();
	vm_page_unlock_queues();
	PMAP_UNLOCK(dst_pmap);

}

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	vm_paddr_t srcpa, dstpa;
	srcpa = VM_PAGE_TO_PHYS(src);
	dstpa = VM_PAGE_TO_PHYS(dst);

	novbcopy((char *)TLB_PHYS_TO_DIRECT(srcpa), (char *)TLB_PHYS_TO_DIRECT(dstpa), PAGE_SIZE);


}

static __inline void
pmap_add_tte(pmap_t pmap, vm_offset_t va, vm_page_t m, tte_t *tte_data, int wired)
{

	if (wired)
		pmap->pm_stats.wired_count++;
	
	if ((m->flags & (PG_FICTITIOUS | PG_UNMANAGED)) == 0) {
		pmap_insert_entry(pmap, va, m);
		*tte_data |= VTD_MANAGED;
	}
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 */
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	vm_paddr_t pa, opa;
	uint64_t tte_data, otte_data;
	vm_page_t om;
	int invlva;

	if (pmap->pm_context)
		DPRINTF("pmap_enter(va=%lx, pa=0x%lx, prot=%x)\n", va, 
			VM_PAGE_TO_PHYS(m), prot);

	om = NULL;
	
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	sched_pin();

	tte_data = pa = VM_PAGE_TO_PHYS(m);
	otte_data = tte_hash_delete(pmap->pm_hash, va);
	opa = TTE_GET_PA(otte_data);

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */

	if (opa == 0) {
		pmap->pm_stats.resident_count++;
		pmap_add_tte(pmap, va, m, &tte_data, wired);

	} else if (pa != opa) {
		/*
		 * Mapping has changed, invalidate old range and fall through to
		 * handle validating new mapping.
		 */
		if (otte_data & VTD_WIRED)
			pmap->pm_stats.wired_count--;

		if (otte_data & VTD_MANAGED) {
			om = PHYS_TO_VM_PAGE(opa);
			pmap_remove_entry(pmap, om, va);
		}

		pmap_add_tte(pmap, va, m, &tte_data, wired);

	} else /* (pa == opa) */ {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && ((otte_data & VTD_WIRED) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (otte_data & VTD_WIRED))
			pmap->pm_stats.wired_count--;

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (otte_data & VTD_MANAGED) {
			om = m;
			tte_data |= VTD_MANAGED;
		}
	} 

	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	if ((prot & VM_PROT_WRITE) != 0) {
		tte_data |= VTD_SW_W; 
		vm_page_flag_set(m, PG_WRITEABLE);
	}
	if ((prot & VM_PROT_EXECUTE) != 0)
		tte_data |= VTD_X;
	if (wired)
		tte_data |= VTD_WIRED;
	if (pmap == kernel_pmap)
		tte_data |= TTE_KERNEL_MINFLAGS;


	tte_hash_insert(pmap->pm_hash, va, tte_data|TTE_MINFLAGS|VTD_REF);
	tsb_set_tte(&pmap->pm_tsb, va, tte_data|TTE_MINFLAGS|VTD_REF, pmap->pm_context);
	
	invlva = FALSE;
	if ((otte_data & ~(VTD_W|VTD_REF)) != tte_data) {
		if (otte_data & VTD_MANAGED) {
			if (otte_data & VTD_REF) {
				vm_page_flag_set(om, PG_REFERENCED);
				if (opa != pa)
					invlva = TRUE;
			}
			if (otte_data & VTD_W) {
				vm_page_dirty(om);
#if 0
				if ((prot & VM_PROT_WRITE) == 0) /* XXX double check */
#endif
					invlva = TRUE;
			}
		}
	} 

	if (tte_hash_needs_resize(pmap->pm_hash))
		pmap_tte_hash_resize(pmap);
#ifdef notyet
	if ((PCPU_GET(curpmap) != kernel_pmap) && (curthread->td_proc->p_numthreads == 1)
	    && (pmap->pm_tsb_cap_miss_count > pmap->pm_tsb_miss_count >> 2)) {
		int size = tsb_size(&pmap->pm_tsb);
		pmap->pm_tsb_ra = tsb_init(&pmap->pm_tsb, &pmap->pm_tsbscratch, size << 1);
		pmap->pm_tsb_miss_count = 0;
		pmap->pm_tsb_cap_miss_count = 0;
	}
#endif
	vm_page_unlock_queues();


	if (invlva)
		pmap_invalidate_page(pmap, va, TRUE);
	sched_unpin();
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
	vm_page_t m, mpte;
        vm_pindex_t diff, psize;

        VM_OBJECT_LOCK_ASSERT(m_start->object, MA_OWNED);
        psize = atop(end - start);
        mpte = NULL;
        m = m_start;
        PMAP_LOCK(pmap);
        while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		pmap_enter_quick_locked(pmap, start + ptoa(diff), m,
					       prot);
                m = TAILQ_NEXT(m, listq);
        }
        PMAP_UNLOCK(pmap);
}

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	PMAP_LOCK(pmap);
	pmap_enter_quick_locked(pmap, va, m, prot);
	PMAP_UNLOCK(pmap);

}

static void
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{

	tte_t tte_data;

	if (pmap->pm_context)
		KDPRINTF("pmap_enter_quick(ctx=0x%lx va=%lx, pa=0x%lx prot=%x)\n", 
			pmap->pm_context, va, VM_PAGE_TO_PHYS(m), prot);

        PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (tte_hash_lookup(pmap->pm_hash, va))
		return;
		
	tte_data = VM_PAGE_TO_PHYS(m);
	/*
	 * Enter on the PV list if part of our managed memory. Note that we
	 * raise IPL while manipulating pv_table since pmap_enter can be
	 * called at interrupt time.
	 */
	if ((m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0) {
		pmap_insert_entry(pmap, va, m);
		tte_data |= VTD_MANAGED;
	}

	pmap->pm_stats.resident_count++;

	if ((prot & VM_PROT_EXECUTE) != 0)
		tte_data |= VTD_X;

	if (pmap == kernel_pmap)
		tte_data |= TTE_KERNEL_MINFLAGS;

	tte_hash_insert(pmap->pm_hash, va, tte_data | TTE_MINFLAGS);

}

/*
 * Extract the physical page address associated with the given
 * map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t pa;
	tte_t tte_data;

	tte_data = tte_hash_lookup(pmap->pm_hash, va);
	pa = TTE_GET_PA(tte_data) | (va & TTE_GET_PAGE_MASK(tte_data));

	return (pa);
}

/*
 * Atomically extract and hold the physical page with the given
 * pmap and virtual address pair if that mapping permits the given
 * protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	tte_t tte_data;
	vm_page_t m;

	m = NULL;
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	sched_pin();
	tte_data = tte_hash_lookup(pmap->pm_hash, va);
	if (tte_data != 0 && 
	    ((tte_data & VTD_SW_W) || (prot & VM_PROT_WRITE) == 0)) {
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tte_data));
		vm_page_hold(m);
	}
	sched_unpin();
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);

	return (m);
}

void *
pmap_alloc_zeroed_contig_pages(int npages, uint64_t alignment)
{
	vm_page_t m, tm;
	int i;
	void *ptr;
	
	m = NULL;
	while (m == NULL) {
		m = vm_page_alloc_contig(npages, phys_avail[0], 
					 phys_avail[1], alignment, (1UL<<34));
		if (m == NULL) {
			printf("vm_page_alloc_contig failed - waiting to retry\n");
			VM_WAIT;
		}
	}
	for (i = 0, tm = m; i < npages; i++, tm++) {
		tm->wire_count++;
		if ((tm->flags & PG_ZERO) == 0)
			pmap_zero_page(tm);
	}
	ptr = (void *)TLB_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m));
	
	return (ptr);
}

void
pmap_free_contig_pages(void *ptr, int npages)
{
	int i;
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(TLB_DIRECT_TO_PHYS((vm_offset_t)ptr));
	for (i = 0; i < npages; i++, m++) {
		m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_free(m);
	}
}

void 
pmap_growkernel(vm_offset_t addr)
{
	return;
}

void 
pmap_init(void)
{

	/* allocate pv_entry zones */
	int shpgperproc = PMAP_SHPGPERPROC;

	for (ctx_stack_top = 1; ctx_stack_top < PMAP_CONTEXT_MAX; ctx_stack_top++) 
		ctx_stack[ctx_stack_top] = ctx_stack_top;

	mtx_init(&pmap_ctx_lock, "ctx lock", NULL, MTX_SPIN);

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
	uma_zone_set_obj(pvzone, &pvzone_obj, pv_entry_max);

	tte_hash_init();

}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	KDPRINTF("pmap_insert_entry(va=0x%lx, pa=0x%lx)\n", va, VM_PAGE_TO_PHYS(m));
	pv = get_pv_entry(pmap);
	pv->pv_va = va;
	pv->pv_pmap = pmap;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;
}

#ifdef TRAP_TRACING
static int trap_trace_report_done;
#endif

#ifdef SMP
static cpumask_t
pmap_ipi(pmap_t pmap, char *func, uint64_t arg1, uint64_t arg2)
{

	int i, cpu_count, retried;
	u_int cpus;
	cpumask_t cpumask, active, curactive;
	cpumask_t active_total, ackmask;
	uint16_t *cpulist;

	retried = 0;

	if (!smp_started)
		return (0);

	cpumask = PCPU_GET(cpumask);
	cpulist = PCPU_GET(cpulist);
	curactive = 0;

	if (rdpr(pil) != 14)
		panic("pil %ld != 14", rdpr(pil));

#ifndef CPUMASK_NOT_BEING_ERRONEOUSLY_CHANGED
	/* by definition cpumask should have curcpu's bit set */
	if (cpumask != (1 << curcpu)) 
		panic("cpumask(0x%x) != (1 << curcpu) (0x%x)\n", 
		      cpumask, (1 << curcpu));

#endif
#ifdef notyet
	if ((active_total = (pmap->pm_tlbactive & ~cpumask)) == 0)
		goto done;

	if (pmap->pm_context != 0)
		active_total = active = (pmap->pm_tlbactive & ~cpumask);
	else 
#endif
		active_total = active = PCPU_GET(other_cpus);

	if (active == 0)
		goto done;
	
 retry:
	
	for (i = curactive = cpu_count = 0, cpus = active; i < mp_ncpus && cpus; i++, cpus = (cpus>>1)) {
		if ((cpus & 0x1) == 0)
			continue;
		
		curactive |= (1 << i);
		cpulist[cpu_count] = (uint16_t)i;
		cpu_count++;
	}

	ackmask = 0;
	cpu_ipi_selected(cpu_count, cpulist, (uint64_t)func, (uint64_t)arg1, 
			 (uint64_t)arg2, (uint64_t *)&ackmask);

	while (ackmask != curactive) {
		membar(Sync);
		i++;
		if (i > 10000000) {
#ifdef TRAP_TRACING
			int j;
#endif
			uint64_t cpu_state;
			printf("cpu with cpumask=0x%x appears to not be responding to ipis\n",
			       curactive & ~ackmask);

#ifdef TRAP_TRACING
			if (!trap_trace_report_done) {
				trap_trace_report_done = 1;
				for (j = 0; j < MAXCPU; j++)
					if (((1 << j) & curactive & ~ackmask) != 0) {
						struct pcpu *pc = pcpu_find(j);
						printf("pcpu pad 0x%jx 0x%jx 0x%jx 0x%jx 0x%jx 0x%jx 0x%jx\n",
						    pc->pad[0], pc->pad[1], pc->pad[2], pc->pad[3],
						    pc->pad[4], pc->pad[5], pc->pad[6]);
						trap_trace_report(j);
					}
			}
#endif

			hv_cpu_state((uint64_t)ffs64(curactive & ~ackmask), &cpu_state);
			printf("cpu_state of %ld is %ld\n", ffs64(curactive & ~ackmask), cpu_state);
			if (!retried) {
				printf("I'm going to send off another ipi just to confirm that it isn't a memory barrier bug\n"
			       "and then I'm going to panic\n");

				retried = 1;
				goto retry;
			}

			panic(" ackmask=0x%x active=0x%x\n", ackmask, curactive);
		}
	}

	active_total |= curactive;
	if ((active = ((pmap->pm_tlbactive & all_cpus) & ~(active_total|cpumask))) != 0) {
		printf("pmap_ipi: retrying");
		goto retry;
	}
 done:
	return (active_total);
}
#endif

void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va, int cleartsb)
{

	if (cleartsb == TRUE)
		tsb_clear_tte(&pmap->pm_tsb, va);

	DPRINTF("pmap_invalidate_page(va=0x%lx)\n", va);
	spinlock_enter();
	invlpg(va, pmap->pm_context);
#ifdef SMP
	pmap_ipi(pmap, (void *)tl_invlpg, (uint64_t)va, (uint64_t)pmap->pm_context);
#endif
	spinlock_exit();
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int cleartsb)
{
	vm_offset_t tva;
#ifdef SMP
	char *func;
	cpumask_t active;
#endif
	if ((eva - sva) == PAGE_SIZE) {
		pmap_invalidate_page(pmap, sva, cleartsb);
		return;
	}
	

	if (sva >= eva) 
		panic("invalidating negative or zero range sva=0x%lx eva=0x%lx", sva, eva);

	if (cleartsb == TRUE) 
		tsb_clear_range(&pmap->pm_tsb, sva, eva);

	spinlock_enter();
	if ((sva - eva) < PAGE_SIZE*64) {
		for (tva = sva; tva < eva; tva += PAGE_SIZE_8K)
			invlpg(tva, pmap->pm_context);
	} else if (pmap->pm_context) 
		invlctx(pmap->pm_context);
	else 
		invltlb();
	
#ifdef SMP
	if (pmap == kernel_pmap)
		func = tl_invltlb;
	else
		func = tl_invlctx;

	active = pmap_ipi(pmap, (void *)func, pmap->pm_context, 0);
	active &= ~pmap->pm_active;
	atomic_clear_int(&pmap->pm_tlbactive, active);
#endif
	spinlock_exit();
}

void
pmap_invalidate_all(pmap_t pmap)
{

	if (pmap == kernel_pmap)
		panic("invalidate_all called on kernel_pmap");

	tsb_clear(&pmap->pm_tsb);

	spinlock_enter();
	invlctx(pmap->pm_context);
#ifdef SMP
	pmap_ipi(pmap, tl_invlctx, pmap->pm_context, 0);
	pmap->pm_tlbactive = pmap->pm_active;
#endif
	spinlock_exit();
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	return (tte_get_phys_bit(m, VTD_W));
}


boolean_t 
pmap_is_prefaultable(pmap_t pmap, vm_offset_t va)
{
	return (tte_hash_lookup(pmap->pm_hash, va) == 0);
}

/*
 * Map a wired page into kernel virtual address space.
 */
static void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	tte_hash_update(kernel_pmap->pm_hash, va, pa | TTE_KERNEL | VTD_8K);
	tsb_set_tte(&kernel_td[TSB8K_INDEX], va, pa | TTE_KERNEL | VTD_8K, 0);
}

/*
 * Extract the physical page address associated with the given kernel virtual
 * address.
 */

vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	tte_t tte_data;
	vm_paddr_t pa;

        pa = 0;
	if (va > KERNBASE && va < KERNBASE + nucleus_memory) {
		uint64_t offset;
		offset = va - KERNBASE; 
		pa = nucleus_mappings[offset >> 22] | (va & PAGE_MASK_4M);
	}
	if ((pa == 0) && (tte_data = tsb_lookup_tte(va, 0)) != 0)
		pa = TTE_GET_PA(tte_data) | (va & TTE_GET_PAGE_MASK(tte_data));

	if ((pa == 0) && (tte_data = tte_hash_lookup(kernel_pmap->pm_hash, va)) != 0)
		pa = TTE_GET_PA(tte_data) | (va & TTE_GET_PAGE_MASK(tte_data));

	return pa;
}

static void
pmap_kremove(vm_offset_t va)
{ 
	tte_hash_delete(kernel_pmap->pm_hash, va);
}

/*
 * Map a range of physical addresses into kernel virtual address space.
 *
 * The value passed in *virt is a suggested virtual address for the mapping.
 * Architectures which can support a direct-mapped physical to virtual region
 * can return the appropriate address within that region, leaving '*virt'
 * unchanged.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	return TLB_PHYS_TO_DIRECT(start);
}

int 
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	return (0);
}

void 
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object, 
		    vm_pindex_t index, vm_size_t size)
{
	printf("pmap_object_init_pt\n");
	return;
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

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pmap) {
			return TRUE;
		}
		loops++;
		if (loops >= 16)
			break;
	}	

	return (FALSE);
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pv_list_count = 0;
}
/*
 * Lower the permission for all mappings to a given page.
 */
void
pmap_remove_write(vm_page_t m)
{

	tte_clear_phys_bit(m, VTD_SW_W);
}
/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pmap)
{
	PMAP_LOCK_INIT(pmap);
	pmap->pm_active = pmap->pm_tlbactive = ~0;
	pmap->pm_context = 0;
	pmap->pm_tsb_ra = kernel_pmap->pm_tsb_ra;
	pmap->pm_hash = kernel_pmap->pm_hash;
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Initialize a preallocated and zeroed pmap structure, such as one in a
 * vmspace structure.
 */
void
pmap_pinit(pmap_t pmap)
{

	pmap->pm_context = get_context();
	pmap->pm_tsb_ra = vtophys(&pmap->pm_tsb);

	vm_page_lock_queues();
	pmap->pm_hash = tte_hash_create(pmap->pm_context, &pmap->pm_hashscratch);
	tsb_init(&pmap->pm_tsb, &pmap->pm_tsbscratch, TSB_INIT_SHIFT);
	vm_page_unlock_queues();
	pmap->pm_active = pmap->pm_tlbactive = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	PMAP_LOCK_INIT(pmap);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Set the physical protection on the specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{

	int anychanged;
	vm_offset_t tva;

	DPRINTF("pmap_protect(0x%lx, 0x%lx, %d)\n", sva, eva, prot);
	
	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	
	if (prot & VM_PROT_WRITE)
		return;

	anychanged = 0;

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	sched_pin();

	for (tva = sva; tva < eva; tva += PAGE_SIZE) {
		uint64_t otte_data;
		vm_page_t m;

		if ((otte_data = tte_hash_clear_bits(pmap->pm_hash, tva, 
						     (VTD_SW_W|VTD_W))) == 0)
			continue;

		if (!anychanged && (otte_data & VTD_W))
			anychanged = 1;
		
		if (otte_data & VTD_MANAGED) {
			m = NULL;

			if (otte_data & VTD_REF) {
				m = PHYS_TO_VM_PAGE(TTE_GET_PA(otte_data));
				vm_page_flag_set(m, PG_REFERENCED);
			}
			if (otte_data & VTD_W) {
				m = PHYS_TO_VM_PAGE(TTE_GET_PA(otte_data));
				vm_page_dirty(m);
			}
		} 
	}

	vm_page_unlock_queues();

	if (anychanged)
		pmap_invalidate_range(pmap, sva, eva, TRUE);
	sched_unpin();
	PMAP_UNLOCK(pmap);
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

	va = sva;
	while (count-- > 0) {
		pmap_kenter(va, VM_PAGE_TO_PHYS(*m));
		va += PAGE_SIZE;
		m++;
	}
	pmap_invalidate_range(kernel_pmap, sva, va, FALSE);
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by pmap_qenter.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;

	va = sva;

#ifndef NFS_NOT_BROKEN
	if (count == 0)
		count = 1;
#endif
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va, TRUE);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));

	tsb_deinit(&pmap->pm_tsb);
	tte_hash_destroy(pmap->pm_hash);
	free_context(pmap->pm_context);
	PMAP_LOCK_DESTROY(pmap);
}

/*
 * Remove the given range of addresses from the specified map.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t start, vm_offset_t end)
{
	int invlva;
	vm_offset_t tva;
	uint64_t tte_data;
	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;
	
	DPRINTF("pmap_remove(start=0x%lx, end=0x%lx)\n", 
		start, end);
	invlva = 0;
	vm_page_lock_queues();
	sched_pin();
	PMAP_LOCK(pmap);
	for (tva = start; tva < end; tva += PAGE_SIZE) {
		if ((tte_data = tte_hash_delete(pmap->pm_hash, tva)) == 0)
			continue;
		pmap_remove_tte(pmap, tte_data, tva);
		if (!invlva && (tte_data & (VTD_REF|VTD_W)))
			invlva = 1;
	}
	vm_page_unlock_queues();

	if (invlva)
		pmap_invalidate_range(pmap, start, end, TRUE);

	sched_unpin();
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
	uint64_t tte_data;
	DPRINTF("pmap_remove_all 0x%lx\n", VM_PAGE_TO_PHYS(m));

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	sched_pin();
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		PMAP_LOCK(pv->pv_pmap);
		pv->pv_pmap->pm_stats.resident_count--;

		tte_data = tte_hash_delete(pv->pv_pmap->pm_hash, pv->pv_va);

		if (tte_data & VTD_WIRED)
			pv->pv_pmap->pm_stats.wired_count--;
		if (tte_data & VTD_REF)
			vm_page_flag_set(m, PG_REFERENCED);
		
		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if (tte_data & VTD_W) {
			KASSERT((tte_data & VTD_SW_W),
	("pmap_remove_all: modified page not writable: va: %lx, tte: %lx",
			    pv->pv_va, tte_data));
			vm_page_dirty(m);
		}
	
		pmap_invalidate_page(pv->pv_pmap, pv->pv_va, TRUE);
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		PMAP_UNLOCK(pv->pv_pmap);
		free_pv_entry(pv);
	}
	vm_page_flag_clear(m, PG_WRITEABLE);
	sched_unpin();
}

static void
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;
	if (pmap != kernel_pmap)
		DPRINTF("pmap_remove_entry(va=0x%lx, pa=0x%lx)\n", va, VM_PAGE_TO_PHYS(m));
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
	KASSERT(pv != NULL, ("pmap_remove_entry: pv not found va=0x%lx pa=0x%lx", va, VM_PAGE_TO_PHYS(m)));
	TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count--;
	if (TAILQ_EMPTY(&m->md.pv_list))
		vm_page_flag_clear(m, PG_WRITEABLE);
	TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
	free_pv_entry(pv);
}


void
pmap_remove_pages(pmap_t pmap)
{
	
	vm_page_t m;
	pv_entry_t pv, npv;
	tte_t tte_data;
	
	DPRINTF("pmap_remove_pages(ctx=0x%lx)\n", pmap->pm_context);
	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	sched_pin();
	for (pv = TAILQ_FIRST(&pmap->pm_pvlist); pv; pv = npv) {
		tte_data = tte_hash_delete(pmap->pm_hash, pv->pv_va);
		
		if (tte_data == 0) {
			membar(Sync);
			DELAY(100);
			membar(Sync);
			tte_data = tte_hash_delete(pmap->pm_hash, pv->pv_va);
			if (tte_data == 0) {
				printf("TTE IS ZERO @ VA %016lx\n", pv->pv_va);
				panic("bad tte");
			}
		}
		
		if (tte_data & VTD_WIRED) 
			pmap->pm_stats.wired_count--;

		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tte_data));

		pmap->pm_stats.resident_count--;
		
		if (tte_data & VTD_W) {
			vm_page_dirty(m);
		}
		
		npv = TAILQ_NEXT(pv, pv_plist);
		TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
		
		m->md.pv_list_count--;
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		if (TAILQ_EMPTY(&m->md.pv_list))
			vm_page_flag_clear(m, PG_WRITEABLE);

		free_pv_entry(pv);
	}
	pmap->pm_hash = tte_hash_reset(pmap->pm_hash, &pmap->pm_hashscratch);
	vm_page_unlock_queues();

	pmap_invalidate_all(pmap);
	sched_unpin();
	PMAP_UNLOCK(pmap);
}

void
pmap_scrub_pages(vm_paddr_t pa, int64_t size)
{
	uint64_t bytes_zeroed;
	while (size > 0) {
		hv_mem_scrub(pa, size, &bytes_zeroed);
		pa += bytes_zeroed;
		size -= bytes_zeroed;
	}
}

static void
pmap_remove_tte(pmap_t pmap, tte_t tte_data, vm_offset_t va)
{
	
	vm_page_t m;

	if (pmap != kernel_pmap)
		DPRINTF("pmap_remove_tte(va=0x%lx, pa=0x%lx)\n", va, TTE_GET_PA(tte_data));

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (tte_data & VTD_WIRED)
		pmap->pm_stats.wired_count--;

	pmap->pm_stats.resident_count--;
	
	if (tte_data & VTD_MANAGED) {
		m = PHYS_TO_VM_PAGE(TTE_GET_PA(tte_data));
		if (tte_data & VTD_W) {
			vm_page_dirty(m);	
		}
		if (tte_data & VTD_REF) 
			vm_page_flag_set(m, PG_REFERENCED);
		pmap_remove_entry(pmap, m, va);
	}
}

static void
pmap_tte_hash_resize(pmap_t pmap)
{
	tte_hash_t old_th = pmap->pm_hash;

	pmap->pm_hash = tte_hash_resize(pmap->pm_hash);

	if (curthread->td_proc->p_numthreads != 1) 
		pmap_ipi(pmap, tl_ttehashupdate, pmap->pm_context, pmap->pm_hashscratch);

	pmap->pm_hashscratch = tte_hash_set_scratchpad_user(pmap->pm_hash, pmap->pm_context);	
	tte_hash_destroy(old_th);
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
	
	int rv;
	pv_entry_t pv, pvf, pvn;
	pmap_t pmap;
	tte_t otte_data;

	rv = 0;
	if (m->flags & PG_FICTITIOUS)
		return (rv);

        sched_pin();
        mtx_assert(&vm_page_queue_mtx, MA_OWNED);
        if ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		
		pvf = pv;

		do {
                        pvn = TAILQ_NEXT(pv, pv_list);
			
                        TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
			
                        TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
			
                        pmap = pv->pv_pmap;
                        PMAP_LOCK(pmap);
			otte_data = tte_hash_clear_bits(pmap->pm_hash, pv->pv_va, VTD_REF);
			if ((otte_data & VTD_REF) != 0) {
                                pmap_invalidate_page(pmap, pv->pv_va, TRUE);
				
                                rv++;
                                if (rv > 4) {
                                        PMAP_UNLOCK(pmap);
                                        break;
                                }
			}
		
			PMAP_UNLOCK(pmap);
		} while ((pv = pvn) != NULL && pv != pvf);
	}
	sched_unpin();

	return (rv);
}

void
pmap_zero_page(vm_page_t m)
{
	hwblkclr((void *)TLB_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m)), PAGE_SIZE);
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_paddr_t pa;
	vm_offset_t va;
		
	pa = VM_PAGE_TO_PHYS(m);
	va = TLB_PHYS_TO_DIRECT(pa);
	
	bzero((char *)(va + off), size);

}

void
pmap_zero_page_idle(vm_page_t m)
{
	hwblkclr((void *)TLB_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m)), PAGE_SIZE);
}

void
pmap_set_ctx_panic(uint64_t error, vm_paddr_t tsb_ra, pmap_t pmap)
{
	panic("setting ctxnon0 failed ctx=0x%lx hvtsb_ra=0x%lx tsbscratch=0x%lx error=0x%lx",
	      pmap->pm_context, tsb_ra, pmap->pm_tsbscratch, error);
	
}
