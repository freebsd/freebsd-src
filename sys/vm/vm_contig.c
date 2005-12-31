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
#include <vm/vm_extern.h>

static int
vm_contig_launder_page(vm_page_t m)
{
	vm_object_t object;
	vm_page_t m_tmp;
	struct vnode *vp;

	object = m->object;
	if (!VM_OBJECT_TRYLOCK(object))
		return (EAGAIN);
	if (vm_page_sleep_if_busy(m, TRUE, "vpctw0")) {
		VM_OBJECT_UNLOCK(object);
		vm_page_lock_queues();
		return (EBUSY);
	}
	vm_page_test_dirty(m);
	if (m->dirty == 0 && m->hold_count == 0)
		pmap_remove_all(m);
	if (m->dirty) {
		if (object->type == OBJT_VNODE) {
			vm_page_unlock_queues();
			vp = object->handle;
			VM_OBJECT_UNLOCK(object);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
			VM_OBJECT_LOCK(object);
			vm_object_page_clean(object, 0, 0, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(object);
			VOP_UNLOCK(vp, 0, curthread);
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

	for (m = TAILQ_FIRST(&vm_page_queues[queue].pl); m != NULL; m = next) {
		next = TAILQ_NEXT(m, pageq);

		/* Skip marker pages */
		if ((m->flags & PG_MARKER) != 0)
			continue;

		KASSERT(VM_PAGE_INQUEUE2(m, queue),
		    ("vm_contig_launder: page %p's queue is not %d", m, queue));
		error = vm_contig_launder_page(m);
		if (error == 0)
			return (TRUE);
		if (error == EBUSY)
			return (FALSE);
	}
	return (FALSE);
}

/*
 * This interface is for merging with malloc() someday.
 * Even if we never implement compaction so that contiguous allocation
 * works after initialization time, malloc()'s data structures are good
 * for statistics and for allocations of less than a page.
 */
static void *
contigmalloc1(
	unsigned long size,	/* should be size_t here and for malloc() */
	struct malloc_type *type,
	int flags,
	vm_paddr_t low,
	vm_paddr_t high,
	unsigned long alignment,
	unsigned long boundary,
	vm_map_t map)
{
	int i, start;
	vm_paddr_t phys;
	vm_object_t object;
	vm_offset_t addr, tmp_addr;
	int pass, pqtype;
	int inactl, actl, inactmax, actmax;
	vm_page_t pga = vm_page_array;

	size = round_page(size);
	if (size == 0)
		panic("contigmalloc1: size must not be 0");
	if ((alignment & (alignment - 1)) != 0)
		panic("contigmalloc1: alignment must be a power of 2");
	if ((boundary & (boundary - 1)) != 0)
		panic("contigmalloc1: boundary must be a power of 2");

	start = 0;
	for (pass = 2; pass >= 0; pass--) {
		vm_page_lock_queues();
again0:
		mtx_lock_spin(&vm_page_queue_free_mtx);
again:
		/*
		 * Find first page in array that is free, within range,
		 * aligned, and such that the boundary won't be crossed.
		 */
		for (i = start; i < cnt.v_page_count; i++) {
			phys = VM_PAGE_TO_PHYS(&pga[i]);
			pqtype = pga[i].queue - pga[i].pc;
			if (((pqtype == PQ_FREE) || (pqtype == PQ_CACHE)) &&
			    (phys >= low) && (phys < high) &&
			    ((phys & (alignment - 1)) == 0) &&
			    (((phys ^ (phys + size - 1)) & ~(boundary - 1)) == 0))
				break;
		}

		/*
		 * If the above failed or we will exceed the upper bound, fail.
		 */
		if ((i == cnt.v_page_count) ||
			((VM_PAGE_TO_PHYS(&pga[i]) + size) > high)) {
			mtx_unlock_spin(&vm_page_queue_free_mtx);
			/*
			 * Instead of racing to empty the inactive/active
			 * queues, give up, even with more left to free,
			 * if we try more than the initial amount of pages.
			 *
			 * There's no point attempting this on the last pass.
			 */
			if (pass > 0) {
				inactl = actl = 0;
				inactmax = vm_page_queues[PQ_INACTIVE].lcnt;
				actmax = vm_page_queues[PQ_ACTIVE].lcnt;
again1:
				if (inactl < inactmax &&
				    vm_contig_launder(PQ_INACTIVE)) {
					inactl++;
					goto again1;
				}
				if (actl < actmax &&
				    vm_contig_launder(PQ_ACTIVE)) {
					actl++;
					goto again1;
				}
			}
			vm_page_unlock_queues();
			continue;
		}
		start = i;

		/*
		 * Check successive pages for contiguous and free.
		 */
		for (i = start + 1; i < (start + size / PAGE_SIZE); i++) {
			pqtype = pga[i].queue - pga[i].pc;
			if ((VM_PAGE_TO_PHYS(&pga[i]) !=
			    (VM_PAGE_TO_PHYS(&pga[i - 1]) + PAGE_SIZE)) ||
			    ((pqtype != PQ_FREE) && (pqtype != PQ_CACHE))) {
				start++;
				goto again;
			}
		}
		mtx_unlock_spin(&vm_page_queue_free_mtx);
		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			vm_page_t m = &pga[i];

			if (VM_PAGE_INQUEUE1(m, PQ_CACHE)) {
				if (m->hold_count != 0) {
					start++;
					goto again0;
				}
				object = m->object;
				if (!VM_OBJECT_TRYLOCK(object)) {
					start++;
					goto again0;
				}
				if ((m->flags & PG_BUSY) || m->busy != 0) {
					VM_OBJECT_UNLOCK(object);
					start++;
					goto again0;
				}
				vm_page_free(m);
				VM_OBJECT_UNLOCK(object);
			}
		}
		mtx_lock_spin(&vm_page_queue_free_mtx);
		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			pqtype = pga[i].queue - pga[i].pc;
			if (pqtype != PQ_FREE) {
				start++;
				goto again;
			}
		}
		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			vm_page_t m = &pga[i];
			vm_pageq_remove_nowakeup(m);
			m->valid = VM_PAGE_BITS_ALL;
			if (m->flags & PG_ZERO)
				vm_page_zero_count--;
			/* Don't clear the PG_ZERO flag, we'll need it later. */
			m->flags = PG_UNMANAGED | (m->flags & PG_ZERO);
			KASSERT(m->dirty == 0,
			    ("contigmalloc1: page %p was dirty", m));
			m->wire_count = 0;
			m->busy = 0;
		}
		mtx_unlock_spin(&vm_page_queue_free_mtx);
		vm_page_unlock_queues();
		/*
		 * We've found a contiguous chunk that meets are requirements.
		 * Allocate kernel VM, unfree and assign the physical pages to
		 * it and return kernel VM pointer.
		 */
		vm_map_lock(map);
		if (vm_map_findspace(map, vm_map_min(map), size, &addr) !=
		    KERN_SUCCESS) {
			/*
			 * XXX We almost never run out of kernel virtual
			 * space, so we don't make the allocated memory
			 * above available.
			 */
			vm_map_unlock(map);
			return (NULL);
		}
		vm_object_reference(kernel_object);
		vm_map_insert(map, kernel_object, addr - VM_MIN_KERNEL_ADDRESS,
		    addr, addr + size, VM_PROT_ALL, VM_PROT_ALL, 0);
		vm_map_unlock(map);

		tmp_addr = addr;
		VM_OBJECT_LOCK(kernel_object);
		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			vm_page_t m = &pga[i];
			vm_page_insert(m, kernel_object,
				OFF_TO_IDX(tmp_addr - VM_MIN_KERNEL_ADDRESS));
			if ((flags & M_ZERO) && !(m->flags & PG_ZERO))
				pmap_zero_page(m);
			tmp_addr += PAGE_SIZE;
		}
		VM_OBJECT_UNLOCK(kernel_object);
		vm_map_wire(map, addr, addr + size,
		    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);

		return ((void *)addr);
	}
	return (NULL);
}

