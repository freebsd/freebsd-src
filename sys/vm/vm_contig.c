/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

/*
 * This interface is for merging with malloc() someday.
 * Even if we never implement compaction so that contiguous allocation
 * works after initialization time, malloc()'s data structures are good
 * for statistics and for allocations of less than a page.
 */
void *
contigmalloc1(
	unsigned long size,	/* should be size_t here and for malloc() */
	struct malloc_type *type,
	int flags,
	unsigned long low,
	unsigned long high,
	unsigned long alignment,
	unsigned long boundary,
	vm_map_t map)
{
	int i, s, start;
	vm_offset_t addr, phys, tmp_addr;
	int pass;
	vm_page_t pga = vm_page_array;

	size = round_page(size);
	if (size == 0)
		panic("contigmalloc1: size must not be 0");
	if ((alignment & (alignment - 1)) != 0)
		panic("contigmalloc1: alignment must be a power of 2");
	if ((boundary & (boundary - 1)) != 0)
		panic("contigmalloc1: boundary must be a power of 2");

	start = 0;
	for (pass = 0; pass <= 1; pass++) {
		s = splvm();
again:
		/*
		 * Find first page in array that is free, within range, aligned, and
		 * such that the boundary won't be crossed.
		 */
		for (i = start; i < cnt.v_page_count; i++) {
			int pqtype;
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
			vm_page_t m, next;

again1:
			for (m = TAILQ_FIRST(&vm_page_queues[PQ_INACTIVE].pl);
				m != NULL;
				m = next) {

				KASSERT(m->queue == PQ_INACTIVE,
					("contigmalloc1: page %p is not PQ_INACTIVE", m));

				next = TAILQ_NEXT(m, pageq);
				if (vm_page_sleep_busy(m, TRUE, "vpctw0"))
					goto again1;
				vm_page_test_dirty(m);
				if (m->dirty) {
					if (m->object->type == OBJT_VNODE) {
						vn_lock(m->object->handle, LK_EXCLUSIVE | LK_RETRY, curproc);
						vm_object_page_clean(m->object, 0, 0, OBJPC_SYNC);
						VOP_UNLOCK(m->object->handle, 0, curproc);
						goto again1;
					} else if (m->object->type == OBJT_SWAP ||
								m->object->type == OBJT_DEFAULT) {
						vm_pageout_flush(&m, 1, 0);
						goto again1;
					}
				}
				if ((m->dirty == 0) && (m->busy == 0) && (m->hold_count == 0))
					vm_page_cache(m);
			}

			for (m = TAILQ_FIRST(&vm_page_queues[PQ_ACTIVE].pl);
				m != NULL;
				m = next) {

				KASSERT(m->queue == PQ_ACTIVE,
					("contigmalloc1: page %p is not PQ_ACTIVE", m));

				next = TAILQ_NEXT(m, pageq);
				if (vm_page_sleep_busy(m, TRUE, "vpctw1"))
					goto again1;
				vm_page_test_dirty(m);
				if (m->dirty) {
					if (m->object->type == OBJT_VNODE) {
						vn_lock(m->object->handle, LK_EXCLUSIVE | LK_RETRY, curproc);
						vm_object_page_clean(m->object, 0, 0, OBJPC_SYNC);
						VOP_UNLOCK(m->object->handle, 0, curproc);
						goto again1;
					} else if (m->object->type == OBJT_SWAP ||
								m->object->type == OBJT_DEFAULT) {
						vm_pageout_flush(&m, 1, 0);
						goto again1;
					}
				}
				if ((m->dirty == 0) && (m->busy == 0) && (m->hold_count == 0))
					vm_page_cache(m);
			}

			splx(s);
			continue;
		}
		start = i;

		/*
		 * Check successive pages for contiguous and free.
		 */
		for (i = start + 1; i < (start + size / PAGE_SIZE); i++) {
			int pqtype;
			pqtype = pga[i].queue - pga[i].pc;
			if ((VM_PAGE_TO_PHYS(&pga[i]) !=
			    (VM_PAGE_TO_PHYS(&pga[i - 1]) + PAGE_SIZE)) ||
			    ((pqtype != PQ_FREE) && (pqtype != PQ_CACHE))) {
				start++;
				goto again;
			}
		}

		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			int pqtype;
			vm_page_t m = &pga[i];

			pqtype = m->queue - m->pc;
			if (pqtype == PQ_CACHE) {
				vm_page_busy(m);
				vm_page_free(m);
			}

			TAILQ_REMOVE(&vm_page_queues[m->queue].pl, m, pageq);
			vm_page_queues[m->queue].lcnt--;
			cnt.v_free_count--;
			m->valid = VM_PAGE_BITS_ALL;
			m->flags = 0;
			KASSERT(m->dirty == 0, ("contigmalloc1: page %p was dirty", m));
			m->wire_count = 0;
			m->busy = 0;
			m->queue = PQ_NONE;
			m->object = NULL;
			vm_page_wire(m);
		}

		/*
		 * We've found a contiguous chunk that meets are requirements.
		 * Allocate kernel VM, unfree and assign the physical pages to it and
		 * return kernel VM pointer.
		 */
		tmp_addr = addr = kmem_alloc_pageable(map, size);
		if (addr == 0) {
			/*
			 * XXX We almost never run out of kernel virtual
			 * space, so we don't make the allocated memory
			 * above available.
			 */
			splx(s);
			return (NULL);
		}

		for (i = start; i < (start + size / PAGE_SIZE); i++) {
			vm_page_t m = &pga[i];
			vm_page_insert(m, kernel_object,
				OFF_TO_IDX(tmp_addr - VM_MIN_KERNEL_ADDRESS));
			pmap_kenter(tmp_addr, VM_PAGE_TO_PHYS(m));
			tmp_addr += PAGE_SIZE;
		}

		splx(s);
		return ((void *)addr);
	}
	return NULL;
}

void *
contigmalloc(
	unsigned long size,	/* should be size_t here and for malloc() */
	struct malloc_type *type,
	int flags,
	unsigned long low,
	unsigned long high,
	unsigned long alignment,
	unsigned long boundary)
{
	void * ret;

	GIANT_REQUIRED;
	ret = contigmalloc1(size, type, flags, low, high, alignment, boundary,
			     kernel_map);
	return (ret);

}

void
contigfree(void *addr, unsigned long size, struct malloc_type *type)
{
	GIANT_REQUIRED;
	kmem_free(kernel_map, (vm_offset_t)addr, size);
}

vm_offset_t
vm_page_alloc_contig(
	vm_offset_t size,
	vm_offset_t low,
	vm_offset_t high,
	vm_offset_t alignment)
{
	vm_offset_t ret;

	GIANT_REQUIRED;
	ret = ((vm_offset_t)contigmalloc1(size, M_DEVBUF, M_NOWAIT, low, high,
					  alignment, 0ul, kernel_map));
	return (ret);

}

