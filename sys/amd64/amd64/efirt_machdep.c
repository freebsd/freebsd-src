/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/clock.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#include <isa/rtc.h>
#include <machine/efi.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>

static pml5_entry_t *efi_pml5;
static pml4_entry_t *efi_pml4;
static vm_object_t obj_1t1_pt;
static vm_page_t efi_pmltop_page;
static vm_pindex_t efi_1t1_idx;

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
	efi_pml4 = NULL;
	efi_pml5 = NULL;
	efi_pmltop_page = NULL;
}

/*
 * Map a physical address from EFI runtime space into KVA space.  Returns 0 to
 * indicate a failed mapping so that the caller may handle error.
 */
vm_offset_t
efi_phys_to_kva(vm_paddr_t paddr)
{

	if (paddr >= dmaplimit)
		return (0);
	return (PHYS_TO_DMAP(paddr));
}

static vm_page_t
efi_1t1_page(void)
{

	return (vm_page_grab(obj_1t1_pt, efi_1t1_idx++, VM_ALLOC_NOBUSY |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO));
}

static pt_entry_t *
efi_1t1_pte(vm_offset_t va)
{
	pml5_entry_t *pml5e;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_page_t m;
	vm_pindex_t pml5_idx, pml4_idx, pdp_idx, pd_idx;
	vm_paddr_t mphys;

	pml4_idx = pmap_pml4e_index(va);
	if (la57) {
		pml5_idx = pmap_pml5e_index(va);
		pml5e = &efi_pml5[pml5_idx];
		if (*pml5e == 0) {
			m = efi_1t1_page();
			mphys = VM_PAGE_TO_PHYS(m);
			*pml5e = mphys | X86_PG_RW | X86_PG_V;
		} else {
			mphys = *pml5e & PG_FRAME;
		}
		pml4e = (pml4_entry_t *)PHYS_TO_DMAP(mphys);
		pml4e = &pml4e[pml4_idx];
	} else {
		pml4e = &efi_pml4[pml4_idx];
	}

	if (*pml4e == 0) {
		m = efi_1t1_page();
		mphys =  VM_PAGE_TO_PHYS(m);
		*pml4e = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pml4e & PG_FRAME;
	}

	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(mphys);
	pdp_idx = pmap_pdpe_index(va);
	pdpe += pdp_idx;
	if (*pdpe == 0) {
		m = efi_1t1_page();
		mphys =  VM_PAGE_TO_PHYS(m);
		*pdpe = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pdpe & PG_FRAME;
	}

	pde = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	pd_idx = pmap_pde_index(va);
	pde += pd_idx;
	if (*pde == 0) {
		m = efi_1t1_page();
		mphys = VM_PAGE_TO_PHYS(m);
		*pde = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pde & PG_FRAME;
	}

	pte = (pt_entry_t *)PHYS_TO_DMAP(mphys);
	pte += pmap_pte_index(va);
	KASSERT(*pte == 0, ("va %#jx *pt %#jx", va, *pte));

	return (pte);
}

bool
efi_create_1t1_map(struct efi_md *map, int ndesc, int descsz)
{
	struct efi_md *p;
	pt_entry_t *pte;
	void *pml;
	vm_page_t m;
	vm_offset_t va;
	uint64_t idx;
	int bits, i, mode;

	obj_1t1_pt = vm_pager_allocate(OBJT_PHYS, NULL, ptoa(1 +
	    NPML4EPG + NPML4EPG * NPDPEPG + NPML4EPG * NPDPEPG * NPDEPG),
	    VM_PROT_ALL, 0, NULL);
	efi_1t1_idx = 0;
	VM_OBJECT_WLOCK(obj_1t1_pt);
	efi_pmltop_page = efi_1t1_page();
	VM_OBJECT_WUNLOCK(obj_1t1_pt);
	pml = (void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(efi_pmltop_page));
	if (la57) {
		efi_pml5 = pml;
		pmap_pinit_pml5(efi_pmltop_page);
	} else {
		efi_pml4 = pml;
		pmap_pinit_pml4(efi_pmltop_page);
	}

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
		else if ((p->md_attr & EFI_MD_ATTR_WP) != 0)
			mode = VM_MEMATTR_WRITE_PROTECTED;
		else if ((p->md_attr & EFI_MD_ATTR_UC) != 0)
			mode = VM_MEMATTR_UNCACHEABLE;
		else {
			if (bootverbose)
				printf("EFI Runtime entry %d mapping "
				    "attributes unsupported\n", i);
			mode = VM_MEMATTR_UNCACHEABLE;
		}
		bits = pmap_cache_bits(kernel_pmap, mode, false) | X86_PG_RW |
		    X86_PG_V;
		VM_OBJECT_WLOCK(obj_1t1_pt);
		for (va = p->md_phys, idx = 0; idx < p->md_pages; idx++,
		    va += PAGE_SIZE) {
			pte = efi_1t1_pte(va);
			pte_store(pte, va | bits);

			m = PHYS_TO_VM_PAGE(va);
			if (m != NULL && VM_PAGE_TO_PHYS(m) == 0) {
				vm_page_init_page(m, va, -1,
				    VM_FREEPOOL_DEFAULT);
				m->order = VM_NFREEORDER + 1; /* invalid */
				m->pool = VM_NFREEPOOL + 1; /* invalid */
				pmap_page_set_memattr_noflush(m, mode);
			}
		}
		VM_OBJECT_WUNLOCK(obj_1t1_pt);
	}

	return (true);

