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
__FBSDID("$FreeBSD$");

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
	vm_page_lock_assert(m, MA_OWNED);
	object = m->object;
	if (!VM_OBJECT_TRYLOCK(object) &&
	    !vm_pageout_fallback_object_lock(m, next)) {
		vm_page_unlock(m);
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
	if (m->dirty != 0) {
		vm_page_unlock(m);
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
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			VM_OBJECT_LOCK(object);
			vm_object_page_clean(object, 0, 0, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(object);
			VOP_UNLOCK(vp, 0);
			VFS_UNLOCK_GIANT(vfslocked);
			vm_object_deallocate(object);
			vn_finished_write(mp);
			vm_page_lock_queues();
			return (0);
		} else if (object->type == OBJT_SWAP ||
			   object->type == OBJT_DEFAULT) {
			vm_page_unlock_queues();
			m_tmp = m;
			vm_pageout_flush(&m_tmp, 1, VM_PAGER_PUT_SYNC, 0, NULL);
			VM_OBJECT_UNLOCK(object);
			vm_page_lock_queues();
			return (0);
		}
	} else {
		if (m->hold_count == 0)
			vm_page_cache(m);
		vm_page_unlock(m);
	}
	VM_OBJECT_UNLOCK(object);
	return (0);
}

static int
vm_contig_launder(int queue, vm_paddr_t low, vm_paddr_t high)
{
	vm_page_t m, next;
	vm_paddr_t pa;
	int error;

	TAILQ_FOREACH_SAFE(m, &vm_page_queues[queue].pl, pageq, next) {

		/* Skip marker pages */
		if ((m->flags & PG_MARKER) != 0)
			continue;

		pa = VM_PAGE_TO_PHYS(m);
		if (pa < low || pa + PAGE_SIZE > high)
			continue;

		if (!vm_pageout_page_lock(m, &next)) {
			vm_page_unlock(m);
			continue;
		}
		KASSERT(m->queue == queue,
		    ("vm_contig_launder: page %p's queue is not %d", m, queue));
		error = vm_contig_launder_page(m, &next);
		vm_page_lock_assert(m, MA_NOTOWNED);
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
 * Increase the number of cached pages.
 */
void
vm_contig_grow_cache(int tries, vm_paddr_t low, vm_paddr_t high)
{
	int actl, actmax, inactl, inactmax;

	vm_page_lock_queues();
	inactl = 0;
	inactmax = tries < 1 ? 0 : cnt.v_inactive_count;
	actl = 0;
	actmax = tries < 2 ? 0 : cnt.v_active_count;
again:
	if (inactl < inactmax && vm_contig_launder(PQ_INACTIVE, low, high)) {
		inactl++;
		goto again;
	}
	if (actl < actmax && vm_contig_launder(PQ_ACTIVE, low, high)) {
		actl++;
		goto again;
	}
	vm_page_unlock_queues();
}

/*
 * Allocates a region from the kernel address map and pages within the
 * specified physical address range to the kernel object, creates a wired
 * mapping from the region to these pages, and returns the region's starting
 * virtual address.  The allocated pages are not necessarily physically
 * contiguous.  If M_ZERO is specified through the given flags, then the pages
 * are zeroed before they are mapped.
 */
vm_offset_t
kmem_alloc_attr(vm_map_t map, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, vm_memattr_t memattr)
{
	vm_object_t object = kernel_object;
	vm_offset_t addr, i, offset;
	vm_page_t m;
	int tries;

	size = round_page(size);
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), size, &addr)) {
		vm_map_unlock(map);
		return (0);
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(object);
	vm_map_insert(map, object, offset, addr, addr + size, VM_PROT_ALL,
	    VM_PROT_ALL, 0);
	VM_OBJECT_LOCK(object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		tries = 0;
retry:
		m = vm_phys_alloc_contig(1, low, high, PAGE_SIZE, 0);
		if (m == NULL) {
			if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
				VM_OBJECT_UNLOCK(object);
				vm_map_unlock(map);
				vm_contig_grow_cache(tries, low, high);
				vm_map_lock(map);
				VM_OBJECT_LOCK(object);
				goto retry;
			}
			while (i != 0) {
				i -= PAGE_SIZE;
				m = vm_page_lookup(object, OFF_TO_IDX(offset +
				    i));
				vm_page_free(m);
			}
			VM_OBJECT_UNLOCK(object);
			vm_map_delete(map, addr, addr + size);
			vm_map_unlock(map);
			return (0);
		}
		if (memattr != VM_MEMATTR_DEFAULT)
			pmap_page_set_memattr(m, memattr);
		vm_page_insert(m, object, OFF_TO_IDX(offset + i));
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
	}
	VM_OBJECT_UNLOCK(object);
	vm_map_unlock(map);
	vm_map_wire(map, addr, addr + size, VM_MAP_WIRE_SYSTEM |
	    VM_MAP_WIRE_NOHOLES);
	return (addr);
}

/*
 *	Allocates a region from the kernel address map, inserts the
 *	given physically contiguous pages into the kernel object,
 *	creates a wired mapping from the region to the pages, and
 *	returns the region's starting virtual address.  If M_ZERO is
 *	specified through the given flags, then the pages are zeroed
 *	before they are mapped.
 */
static vm_offset_t
contigmapping(vm_map_t map, vm_size_t size, vm_page_t m, vm_memattr_t memattr,
    int flags)
{
	vm_object_t object = kernel_object;
	vm_offset_t addr, tmp_addr;
 
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), size, &addr)) {
		vm_map_unlock(map);
		return (0);
	}
	vm_object_reference(object);
	vm_map_insert(map, object, addr - VM_MIN_KERNEL_ADDRESS,
	    addr, addr + size, VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);
	VM_OBJECT_LOCK(object);
	for (tmp_addr = addr; tmp_addr < addr + size; tmp_addr += PAGE_SIZE) {
		if (memattr != VM_MEMATTR_DEFAULT)
			pmap_page_set_memattr(m, memattr);
		vm_page_insert(m, object,
		    OFF_TO_IDX(tmp_addr - VM_MIN_KERNEL_ADDRESS));
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
		m++;
	}
	VM_OBJECT_UNLOCK(object);
	vm_map_wire(map, addr, addr + size,
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	return (addr);
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

	ret = (void *)kmem_alloc_contig(kernel_map, size, flags, low, high,
	    alignment, boundary, VM_MEMATTR_DEFAULT);
	if (ret != NULL)
		malloc_type_allocated(type, round_page(size));
	return (ret);
}

vm_offset_t
kmem_alloc_contig(vm_map_t map, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, unsigned long alignment, unsigned long boundary,
    vm_memattr_t memattr)
{
	vm_offset_t ret;
	vm_page_t pages;
	unsigned long npgs;
	int tries;

	size = round_page(size);
	npgs = size >> PAGE_SHIFT;
	tries = 0;
retry:
	pages = vm_phys_alloc_contig(npgs, low, high, alignment, boundary);
	if (pages == NULL) {
		if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
			vm_contig_grow_cache(tries, low, high);
			tries++;
			goto retry;
		}
		ret = 0;
	} else {
		ret = contigmapping(map, size, pages, memattr, flags);
		if (ret == 0)
			vm_page_release_contig(pages, npgs);
	}
	return (ret);
}

void
contigfree(void *addr, unsigned long size, struct malloc_type *type)
{

	kmem_free(kernel_map, (vm_offset_t)addr, size);
	malloc_type_freed(type, round_page(size));
}
