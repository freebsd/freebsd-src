/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2016 The FreeBSD Foundation
 * Copyright (c) 2017 Andrew Turner
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/param.h>
#include <sys/efi.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <machine/pte.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>

static vm_object_t obj_1t1_pt;
static vm_pindex_t efi_1t1_idx;
static pd_entry_t *efi_l0;
static uint64_t efi_ttbr0;

void
efi_destroy_1t1_map(void)
{
	struct pctrie_iter pages;
	vm_page_t m;

	if (obj_1t1_pt != NULL) {
		vm_page_iter_init(&pages, obj_1t1_pt);
		VM_OBJECT_RLOCK(obj_1t1_pt);
		VM_RADIX_FOREACH(m, &pages)
			m->ref_count = VPRC_OBJREF;
		vm_wire_sub(obj_1t1_pt->resident_page_count);
		VM_OBJECT_RUNLOCK(obj_1t1_pt);
		vm_object_deallocate(obj_1t1_pt);
	}

	obj_1t1_pt = NULL;
	efi_1t1_idx = 0;
	efi_l0 = NULL;
	efi_ttbr0 = 0;
}

static vm_page_t
efi_1t1_page(void)
{

	return (vm_page_grab(obj_1t1_pt, efi_1t1_idx++, VM_ALLOC_NOBUSY |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO));
}

static pt_entry_t *
efi_1t1_l3(vm_offset_t va)
{
	pd_entry_t *l0, *l1, *l2;
	pt_entry_t *l3;
	vm_pindex_t l0_idx, l1_idx, l2_idx;
	vm_page_t m;
	vm_paddr_t mphys;

	l0_idx = pmap_l0_index(va);
	l0 = &efi_l0[l0_idx];
	if (*l0 == 0) {
		m = efi_1t1_page();
		mphys = VM_PAGE_TO_PHYS(m);
		*l0 = PHYS_TO_PTE(mphys) | L0_TABLE;
	} else {
		mphys = PTE_TO_PHYS(*l0);
	}

	l1 = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	l1_idx = pmap_l1_index(va);
	l1 += l1_idx;
	if (*l1 == 0) {
		m = efi_1t1_page();
		mphys = VM_PAGE_TO_PHYS(m);
		*l1 = PHYS_TO_PTE(mphys) | L1_TABLE;
	} else {
		mphys = PTE_TO_PHYS(*l1);
	}

	l2 = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	l2_idx = pmap_l2_index(va);
	l2 += l2_idx;
	if (*l2 == 0) {
		m = efi_1t1_page();
		mphys = VM_PAGE_TO_PHYS(m);
		*l2 = PHYS_TO_PTE(mphys) | L2_TABLE;
	} else {
		mphys = PTE_TO_PHYS(*l2);
	}

	l3 = (pt_entry_t *)PHYS_TO_DMAP(mphys);
	l3 += pmap_l3_index(va);
	KASSERT(*l3 == 0, ("%s: Already mapped: va %#jx *pt %#jx", __func__,
	    va, *l3));

	return (l3);
}

/*
 * Map a physical address from EFI runtime space into KVA space.  Returns 0 to
 * indicate a failed mapping so that the caller may handle error.
 */
vm_offset_t
efi_phys_to_kva(vm_paddr_t paddr)
{
	if (PHYS_IN_DMAP(paddr))
		return (PHYS_TO_DMAP(paddr));

	/* TODO: Map memory not in the DMAP */

	return (0);
}

/*
 * Create the 1:1 virtual to physical map for EFI
 */
