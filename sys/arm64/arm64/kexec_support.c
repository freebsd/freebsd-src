/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Juniper Networks, Inc.
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/kexec.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>

#include <machine/armreg.h>
#include <machine/pmap.h>
#include <machine/pte.h>

/*
 * Idea behind this:
 *
 * kexec_load_md():
 * - Update boot page tables (identity map) to include all pages needed before
 *   disabling MMU.
 *
 * kexec_reboot_md():
 * - Copy pages into target(s)
 * - Do "other stuff"
 * - Does not return
 */

extern pt_entry_t pagetable_l0_ttbr0_bootstrap[];
extern unsigned long initstack_end[];
void switch_stack(void *, void (*)(void *, void *, struct kexec_image *), void *);

#define	SCTLR_EL1_NO_MMU	(SCTLR_RES1 | SCTLR_LSMAOE | SCTLR_nTLSMD | \
		SCTLR_EIS | SCTLR_TSCXT | SCTLR_EOS)
#define	vm_page_offset(m)	((vm_offset_t)(m) - vm_page_base)
static inline vm_page_t
phys_vm_page(vm_page_t m, vm_offset_t vm_page_v, vm_paddr_t vm_page_p)
{
	return ((vm_page_t)((vm_offset_t)m - vm_page_v + vm_page_p));
}

/* First 2 args are filler for switch_stack() */
static void __aligned(16) __dead2
kexec_reboot_bottom( void *arg1 __unused, void *arg2 __unused,
    struct kexec_image *image)
{
	void (*e)(void) = (void *)image->entry;
	vm_offset_t	vm_page_base = (vm_offset_t)vm_page_array;
	vm_paddr_t	vm_page_phys = pmap_kextract((vm_offset_t)vm_page_array);
	struct kexec_segment_stage *phys_segs =
	    (void *)pmap_kextract((vm_offset_t)&image->segments);
	vm_paddr_t from_pa, to_pa;
	vm_size_t size;
	vm_page_t	first, m, mp;
	struct pctrie_iter pct_i;

	/*
	 * Create a linked list of all pages in the object before we disable the
	 * MMU.  Once the MMU is disabled we can't use the vm_radix iterators,
	 * as they rely on virtual address pointers.
	 */
	first = NULL;
	vm_radix_iter_init(&pct_i, &image->map_obj->rtree);
	VM_RADIX_FORALL(m, &pct_i) {
		if (first == NULL)
			first = m;
		else
			SLIST_INSERT_AFTER(mp, m, plinks.s.ss);
		mp = m;
	}

	/*
	 * We're running out of the identity map now, disable the MMU before we
	 * continue.  It's possible page tables can be overwritten, which would
	 * be very bad if we were running with the MMU enabled.
	 */
	WRITE_SPECIALREG(sctlr_el1, SCTLR_EL1_NO_MMU);
	isb();
	for (int i = 0; i < KEXEC_SEGMENT_MAX; i++) {
		if (phys_segs[i].size == 0)
			break;
		to_pa = phys_segs[i].target;
		/* Copy the segment here... */
		for (vm_page_t p = phys_segs[i].first_page;
		    p != NULL && to_pa - phys_segs[i].target < phys_segs[i].size;
		    p = SLIST_NEXT(p, plinks.s.ss)) {
			p = phys_vm_page(p, vm_page_base, vm_page_phys);
			from_pa = p->phys_addr;
			if (p->phys_addr == to_pa) {
				to_pa += PAGE_SIZE;
				continue;
			}
			for (size = PAGE_SIZE / sizeof(register_t);
			    size > 0; --size) {
				*(register_t *)to_pa = *(register_t *)from_pa;
				to_pa += sizeof(register_t);
				from_pa += sizeof(register_t);
			}
		}
	}
	invalidate_icache();
	e();
	while (1)
		;
}

void
kexec_reboot_md(struct kexec_image *image)
{
	uintptr_t ptr;
	register_t reg;

	for (int i = 0; i < KEXEC_SEGMENT_MAX; i++) {
		if (image->segments[i].size > 0)
			cpu_dcache_inv_range((void *)PHYS_TO_DMAP(image->segments[i].target),
			    image->segments[i].size);
	}
	ptr = pmap_kextract((vm_offset_t)kexec_reboot_bottom);
	serror_disable();

	reg = pmap_kextract((vm_offset_t)pagetable_l0_ttbr0_bootstrap);
	set_ttbr0(reg);
	cpu_tlb_flushID();

	typeof(kexec_reboot_bottom) *p = (void *)ptr;
	switch_stack((void *)pmap_kextract((vm_offset_t)initstack_end),
	    p, image);
	while (1)
		;
}

int
kexec_load_md(struct kexec_image *image)
{
	vm_paddr_t tmp;
	pt_entry_t *pte;

	/* Create L2 page blocks for the trampoline. L0/L1 are from the startup. */

	/*
	 * There are exactly 2 pages before the pagetable_l0_ttbr0_bootstrap, so
	 * move to there.
	 */
	pte = pagetable_l0_ttbr0_bootstrap;
	pte -= (Ln_ENTRIES * 2);	/* move to start of L2 pages */

	/*
	 * Populate the identity map with symbols we know we'll need before we
	 * turn off the MMU.
	 */
	tmp = pmap_kextract((vm_offset_t)kexec_reboot_bottom);
	pte[pmap_l2_index(tmp)] = (tmp | L2_BLOCK | ATTR_AF | ATTR_S1_UXN);
	tmp = pmap_kextract((vm_offset_t)initstack_end);
	pte[pmap_l2_index(tmp)] = (tmp | L2_BLOCK | ATTR_AF | ATTR_S1_UXN);
	/* We'll need vm_page_array for doing offset calculations. */
	tmp = pmap_kextract((vm_offset_t)&vm_page_array);
	pte[pmap_l2_index(tmp)] = (tmp | L2_BLOCK | ATTR_AF | ATTR_S1_UXN);

	return (0);
}
