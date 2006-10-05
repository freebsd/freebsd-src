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
#define TSB_MASK(tsb) ((tsb->hvtsb_ntte) - 1)
/* make TSB start off at the same size as the hash */
#define TSB_SIZE       8

#ifdef DEBUG_TSB
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

void tsb_sysinit(void);

vm_paddr_t
tsb_init(hv_tsb_info_t *hvtsb, uint64_t *scratchval)
{
	vm_page_t m;
	int i;
	uint64_t tsb_pages;	

	m = NULL;
	while (m == NULL) {
		m = vm_page_alloc_contig(TSB_SIZE, phys_avail[0], 
					 phys_avail[1], TSB_SIZE*PAGE_SIZE, (1UL<<34));
		if (m == NULL) {
			printf("vm_page_alloc_contig failed - waiting to retry\n");
			VM_WAIT;
		}
	}
	if ((VM_PAGE_TO_PHYS(m) & (TSB_SIZE*PAGE_SIZE - 1)) != 0)
	    panic("vm_page_alloc_contig allocated unaligned pages: 0x%lx",
		  VM_PAGE_TO_PHYS(m));

	hvtsb->hvtsb_idxpgsz = TTE8K;
	hvtsb->hvtsb_assoc = 1;
	hvtsb->hvtsb_ntte = (TSB_SIZE*PAGE_SIZE >> TTE_SHIFT);
	hvtsb->hvtsb_ctx_index = -1;    /* TSBs aren't shared so if we don't 
					 * set the context in the TTEs we can 
					 * simplify miss handling slightly
					 */
	hvtsb->hvtsb_pgszs = TSB8K;
	hvtsb->hvtsb_rsvd = 0;
	hvtsb->hvtsb_pa = VM_PAGE_TO_PHYS(m);

	for (i = 0; i < TSB_SIZE; i++, m++) 
		if ((m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);

	tsb_pages = hvtsb->hvtsb_ntte >> (PAGE_SHIFT - TTE_SHIFT);
	*scratchval = TLB_PHYS_TO_DIRECT(hvtsb->hvtsb_pa) | tsb_pages;

	return vtophys(hvtsb);
}

void
tsb_deinit(hv_tsb_info_t *hvtsb)
{
	vm_page_t m, tm;
	int i;
	

	m = PHYS_TO_VM_PAGE((vm_paddr_t)hvtsb->hvtsb_pa);
	for (i = 0, tm = m; i < TSB_SIZE; i++, m++) {
		tm->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
	}
	vm_page_release_contig(m, TSB_SIZE);
}


void
tsb_assert_invalid(hv_tsb_info_t *tsb, vm_offset_t va)
{
	vm_paddr_t tsb_load_pa;
	uint64_t tsb_index, tsb_shift, tte_tag, tte_data;
	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	tsb_load_pa = tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t);
	load_real_dw(tsb_load_pa, &tte_tag, &tte_data);
	if (tte_tag == 0 && tte_data == 0)
		return;
	printf("tsb_shift=0x%lx tsb_index=0x%lx\n", tsb_shift, tsb_index);
	printf("tte_tag=0x%lx tte_data=0x%lx TSB_MASK=%lx\n", tte_tag, tte_data, (uint64_t)TSB_MASK(tsb));
	panic("non-zero entry found where not expected");

}

void 
tsb_set_tte_real(hv_tsb_info_t *tsb, vm_offset_t va, uint64_t tte_data, uint64_t ctx)
{
	vm_paddr_t tsb_store_pa;
	uint64_t tsb_index, tsb_shift, tte_tag;
	DPRINTF("tsb_set_tte va: 0x%lx idxpgsz: %x\n", va, tsb->hvtsb_idxpgsz);

	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);

	DPRINTF("tsb_shift: 0x%lx\n", tsb_shift);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	DPRINTF("tsb_index_absolute: 0x%lx tsb_index: 0x%lx\n", (va >> tsb_shift), tsb_index);
	tsb_store_pa = tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t);

	tte_data &= ~VTD_V;
	/* store new value with valid bit cleared 
	 * to avoid invalid intermediate value;
	 */
	store_real(tsb_store_pa + sizeof(uint64_t), tte_data);
	tte_data |= VTD_V;

	tte_tag = (ctx << TTARGET_CTX_SHIFT) | (va >> TTARGET_VA_SHIFT);
	store_real(tsb_store_pa, tte_tag); 
	store_real(tsb_store_pa + sizeof(uint64_t), tte_data);
} 