static void
vm_page_release_contigl(vm_page_t m, vm_pindex_t count)
{
	while (count--) {
		vm_page_free_toq(m);
		m++;
	}
}

void
vm_page_release_contig(vm_page_t m, vm_pindex_t count)
{
	vm_page_lock_queues();
	vm_page_release_contigl(m, count);
	vm_page_unlock_queues();
}

static int
vm_contig_unqueue_free(vm_page_t m)
{
	int error = 0;

	mtx_lock_spin(&vm_page_queue_free_mtx);
	if ((m->queue - m->pc) == PQ_FREE)
		vm_pageq_remove_nowakeup(m);
	else
		error = EAGAIN;
	mtx_unlock_spin(&vm_page_queue_free_mtx);
	if (error)
		return (error);
	m->valid = VM_PAGE_BITS_ALL;
	if (m->flags & PG_ZERO)
		vm_page_zero_count--;
	/* Don't clear the PG_ZERO flag; we'll need it later. */
	m->flags = PG_UNMANAGED | (m->flags & PG_ZERO);
	KASSERT(m->dirty == 0,
	    ("contigmalloc2: page %p was dirty", m));
	m->wire_count = 0;
	m->busy = 0;
	return (error);
}

vm_page_t
vm_page_alloc_contig(vm_pindex_t npages, vm_paddr_t low, vm_paddr_t high,
	    vm_offset_t alignment, vm_offset_t boundary)
{
	vm_object_t object;
	vm_offset_t size;
	vm_paddr_t phys;
	vm_page_t pga = vm_page_array;
	int i, pass, pqtype, start;

	size = npages << PAGE_SHIFT;
	if (size == 0)
		panic("vm_page_alloc_contig: size must not be 0");
	if ((alignment & (alignment - 1)) != 0)
		panic("vm_page_alloc_contig: alignment must be a power of 2");
	if ((boundary & (boundary - 1)) != 0)
		panic("vm_page_alloc_contig: boundary must be a power of 2");

	for (pass = 0; pass < 2; pass++) {
		start = vm_page_array_size - npages + 1;
		vm_page_lock_queues();
retry:
		start--;
		/*
		 * Find last page in array that is free, within range,
		 * aligned, and such that the boundary won't be crossed.
		 */
		for (i = start; i >= 0; i--) {
			phys = VM_PAGE_TO_PHYS(&pga[i]);
			pqtype = pga[i].queue - pga[i].pc;
			if (pass == 0) {
				if (pqtype != PQ_FREE && pqtype != PQ_CACHE)
					continue;
			} else if (pqtype != PQ_FREE && pqtype != PQ_CACHE &&
				    pga[i].queue != PQ_ACTIVE &&
				    pga[i].queue != PQ_INACTIVE)
				continue;
			if (phys >= low && phys + size <= high &&
			    ((phys & (alignment - 1)) == 0) &&
			    ((phys ^ (phys + size - 1)) & ~(boundary - 1)) == 0)
				break;
		}
		/* There are no candidates at all. */
		if (i == -1) {
			vm_page_unlock_queues();
			continue;
		}
		start = i;
		/*
		 * Check successive pages for contiguous and free.
		 */
		for (i = start + npages - 1; i > start; i--) {
			pqtype = pga[i].queue - pga[i].pc;
			if (VM_PAGE_TO_PHYS(&pga[i]) !=
			    VM_PAGE_TO_PHYS(&pga[i - 1]) + PAGE_SIZE) {
				start = i - npages + 1;
				goto retry;
			}
			if (pass == 0) {
				if (pqtype != PQ_FREE && pqtype != PQ_CACHE) {
					start = i - npages + 1;
					goto retry;
				}
			} else if (pqtype != PQ_FREE && pqtype != PQ_CACHE &&
				    pga[i].queue != PQ_ACTIVE &&
				    pga[i].queue != PQ_INACTIVE) {
				start = i - npages + 1;
				goto retry;
			}
		}
		for (i = start + npages - 1; i >= start; i--) {
			vm_page_t m = &pga[i];

retry_page:
			pqtype = m->queue - m->pc;
			if (pass != 0 && pqtype != PQ_FREE &&
			    pqtype != PQ_CACHE) {
				if (m->queue == PQ_ACTIVE ||
				    m->queue == PQ_INACTIVE) {
					if (vm_contig_launder_page(m) != 0)
						goto cleanup_freed;
					pqtype = m->queue - m->pc;
					if (pqtype == PQ_FREE ||
					    pqtype == PQ_CACHE)
						break;
				} else {
cleanup_freed:
					vm_page_release_contigl(&pga[i + 1],
					    start + npages - 1 - i);
					start = i - npages + 1;
					goto retry;
				}
			}
			if (pqtype == PQ_CACHE) {
				if (m->hold_count != 0) {
					start = i - npages + 1;
					goto retry;
				}
				object = m->object;
				if (!VM_OBJECT_TRYLOCK(object)) {
					start = i - npages + 1;
					goto retry;
				}
				if ((m->flags & PG_BUSY) || m->busy != 0) {
					VM_OBJECT_UNLOCK(object);
					start = i - npages + 1;
					goto retry;
				}
				vm_page_free(m);
				VM_OBJECT_UNLOCK(object);
			}
			/*
			 * There is no good API for freeing a page
			 * directly to PQ_NONE on our behalf, so spin.
			 */
			if (vm_contig_unqueue_free(m) != 0)
				goto retry_page;
		}
		vm_page_unlock_queues();
		/*
		 * We've found a contiguous chunk that meets are requirements.
		 */
		return (&pga[start]);
	}
	return (NULL);
}

