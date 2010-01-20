/*-
 * Copyright (c) 2006 Kip Macy
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


#include "opt_ddb.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h> 
#include <vm/vm_extern.h> 
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#include <machine/cpufunc.h>
#include <machine/hypervisorvar.h>
#include <machine/smp.h>
#include <machine/mmu.h>
#include <machine/tte.h>
#include <machine/tte_hash.h>
#include <machine/tsb.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>

CTASSERT(sizeof(tte_t) == sizeof(uint64_t));
#define TSB_MASK(tsb) ((tsb->hti_ntte) - 1)
/* make TSB start off at the same size as the hash */
#define TSB_SIZE       8

#ifdef DEBUG_TSB
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

void tsb_sysinit(void);

void
tsb_init(hv_tsb_info_t *hvtsb, uint64_t *scratchval, uint64_t page_shift)
{
	void *ptr;
	int npages = (1 << page_shift);

	ptr = pmap_alloc_zeroed_contig_pages(npages, npages*PAGE_SIZE);
	
	if ((((uint64_t)ptr) & (npages*PAGE_SIZE - 1)) != 0)
		panic("vm_page_alloc_contig allocated unaligned pages: %p",
		      ptr);
	
	hvtsb->hti_idxpgsz = TTE8K;
	hvtsb->hti_assoc = 1;
	hvtsb->hti_ntte = (npages*PAGE_SIZE >> TTE_SHIFT);
	hvtsb->hti_ctx_index = -1;    /* TSBs aren't shared so if we don't 
					 * set the context in the TTEs we can 
					 * simplify miss handling slightly
					 */
	hvtsb->hti_pgszs = TSB8K;
	hvtsb->hti_rsvd = 0;
	hvtsb->hti_ra = TLB_DIRECT_TO_PHYS((vm_offset_t)ptr);

	*scratchval = ((uint64_t) ptr) | page_shift;
}

void
tsb_deinit(hv_tsb_info_t *hvtsb)
{
	vm_page_t m, tm;
	int i;
	

	m = PHYS_TO_VM_PAGE((vm_paddr_t)hvtsb->hti_ra);
	for (i = 0, tm = m; i < TSB_SIZE; i++, m++) {
		tm->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_free(tm);
	}
}


void
tsb_assert_invalid(hv_tsb_info_t *tsb, vm_offset_t va)
{
	vm_paddr_t tsb_load_pa;
	uint64_t tsb_index, tsb_shift, tte_tag, tte_data;
	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	tsb_load_pa = tsb->hti_ra + 2*tsb_index*sizeof(uint64_t);
	load_real_dw(tsb_load_pa, &tte_tag, &tte_data);
	if (tte_tag == 0 && tte_data == 0)
		return;
	printf("tsb_shift=0x%lx tsb_index=0x%lx\n", tsb_shift, tsb_index);
	printf("tte_tag=0x%lx tte_data=0x%lx TSB_MASK=%lx\n", tte_tag, tte_data, (uint64_t)TSB_MASK(tsb));
	panic("non-zero entry found where not expected");

}

void 
tsb_set_tte_real(hv_tsb_info_t *tsb, vm_offset_t index_va, vm_offset_t tag_va, 
		 uint64_t tte_data, uint64_t ctx)
{
	vm_paddr_t tsb_store_pa;
	uint64_t tsb_index, tsb_shift, tte_tag;
	DPRINTF("tsb_set_tte index_va: 0x%lx tag_va: 0x%lx idxpgsz: %x ", 
		index_va, tag_va, tsb->hti_idxpgsz);

	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);

	tsb_index = (index_va >> tsb_shift) & TSB_MASK(tsb);
	DPRINTF("tsb_index_absolute: 0x%lx tsb_index: 0x%lx\n", (index_va >> tsb_shift), tsb_index);
	tsb_store_pa = tsb->hti_ra + 2*tsb_index*sizeof(uint64_t);

	/* store new value with valid bit cleared 
	 * to avoid invalid intermediate value;
	 */
	store_real(tsb_store_pa + sizeof(uint64_t), tte_data);
	tte_tag = (ctx << TTARGET_CTX_SHIFT) | (tag_va >> TTARGET_VA_SHIFT);
	store_real(tsb_store_pa, tte_tag);
} 