void 
tsb_set_tte(hv_tsb_info_t *tsb, vm_offset_t va, uint64_t tte_data, uint64_t ctx)
{

	uint64_t tsb_index, tsb_shift, tte_tag;
	tte_t *entry;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t));
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
	hwblkclr((void *)TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa), tsb->hvtsb_ntte << TTE_SHIFT);
}

void 
tsb_clear_tte(hv_tsb_info_t *tsb, vm_offset_t va)
{
	tte_t *entry;
	uint64_t tsb_index, tsb_shift;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t));
	
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
	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);

	for (tva = sva; tva < eva; tva += PAGE_SIZE) {
		tsb_index = (tva >> tsb_shift) & tsb_mask;
		entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t));
		*(entry + 1) = 0;
	}

	membar(Sync);
}

tte_t
tsb_get_tte(hv_tsb_info_t *tsb, vm_offset_t va)
{
	tte_t *entry;
	uint64_t tsb_index, tsb_shift, tte_tag, tte_data;

	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	entry = (tte_t *)TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t));
	tte_tag = *(entry);
	tte_data = *(entry + 1);

	if ((tte_tag << TTARGET_VA_SHIFT) == (va & ~PAGE_MASK_4M))
		return tte_data;

	return (0UL);
}
#if 0
tte_t
tsb_get_tte_real(hv_tsb_info_t *tsb, vm_offset_t va)
{
	vm_paddr_t tsb_load_pa;
	uint64_t tsb_index, tsb_shift, tte_tag, tte_data;

	DPRINTF("tsb_get_tte va: 0x%lx\n", va);
	tsb_shift = TTE_PAGE_SHIFT(tsb->hvtsb_idxpgsz);
	DPRINTF("tsb_shift: %lx\n", tsb_shift);
	tsb_index = (va >> tsb_shift) & TSB_MASK(tsb);
	DPRINTF("tsb_index_absolute: %lx tsb_index: %lx\n", (va >> tsb_shift), tsb_index);
	tsb_load_pa = tsb->hvtsb_pa + 2*tsb_index*sizeof(uint64_t);
	DPRINTF("load_real_dw - ra: %lx &tte_tag: %p &tte_data: %p \n", tsb_load_pa, &tte_tag, &tte_data);
	load_real_dw(tsb_load_pa, &tte_tag, &tte_data);
	DPRINTF("tte_data: %lx ctx: %lx  va: %lx\n", tte_data, tte_tag >> TTARGET_CTX_SHIFT, 
		tte_tag << TTARGET_VA_SHIFT);
	if ((tte_tag << TTARGET_VA_SHIFT) == (va & ~PAGE_MASK_4M))
		return tte_data;

	return (0UL);
}
#endif

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
	uint64_t tsb_pages, tsb_scratch;	
	tsb_pages = tsb->hvtsb_ntte >> (PAGE_SHIFT - TTE_SHIFT);
	tsb_scratch = TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa) | tsb_pages;
	
	set_tsb_kernel_scratchpad(tsb_scratch);
	membar(Sync);
	return tsb_scratch;
}

uint64_t
tsb_set_scratchpad_user(hv_tsb_info_t *tsb)
{
	uint64_t tsb_pages, tsb_scratch;	
	tsb_pages = tsb->hvtsb_ntte >> (PAGE_SHIFT - TTE_SHIFT);
	tsb_scratch = TLB_PHYS_TO_DIRECT(tsb->hvtsb_pa) | tsb_pages;
	set_tsb_user_scratchpad(tsb_scratch);
	membar(Sync);
	return tsb_scratch;
}
