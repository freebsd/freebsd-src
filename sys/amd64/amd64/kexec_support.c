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

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kexec.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_phys.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_radix.h>

#include <machine/intr_machdep.h>
#include <machine/kexec.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <x86/apicvar.h>

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

/*
 * do_pte: Create PTE entries (4k pages). If false, create 2MB superpages.
 * identity: This is for an identity map, treat `start` as a physical address.
 * Only valid here if do_pte is false.
 */
static void
kexec_generate_page_tables(pml4_entry_t *root, vm_offset_t start,
    vm_size_t size, bool do_pte, bool identity, struct pctrie_iter *pages)
{
	vm_paddr_t mpa;
	vm_offset_t pg;
	vm_size_t stride = do_pte ? PAGE_SIZE : NBPDR;
	vm_page_t m;
	vm_pindex_t i, j, k, l;

	pg = start & ~(stride - 1);
	i = pmap_pml4e_index(pg);
	j = pmap_pdpe_index(pg);
	k = pmap_pde_index(pg);
	l = pmap_pte_index(pg);
	for (; pg < start + size; i++, j = 0, k = 0, l = 0) {
		/*
		 * Walk linearly, as above, but one fell swoop, one page at a
		 * time.
		 */
		if (root[i] == 0) {
			m = vm_radix_iter_next(pages);
			mpa = VM_PAGE_TO_PHYS(m);
			root[i] = mpa | PG_RW | PG_V;
		}
		pdp_entry_t *pdp =
			(pdp_entry_t *)(PHYS_TO_DMAP(root[i] & PG_FRAME));
		for (; j < NPDPEPG && pg < start + size; j++, k = 0, l = 0) {
			if (pdp[j] == 0) {
				m = vm_radix_iter_next(pages);
				mpa = VM_PAGE_TO_PHYS(m);
				pdp[j] = mpa | PG_RW | PG_V;
			}
			pd_entry_t *pde =
			    (pd_entry_t *)(PHYS_TO_DMAP(pdp[j] & PG_FRAME));
			for (; k < NPDEPG && pg < start + size; k++, l = 0) {
				if (pde[k] == 0) {
					if (!do_pte) {
						pde[k] =
						    (identity ? pg : pmap_kextract(pg)) |
						    PG_RW | PG_PS | PG_V;
						pg += NBPDR;
						continue;
					}
					m = vm_radix_iter_next(pages);
					mpa = VM_PAGE_TO_PHYS(m);
					pde[k] = mpa | PG_V | PG_RW;
				} else if ((pde[k] & PG_PS) != 0) {
					pg += NBPDR;
					continue;
				}
				/* Populate the PTEs. */
				for (; l < NPTEPG && pg < start + size;
				    l++, pg += PAGE_SIZE) {
					pt_entry_t *pte =
					    (pt_entry_t *)PHYS_TO_DMAP(pde[pmap_pde_index(pg)] & PG_FRAME);
					pte[pmap_pte_index(pg)] =
					    pmap_kextract(pg) | PG_RW | PG_V;
				}
			}
		}
	}
}

void
kexec_reboot_md(struct kexec_image *image)
{
	void (*kexec_do_tramp)(void) = image->md_image;

	intr_disable_all();
	lapic_disable();
	kexec_do_reboot_trampoline(VM_PAGE_TO_PHYS(image->first_md_page),
	    kexec_do_tramp);

	for (;;)
		;
}