void 
tsb_set_tte(hv_tsb_info_t *tsb, vm_offset_t va, uint64_t tte_data, uint64_t ctx)
{

	uint64_t tsb_index, tsb_shift, tte_tag;
	tte_t *entry;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hti_ra + 2*tsb_index*sizeof(uint64_t));
	tte_tag = (ctx << TTARGET_CTX_SHIFT) | (va >> TTARGET_VA_SHIFT);
	/* store new value with valid bit cleared 
	 * to avoid invalid intermediate value;
	 */
	*(entry + 1) = 0;
	membar(StoreLoad);
	*(entry) = tte_tag;
	*(entry + 1) = tte_data;
	membar(Sync);
} 


void 
tsb_clear(hv_tsb_info_t *tsb)
{
	hwblkclr((void *)TLB_PHYS_TO_DIRECT(tsb->hti_ra), tsb->hti_ntte << TTE_SHIFT);
}

void 
tsb_clear_tte(hv_tsb_info_t *tsb, vm_offset_t va)
{
	tte_t *entry;
	uint64_t tsb_index, tsb_shift;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hti_ra + 2*tsb_index*sizeof(uint64_t));
	
	*(entry + 1) = 0;

	membar(Sync);
}

void
tsb_clear_range(hv_tsb_info_t *tsb, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t tva;
	uint64_t tsb_index, tsb_shift, tsb_mask;
	tte_t *entry;

	tsb_mask = TSB_MASK(tsb);
	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);

	for (tva = sva; tva < eva; tva += PAGE_SIZE) {
		tsb_index = (tva >> tsb_shift) & tsb_mask;
		entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hti_ra + 2*tsb_index*sizeof(uint64_t));
		*(entry + 1) = 0;
	}

	membar(Sync);
}

tte_t
tsb_get_tte(hv_tsb_info_t *tsb, vm_offset_t va)
{
	tte_t *entry;
	uint64_t tsb_index, tsb_shift, tte_tag, tte_data;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hti_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hti_ra + 2*tsb_index*sizeof(uint64_t));
	tte_tag = *(entry);
	tte_data = *(entry + 1);

	if ((tte_tag << TTARGET_VA_SHIFT) == (va & ~PAGE_MASK_4M))
		return tte_data;

	return (0UL);
}

tte_t
tsb_lookup_tte(vm_offset_t va, uint64_t ctx)
{
	tte_t tte_data;

	tte_data = 0;

	if ((tte_data = tsb_get_tte(&kernel_td[TSB4M_INDEX], va)) != 0)
		goto done;

	/*
	 * handle user data 
	 */
done:
	return tte_data;
}

uint64_t
tsb_set_scratchpad_kernel(hv_tsb_info_t *tsb)
{
	uint64_t tsb_shift, tsb_scratch;
	tsb_shift = ffs(tsb->hti_ntte >> (PAGE_SHIFT - TTE_SHIFT)) - 1;
	tsb_scratch = TLB_PHYS_TO_DIRECT(tsb->hti_ra) | tsb_shift;
	
	set_tsb_kernel_scratchpad(tsb_scratch);
	membar(Sync);
	return tsb_scratch;
}

uint64_t
tsb_set_scratchpad_user(hv_tsb_info_t *tsb)
{
	uint64_t tsb_shift, tsb_scratch;
	tsb_shift = ffs(tsb->hti_ntte >> (PAGE_SHIFT - TTE_SHIFT)) - 1;
	tsb_scratch = TLB_PHYS_TO_DIRECT(tsb->hti_ra) | tsb_shift;
	set_tsb_user_scratchpad(tsb_scratch);
	membar(Sync);
	return tsb_scratch;
}

int
tsb_size(hv_tsb_info_t *hvtsb)
{
	return (hvtsb->hti_ntte >> (PAGE_SHIFT - TTE_SHIFT));
}

int
tsb_page_shift(pmap_t pmap)
{
	return (pmap->pm_tsbscratch & PAGE_MASK);
}
