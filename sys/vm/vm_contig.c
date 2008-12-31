/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 */

/*-
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/vm/vm_contig.c,v 1.63.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_extern.h>

static int
vm_contig_launder_page(vm_page_t m, vm_page_t *next)
{
	vm_object_t object;
	vm_page_t m_tmp;
	struct vnode *vp;
	struct mount *mp;
	int vfslocked;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	object = m->object;
	if (!VM_OBJECT_TRYLOCK(object) &&
	    !vm_pageout_fallback_object_lock(m, next)) {
		VM_OBJECT_UNLOCK(object);
		return (EAGAIN);
	}
	if (vm_page_sleep_if_busy(m, TRUE, "vpctw0")) {
		VM_OBJECT_UNLOCK(object);
		vm_page_lock_queues();
		return (EBUSY);
	}
	vm_page_test_dirty(m);
	if (m->dirty == 0 && m->hold_count == 0)
		pmap_remove_all(m);
	if (m->dirty) {
		if ((object->flags & OBJ_DEAD) != 0) {
			VM_OBJECT_UNLOCK(object);
			return (EAGAIN);
		}
		if (object->type == OBJT_VNODE) {
			vm_page_unlock_queues();
			vp = object->handle;
			vm_object_reference_locked(object);
			VM_OBJECT_UNLOCK(object);
			(void) vn_start_write(vp, &mp, V_WAIT);
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
			VM_OBJECT_LOCK(object);
			vm_object_page_clean(object, 0, 0, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(object);
			VOP_UNLOCK(vp, 0, curthread);
			VFS_UNLOCK_GIANT(vfslocked);
			vm_object_deallocate(object);
			vn_finished_write(mp);
			vm_page_lock_queues();
			return (0);
		} else if (object->type == OBJT_SWAP ||
			   object->type == OBJT_DEFAULT) {
			m_tmp = m;
			vm_pageout_flush(&m_tmp, 1, VM_PAGER_PUT_SYNC);
			VM_OBJECT_UNLOCK(object);
			return (0);
		}
	} else if (m->hold_count == 0)
		vm_page_cache(m);
	VM_OBJECT_UNLOCK(object);
	return (0);
}

static int
vm_contig_launder(int queue)
{
	vm_page_t m, next;
	int error;

	TAILQ_FOREACH_SAFE(m, &vm_page_queues[queue].pl, pageq, next) {

		/* Skip marker pages */
		if ((m->flags & PG_MARKER) != 0)
			continue;

		KASSERT(VM_PAGE_INQUEUE2(m, queue),
		    ("vm_contig_launder: page %p's queue is not %d", m, queue));
		error = vm_contig_launder_page(m, &next);
		if (error == 0)
			return (TRUE);
		if (error == EBUSY)
			return (FALSE);
	}
	return (FALSE);
}

/*
 *	Frees the given physically contiguous pages.
 *
 *	N.B.: Any pages with PG_ZERO set must, in fact, be zero filled.
 */
static void
vm_page_release_contig(vm_page_t m, vm_pindex_t count)
{

	while (count--) {
		/* Leave PG_ZERO unchanged. */
		vm_page_free_toq(m);
		m++;
	}
}

/*
 *	Allocates a region from the kernel address map, inserts the
 *	given physically contiguous pages into the kernel object,
 *	creates a wired mapping from the region to the pages, and
 *	returns the region's starting virtual address.  If M_ZERO is
 *	specified through the given flags, then the pages are zeroed
 *	before they are mapped.
 */
static void *
contigmapping(vm_page_t m, vm_pindex_t npages, int flags)
{
	vm_object_t object = kernel_object;
	vm_map_t map = kernel_map;
	vm_offset_t addr, tmp_addr;
	vm_pindex_t i;
 
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), npages << PAGE_SHIFT, &addr)
	    != KERN_SUCCESS) {
		vm_map_unlock(map);
		return (NULL);
	}
	vm_object_reference(object);
	vm_map_insert(map, object, addr - VM_MIN_KERNEL_ADDRESS,
	    addr, addr + (npages << PAGE_SHIFT), VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);
	tmp_addr = addr;
	VM_OBJECT_LOCK(object);
	for (i = 0; i < npages; i++) {
		vm_page_insert(&m[i], object,
		    OFF_TO_IDX(tmp_addr - VM_MIN_KERNEL_ADDRESS));
		if ((flags & M_ZERO) && !(m[i].flags & PG_ZERO))
			pmap_zero_page(&m[i]);
		tmp_addr += PAGE_SIZE;
	}
	VM_OBJECT_UNLOCK(object);
	vm_map_wire(map, addr, addr + (npages << PAGE_SHIFT),
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	return ((void *)addr);
}

void *
contigmalloc(
	unsigned long size,	/* should be size_t here and for malloc() */
	struct malloc_type *type,
	int flags,
	vm_paddr_t low,
	vm_paddr_t high,
	unsigned long alignment,
	unsigned long boundary)
{
	void *ret;
	vm_page_t pages;
	unsigned long npgs;
	int actl, actmax, inactl, inactmax, tries;

	npgs = round_page(size) >> PAGE_SHIFT;
	tries = 0;
retry:
	pages = vm_phys_alloc_contig(npgs, low, high, alignment, boundary);
	if (pages == NULL) {
		if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
			vm_page_lock_queues();
			inactl = 0;
			inactmax = tries < 1 ? 0 : cnt.v_inactive_count;
			actl = 0;
			actmax = tries < 2 ? 0 : cnt.v_active_count;
again:
			if (inactl < inactmax &&
			    vm_contig_launder(PQ_INACTIVE)) {
				inactl++;
				goto again;
			}
			if (actl < actmax &&
			    vm_contig_launder(PQ_ACTIVE)) {
				actl++;
				goto again;
			}
			vm_page_unlock_queues();
			tries++;
			goto retry;
		}
		ret = NULL;
	} else {
		ret = contigmapping(pages, npgs, flags);
		if (ret == NULL)
			vm_page_release_contig(pages, npgs);
		else
			malloc_type_allocated(type, npgs << PAGE_SHIFT);
	}
	return (ret);
}

void
contigfree(void *addr, unsigned long size, struct malloc_type *type)
{
	vm_pindex_t npgs;

	npgs = round_page(size) >> PAGE_SHIFT;
	kmem_free(kernel_map, (vm_offset_t)addr, size);
	malloc_type_freed(type, npgs << PAGE_SHIFT);
}