int
kexec_load_md(struct kexec_image *image)
{
	struct pctrie_iter pct_iter;
	pml4_entry_t *PT4;
	pdp_entry_t *PDP_l;
	pd_entry_t *PD_l0;
	vm_offset_t va;
	int i;

	/*
	 * Start building the page table.
	 * First part of the page table is standard for all.
	 */
	vm_offset_t pa_pdp_l, pa_pd_l0, pa_pd_l1, pa_pd_l2, pa_pd_l3;
	vm_page_t m;

	if (la57)
		return (EINVAL);

	vm_radix_iter_init(&pct_iter, &image->map_obj->rtree);
	/* Working in linear space in the mapped space, `va` is our tracker. */
	m = vm_radix_iter_lookup(&pct_iter, image->first_md_page->pindex);
	va = (vm_offset_t)image->map_addr + ptoa(m->pindex);
	/* We'll find a place for these later */
	PT4 = (void *)va;
	va += PAGE_SIZE;
	m = vm_radix_iter_next(&pct_iter);
	pa_pdp_l = VM_PAGE_TO_PHYS(m);
	PDP_l = (void *)va;
	va += PAGE_SIZE;
	m = vm_radix_iter_next(&pct_iter);
	pa_pd_l0 = VM_PAGE_TO_PHYS(m);
	PD_l0 = (void *)va;
	va += PAGE_SIZE;
	m = vm_radix_iter_next(&pct_iter);
	pa_pd_l1 = VM_PAGE_TO_PHYS(m);
	m = vm_radix_iter_next(&pct_iter);
	pa_pd_l2 = VM_PAGE_TO_PHYS(m);
	m = vm_radix_iter_next(&pct_iter);
	pa_pd_l3 = VM_PAGE_TO_PHYS(m);
	m = vm_radix_iter_next(&pct_iter);

	/* 1:1 mapping of lower 4G */
	PT4[0] = (pml4_entry_t)pa_pdp_l | PG_V | PG_RW;
	PDP_l[0] = (pdp_entry_t)pa_pd_l0 | PG_V | PG_RW;
	PDP_l[1] = (pdp_entry_t)pa_pd_l1 | PG_V | PG_RW;
	PDP_l[2] = (pdp_entry_t)pa_pd_l2 | PG_V | PG_RW;
	PDP_l[3] = (pdp_entry_t)pa_pd_l3 | PG_V | PG_RW;
	for (i = 0; i < 4 * NPDEPG; i++) {	/* we overflow PD_l0 into _l1, etc */
		PD_l0[i] = ((pd_entry_t)i << PDRSHIFT) | PG_V |
		    PG_RW | PG_PS;
	}

	/* Map the target(s) in 2MB chunks. */
	for (i = 0; i < KEXEC_SEGMENT_MAX; i++) {
		struct kexec_segment_stage *s = &image->segments[i];

		if (s->size == 0)
			break;
		kexec_generate_page_tables(PT4, s->target, s->size, false,
		    true, &pct_iter);
	}
	/* Now create the source page tables */
	kexec_generate_page_tables(PT4, image->map_addr, image->map_size, true,
	    false, &pct_iter);
	kexec_generate_page_tables(PT4,
	    trunc_page((vm_offset_t)kexec_do_reboot_trampoline),
	    PAGE_SIZE, true, false, &pct_iter);
	KASSERT(m != NULL, ("kexec_load_md: Missing trampoline page!\n"));

	/* MD control pages start at this next page. */
	image->md_image = (void *)(image->map_addr + ptoa(m->pindex));
	bcopy(kexec_do_reboot, image->md_image, kexec_do_reboot_size);

	/* Save the image into the MD page(s) right after the trampoline */
	bcopy(image, (void *)((vm_offset_t)image->md_image +
	    (vm_offset_t)&kexec_saved_image - (vm_offset_t)&kexec_do_reboot),
	    sizeof(*image));

	return (0);
}

/*
 * Required pages:
 * - L4 (1) (root)
 * - L3 (PDPE) - 2 (bottom 512GB, bottom 4 used, top range for kernel map)
 * - L2 (PDP) - 5 (2MB superpage mappings, 1GB each, for bottom 4GB, top 1)
 * - L1 (PDR) - 1 (kexec trampoline page, first MD page)
 * - kexec_do_reboot trampoline - 1
 * - Slop pages for staging (in case it's not aligned nicely) - 3 (worst case)
 *
 * Minimum 9 pages for the direct map.
 */
int
kexec_md_pages(struct kexec_segment *seg_in)
{
	struct kexec_segment *segs = seg_in;
	vm_size_t pages = 13;	/* Minimum number of starting pages */
	vm_paddr_t cur_addr = (1UL << 32) - 1;	/* Bottom 4G will be identity mapped in full */
	vm_size_t source_total = 0;

	for (int i = 0; i < KEXEC_SEGMENT_MAX; i++) {
		vm_offset_t start, end;
		if (segs[i].memsz == 0)
			break;

		end = round_2mpage((vm_offset_t)segs[i].mem + segs[i].memsz);
		start = trunc_2mpage((vm_offset_t)segs[i].mem);
		start = max(start, cur_addr + 1);
		/*
		 * Round to cover the full range of page table pages for each
		 * segment.
		 */
		source_total += round_2mpage(end - start);

		/*
		 * Bottom 4GB are identity mapped already in the count, so skip
		 * any segments that end up there, this will short-circuit that.
		 */
		if (end <= cur_addr + 1)
			continue;

		if (pmap_pml4e_index(end) != pmap_pml4e_index(cur_addr)) {
			/* Need a new 512GB mapping page */
			pages++;
			pages += howmany(end - (start & ~PML4MASK), NBPML4);
			pages += howmany(end - (start & ~PDPMASK), NBPDP);
			pages += howmany(end - (start & ~PDRMASK), NBPDR);

		} else if (pmap_pdpe_index(end) != pmap_pdpe_index(cur_addr)) {
			pages++;
			pages += howmany(end - (start & ~PDPMASK), NBPDP) - 1;
			pages += howmany(end - (start & ~PDRMASK), NBPDR);
		}

	}
	/* Be pessimistic when totaling up source pages.  We likely
	 * can't use superpages, so need to map each page individually.
	 */
	pages += howmany(source_total, NBPDR);
	pages += howmany(source_total, NBPDP);
	pages += howmany(source_total, NBPML4);

	/*
	 * Be intentionally sloppy adding in the extra page table pages. It's
	 * better to go over than under.
	 */
	pages += howmany(pages * PAGE_SIZE, NBPDR);
	pages += howmany(pages * PAGE_SIZE, NBPDP);
	pages += howmany(pages * PAGE_SIZE, NBPML4);

	/* Add in the trampoline pages */
	pages += howmany(kexec_do_reboot_size, PAGE_SIZE);

	return (pages);
}