bool
efi_create_1t1_map(struct efi_md *map, int ndesc, int descsz)
{
	struct efi_md *p;
	pt_entry_t *l3, l3_attr;
	vm_offset_t va;
	vm_page_t efi_l0_page;
	uint64_t idx;
	int i, mode;

	obj_1t1_pt = vm_pager_allocate(OBJT_PHYS, NULL, L0_ENTRIES +
	    L0_ENTRIES * Ln_ENTRIES + L0_ENTRIES * Ln_ENTRIES * Ln_ENTRIES +
	    L0_ENTRIES * Ln_ENTRIES * Ln_ENTRIES * Ln_ENTRIES,
	    VM_PROT_ALL, 0, NULL);
	VM_OBJECT_WLOCK(obj_1t1_pt);
	efi_l0_page = efi_1t1_page();
	VM_OBJECT_WUNLOCK(obj_1t1_pt);
	efi_l0 = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(efi_l0_page));
	efi_ttbr0 = ASID_TO_OPERAND(ASID_RESERVED_FOR_EFI) |
	    VM_PAGE_TO_PHYS(efi_l0_page);

	for (i = 0, p = map; i < ndesc; i++, p = efi_next_descriptor(p,
	    descsz)) {
		if ((p->md_attr & EFI_MD_ATTR_RT) == 0)
			continue;
		if (p->md_virt != 0 && p->md_virt != p->md_phys) {
			if (bootverbose)
				printf("EFI Runtime entry %d is mapped\n", i);
			goto fail;
		}
		if ((p->md_phys & EFI_PAGE_MASK) != 0) {
			if (bootverbose)
				printf("EFI Runtime entry %d is not aligned\n",
				    i);
			goto fail;
		}
		if (p->md_phys + p->md_pages * EFI_PAGE_SIZE < p->md_phys ||
		    p->md_phys + p->md_pages * EFI_PAGE_SIZE >=
		    VM_MAXUSER_ADDRESS) {
			printf("EFI Runtime entry %d is not in mappable for RT:"
			    "base %#016jx %#jx pages\n",
			    i, (uintmax_t)p->md_phys,
			    (uintmax_t)p->md_pages);
			goto fail;
		}
		if ((p->md_attr & EFI_MD_ATTR_WB) != 0)
			mode = VM_MEMATTR_WRITE_BACK;
		else if ((p->md_attr & EFI_MD_ATTR_WT) != 0)
			mode = VM_MEMATTR_WRITE_THROUGH;
		else if ((p->md_attr & EFI_MD_ATTR_WC) != 0)
			mode = VM_MEMATTR_WRITE_COMBINING;
		else
			mode = VM_MEMATTR_DEVICE;

		if (bootverbose) {
			printf("MAP %lx mode %x pages %lu\n",
			    p->md_phys, mode, p->md_pages);
		}

		l3_attr = ATTR_AF | pmap_sh_attr | ATTR_S1_IDX(mode) |
		    ATTR_S1_AP(ATTR_S1_AP_RW) | ATTR_S1_nG | L3_PAGE;
		if (mode == VM_MEMATTR_DEVICE || p->md_attr & EFI_MD_ATTR_XP)
			l3_attr |= ATTR_S1_XN;

		VM_OBJECT_WLOCK(obj_1t1_pt);
		for (va = p->md_phys, idx = 0; idx < p->md_pages;
		    idx += (PAGE_SIZE / EFI_PAGE_SIZE), va += PAGE_SIZE) {
			l3 = efi_1t1_l3(va);
			*l3 = va | l3_attr;
		}
		VM_OBJECT_WUNLOCK(obj_1t1_pt);
	}

	return (true);
fail:
	efi_destroy_1t1_map();
	return (false);
}

int
efi_arch_enter(void)
{

	CRITICAL_ASSERT(curthread);
	curthread->td_md.md_efirt_dis_pf = vm_fault_disable_pagefaults();

	/*
	 * Temporarily switch to EFI's page table.  However, we leave curpmap
	 * unchanged in order to prevent its ASID from being reclaimed before
	 * we switch back to its page table in efi_arch_leave().
	 */
	set_ttbr0(efi_ttbr0);
	if (PCPU_GET(bcast_tlbi_workaround) != 0)
		invalidate_local_icache();

	return (0);
}

void
efi_arch_leave(void)
{

	/*
	 * Restore the pcpu pointer. Some UEFI implementations trash it and
	 * we don't store it before calling into them. To fix this we need
	 * to restore it after returning to the kernel context. As reading
	 * curpmap will access x18 we need to restore it before loading
	 * the pmap pointer.
	 */
	__asm __volatile(
	    "mrs x18, tpidr_el1	\n"
	);
	set_ttbr0(pmap_to_ttbr0(PCPU_GET(curpmap)));
	if (PCPU_GET(bcast_tlbi_workaround) != 0)
		invalidate_local_icache();
	vm_fault_enable_pagefaults(curthread->td_md.md_efirt_dis_pf);
}