fail:
	efi_destroy_1t1_map();
	return (false);
}

/*
 * Create an environment for the EFI runtime code call.  The most
 * important part is creating the required 1:1 physical->virtual
 * mappings for the runtime segments.  To do that, we manually create
 * page table which unmap userspace but gives correct kernel mapping.
 * The 1:1 mappings for runtime segments usually occupy low 4G of the
 * physical address map.
 *
 * The 1:1 mappings were chosen over the SetVirtualAddressMap() EFI RT
 * service, because there are some BIOSes which fail to correctly
 * relocate itself on the call, requiring both 1:1 and virtual
 * mapping.  As result, we must provide 1:1 mapping anyway, so no
 * reason to bother with the virtual map, and no need to add a
 * complexity into loader.
 *
 * There is no need to disable interrupts around the change of %cr3,
 * the kernel mappings are correct, while we only grabbed the
 * userspace portion of VA.  Interrupts handlers must not access
 * userspace.  Having interrupts enabled fixes the issue with
 * firmware/SMM long operation, which would negatively affect IPIs,
 * esp. TLB shootdown requests.
 */
int
efi_arch_enter(void)
{
	pmap_t curpmap;
	uint64_t cr3;

	curpmap = PCPU_GET(curpmap);
	PMAP_LOCK_ASSERT(curpmap, MA_OWNED);
	curthread->td_md.md_efirt_dis_pf = vm_fault_disable_pagefaults();

	/*
	 * IPI TLB shootdown handler invltlb_pcid_handler() reloads
	 * %cr3 from the curpmap->pm_cr3, which would disable runtime
	 * segments mappings.  Block the handler's action by setting
	 * curpmap to impossible value.  See also comment in
	 * pmap.c:pmap_activate_sw().
	 */
	if (pmap_pcid_enabled && !invpcid_works)
		PCPU_SET(curpmap, NULL);

	cr3 = VM_PAGE_TO_PHYS(efi_pmltop_page);
	if (pmap_pcid_enabled)
		cr3 |= pmap_get_pcid(curpmap);
	load_cr3(cr3);
	/*
	 * If PCID is enabled, the clear CR3_PCID_SAVE bit in the loaded %cr3
	 * causes TLB invalidation.
	 */
	if (!pmap_pcid_enabled)
		invltlb();
	return (0);
}

void
efi_arch_leave(void)
{
	pmap_t curpmap;
	uint64_t cr3;

	curpmap = &curproc->p_vmspace->vm_pmap;
	cr3 = curpmap->pm_cr3;
	if (pmap_pcid_enabled) {
		cr3 |= pmap_get_pcid(curpmap);
		if (!invpcid_works)
			PCPU_SET(curpmap, curpmap);
	}
	load_cr3(cr3);
	if (!pmap_pcid_enabled)
		invltlb();
	vm_fault_enable_pagefaults(curthread->td_md.md_efirt_dis_pf);
}

/* XXX debug stuff */
static int
efi_time_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct efi_tm tm;
	int error, val;

	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	error = efi_get_time(&tm);
	if (error == 0) {
		uprintf("EFI reports: Year %d Month %d Day %d Hour %d Min %d "
		    "Sec %d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
		    tm.tm_min, tm.tm_sec);
	}
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, efi_time,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    efi_time_sysctl_handler, "I",
    "");