static void *
contigmalloc2(vm_page_t m, vm_pindex_t npages, int flags)
{
	vm_object_t object = kernel_object;
	vm_map_t map = kernel_map;
	vm_offset_t addr, tmp_addr;
	vm_pindex_t i;
 
	/*
	 * Allocate kernel VM, unfree and assign the physical pages to
	 * it and return kernel VM pointer.
	 */
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
		if ((flags & M_ZERO) && !(m->flags & PG_ZERO))
			pmap_zero_page(&m[i]);
		tmp_addr += PAGE_SIZE;
	}
	VM_OBJECT_UNLOCK(object);
	vm_map_wire(map, addr, addr + (npages << PAGE_SHIFT),
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	return ((void *)addr);
}

static int vm_old_contigmalloc = 0;
SYSCTL_INT(_vm, OID_AUTO, old_contigmalloc,
    CTLFLAG_RW, &vm_old_contigmalloc, 0, "Use the old contigmalloc algorithm");
TUNABLE_INT("vm.old_contigmalloc", &vm_old_contigmalloc);

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
	void * ret;
	vm_page_t pages;
	vm_pindex_t npgs;

	npgs = round_page(size) >> PAGE_SHIFT;
	mtx_lock(&Giant);
	if (vm_old_contigmalloc) {
		ret = contigmalloc1(size, type, flags, low, high, alignment,
		    boundary, kernel_map);
	} else {
		pages = vm_page_alloc_contig(npgs, low, high,
		    alignment, boundary);
		if (pages == NULL) {
			ret = NULL;
		} else {
			ret = contigmalloc2(pages, npgs, flags);
			if (ret == NULL)
				vm_page_release_contig(pages, npgs);
		}
		
	}
	mtx_unlock(&Giant);
	malloc_type_allocated(type, ret == NULL ? 0 : npgs << PAGE_SHIFT);
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
