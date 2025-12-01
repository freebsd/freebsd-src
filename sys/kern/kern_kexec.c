/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Juniper Networks, Inc.
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
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#ifdef INTRNG
#include <sys/intr.h>
#endif
#include <sys/kexec.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/priv.h>
#include <sys/reboot.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>

#include <machine/kexec.h>

#ifndef	KEXEC_MD_PAGES
/*
 * Number of MD pages for extra bookkeeping.
 * This is a macro because it can be a constant (some architectures make it 0).
 * It accepts an argument, which is an array of
 * kexec_segment[KEXEC_SEGMENT_MAX].
 */
#define	KEXEC_MD_PAGES(x)	0
#endif

/*
 * Basic design:
 *
 * Given an array of "segment descriptors" stage an image to be loaded and
 * jumped to at reboot, instead of rebooting via firmware.
 *
 * Constraints:
 * - The segment descriptors' "mem" and "memsz" must each fit within a
 *   vm_phys_seg segment, which can be obtained via the `vm.phys_segs` sysctl.
 *   A single segment cannot span multiple vm_phys_seg segments, even if the
 *   vm_phys_seg segments are adjacent.
 *
 * Technical details:
 *
 * Take advantage of the VM subsystem and create a vm_object to hold the staged
 * image.  When grabbing pages for the object, sort the pages so that if a page
 * in the object is located in the physical range of any of the kexec segment
 * targets then it gets placed at the pindex corresponding to that physical
 * address.  This avoids the chance of corruption by writing over the page in
 * the final copy, or the need for a copy buffer page.
 */

static struct kexec_image staged_image;
static vm_offset_t stage_addr;
static vm_object_t kexec_obj;

static eventhandler_tag kexec_reboot_handler;
static struct mtx kexec_mutex;

static MALLOC_DEFINE(M_KEXEC, "kexec", "Kexec segments");


static void
kexec_reboot(void *junk __unused, int howto)
{
	if ((howto & RB_KEXEC) == 0 || kexec_obj == NULL)
		return;

#ifdef SMP
	cpu_mp_stop();
#endif /* SMP */
	intr_disable();
	printf("Starting kexec reboot\n");

	scheduler_stopped = true;
	kexec_reboot_md(&staged_image);
}

MTX_SYSINIT(kexec_mutex, &kexec_mutex, "kexec", MTX_DEF);

/* Sort the segment list once copied in */
static int
seg_cmp(const void *seg1, const void *seg2)
{
	const struct kexec_segment *s1, *s2;

	s1 = seg1;
	s2 = seg2;

	return ((uintptr_t)s1->mem - (uintptr_t)s2->mem);
}

static bool
segment_fits(struct kexec_segment *seg)
{
	vm_paddr_t v = (vm_paddr_t)(uintptr_t)seg->mem;

	for (int i = 0; i < vm_phys_nsegs; i++) {
		if (v >= vm_phys_segs[i].start &&
		    (v + seg->memsz - 1) <= vm_phys_segs[i].end)
			return (true);
	}

	return (false);
}

static vm_paddr_t
pa_for_pindex(struct kexec_segment_stage *segs, int count, vm_pindex_t pind)
{
	for (int i = count; i > 0; --i) {
		if (pind >= segs[i - 1].pindex)
			return (ptoa(pind - segs[i-1].pindex) + segs[i - 1].target);
	}

	panic("No segment for pindex %ju\n", (uintmax_t)pind);
}

/*
 * For now still tied to the system call, so assumes all memory is userspace.
 */
int
kern_kexec_load(struct thread *td, u_long entry, u_long nseg,
    struct kexec_segment *seg, u_long flags)
{
	static int kexec_loading;
	struct kexec_segment segtmp[KEXEC_SEGMENT_MAX];
	struct kexec_image *new_image_stage = 0;
	vm_object_t new_segments = NULL;
	uint8_t *buf;
	int err = 0;
	int i;
	const size_t segsize = nseg * sizeof(struct kexec_segment);
	vm_page_t *page_list = 0;
	vm_size_t image_count, md_pages, page_count, tmpsize;
	vm_offset_t segment_va = 0;
	/*
	 * - Do any sanity checking
	 * - Load the new segments to temporary
	 * - Remove the old segments
	 * - Install the new segments
	 */

	if (nseg > KEXEC_SEGMENT_MAX)
		return (EINVAL);

	if (atomic_cmpset_acq_int(&kexec_loading, false, true) == 0)
		return (EBUSY);

	/* Only do error checking if we're installing new segments. */
	if (nseg > 0) {
		/* Create the new kexec object before destroying the old one. */
		bzero(&segtmp, sizeof(segtmp));
		err = copyin(seg, segtmp, segsize);
		if (err != 0)
			goto out;
		qsort(segtmp, nseg, sizeof(*segtmp), seg_cmp);
		new_image_stage = malloc(sizeof(*new_image_stage), M_TEMP, M_WAITOK | M_ZERO);
		/*
		 * Sanity checking:
		 * - All segments must not overlap the kernel, so must be fully enclosed
		 *   in a vm_phys_seg (each kexec segment must be in a single
		 *   vm_phys_seg segment, cannot cross even adjacent segments).
		 */
		image_count = 0;
		for (i = 0; i < nseg; i++) {
			if (!segment_fits(&segtmp[i]) ||
			    segtmp[i].bufsz > segtmp[i].memsz) {
				err = EINVAL;
				goto out;
			}
			new_image_stage->segments[i].pindex = image_count;
			new_image_stage->segments[i].target = (vm_offset_t)segtmp[i].mem;
			new_image_stage->segments[i].size = segtmp[i].memsz;
			image_count += atop(segtmp[i].memsz);
		}
		md_pages = KEXEC_MD_PAGES(segtmp);
		page_count = image_count + md_pages;
		new_segments = vm_object_allocate(OBJT_PHYS, page_count);
		page_list = malloc(page_count * sizeof(vm_page_t), M_TEMP, M_WAITOK);

		/*
		 * - Grab all pages for all segments (use pindex to slice it)
		 * - Walk the list (once)
		 *   - At each pindex, check if the target PA that corresponds
		 *     to that index is in the object.  If so, swap the pages.
		 *   - At the end of this the list will be "best" sorted.
		 */
		vm_page_grab_pages_unlocked(new_segments, 0,
		    VM_ALLOC_NORMAL | VM_ALLOC_WAITOK | VM_ALLOC_WIRED | VM_ALLOC_NOBUSY | VM_ALLOC_ZERO,
		    page_list, page_count);

		/* Sort the pages to best match the PA */
		VM_OBJECT_WLOCK(new_segments);
		for (i = 0; i < image_count; i++) {
			vm_page_t curpg, otherpg, tmp;
			vm_pindex_t otheridx;

			curpg = page_list[i];
			otherpg = PHYS_TO_VM_PAGE(pa_for_pindex(new_image_stage->segments,
			    nseg, curpg->pindex));
			otheridx = otherpg->pindex;

			if (otherpg->object == new_segments) {
				/*
				 * Swap 'curpg' and 'otherpg', since 'otherpg'
				 * is at the PA 'curpg' covers.
				 */
				vm_radix_remove(&new_segments->rtree, otheridx);
				vm_radix_remove(&new_segments->rtree, i);
				otherpg->pindex = i;
				curpg->pindex = otheridx;
				vm_radix_insert(&new_segments->rtree, curpg);
				vm_radix_insert(&new_segments->rtree, otherpg);
				tmp = curpg;
				page_list[i] = otherpg;
				page_list[otheridx] = tmp;
			}
		}
		for (i = 0; i < nseg; i++) {
			new_image_stage->segments[i].first_page =
			    vm_radix_lookup(&new_segments->rtree,
			    new_image_stage->segments[i].pindex);
		}
		if (md_pages > 0)
			new_image_stage->first_md_page =
			    vm_radix_lookup(&new_segments->rtree,
			    page_count - md_pages);
		else
			new_image_stage->first_md_page = NULL;
		VM_OBJECT_WUNLOCK(new_segments);

		/* Map the object to do the copies */
		err = vm_map_find(kernel_map, new_segments, 0, &segment_va,
		    ptoa(page_count), 0, VMFS_ANY_SPACE,
		    VM_PROT_RW, VM_PROT_RW, MAP_PREFAULT);
		if (err != 0)
			goto out;
		buf = (void *)segment_va;
		new_image_stage->map_addr = segment_va;
		new_image_stage->map_size = ptoa(new_segments->size);
		new_image_stage->entry = entry;
		new_image_stage->map_obj = new_segments;
		for (i = 0; i < nseg; i++) {
			err = copyin(segtmp[i].buf, buf, segtmp[i].bufsz);
			if (err != 0) {
				goto out;
			}
			new_image_stage->segments[i].map_buf = buf;
			buf += segtmp[i].bufsz;
			tmpsize = segtmp[i].memsz - segtmp[i].bufsz;
			if (tmpsize > 0)
				memset(buf, 0, tmpsize);
			buf += tmpsize;
		}
		/* What's left are the MD pages, so zero them all out. */
		if (md_pages > 0)
			bzero(buf, ptoa(md_pages));

		cpu_flush_dcache((void *)segment_va, ptoa(page_count));
		if ((err = kexec_load_md(new_image_stage)) != 0)
			goto out;
	}
	if (kexec_obj != NULL) {
		vm_object_unwire(kexec_obj, 0, kexec_obj->size, 0);
		KASSERT(stage_addr != 0, ("Mapped kexec_obj without address"));
		vm_map_remove(kernel_map, stage_addr, stage_addr + kexec_obj->size);
	}
	kexec_obj = new_segments;
	bzero(&staged_image, sizeof(staged_image));
	if (nseg > 0)
		memcpy(&staged_image, new_image_stage, sizeof(*new_image_stage));

	printf("trampoline at %#jx\n", (uintmax_t)staged_image.entry);
	if (nseg > 0) {
		if (kexec_reboot_handler == NULL)
			kexec_reboot_handler =
			    EVENTHANDLER_REGISTER(shutdown_final, kexec_reboot, NULL,
			    SHUTDOWN_PRI_LAST - 150);
	} else {
		if (kexec_reboot_handler != NULL)
			EVENTHANDLER_DEREGISTER(shutdown_final, kexec_reboot_handler);
	}
out:
	/* Clean up the mess if we've gotten far. */
	if (err != 0 && new_segments != NULL) {
		vm_object_unwire(new_segments, 0, new_segments->size, 0);
		if (segment_va != 0)
			vm_map_remove(kernel_map, segment_va, segment_va + kexec_obj->size);
		else
			vm_object_deallocate(new_segments);
	}
	atomic_store_rel_int(&kexec_loading, false);
	if (new_image_stage != NULL)
		free(new_image_stage, M_TEMP);
	if (page_list != 0)
		free(page_list, M_TEMP);

	return (err);
}

int
sys_kexec_load(struct thread *td, struct kexec_load_args *uap)
{
	int error;

	// FIXME: Do w need a better privilege check than PRIV_REBOOT here?
	error = priv_check(td, PRIV_REBOOT);
	if (error != 0)
		return (error);
	return (kern_kexec_load(td, uap->entry, uap->nseg, uap->segments, uap->flags));
}
