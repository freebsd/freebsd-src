/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)vm_kern.c	8.3 (Berkeley) 1/12/94
 *
 *
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
 *
 * $Id: vm_kern.c,v 1.13.4.1 1996/01/31 12:06:48 davidg Exp $
 */

/*
 *	Kernel memory management.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>

vm_map_t buffer_map;
vm_map_t kernel_map;
vm_map_t kmem_map;
vm_map_t mb_map;
vm_map_t io_map;
vm_map_t clean_map;
vm_map_t pager_map;
vm_map_t phys_map;
vm_map_t exec_map;
vm_map_t u_map;
extern int mb_map_full;

/*
 *	kmem_alloc_pageable:
 *
 *	Allocate pageable memory to the kernel's address map.
 *	map must be "kernel_map" below.
 */

vm_offset_t
kmem_alloc_pageable(map, size)
	vm_map_t map;
	register vm_size_t size;
{
	vm_offset_t addr;
	register int result;

#if	0
	if (map != kernel_map)
		panic("kmem_alloc_pageable: not called with kernel_map");
#endif

	size = round_page(size);

	addr = vm_map_min(map);
	result = vm_map_find(map, NULL, (vm_offset_t) 0,
	    &addr, size, TRUE);
	if (result != KERN_SUCCESS) {
		return (0);
	}
	return (addr);
}

/*
 *	Allocate wired-down memory in the kernel's address map
 *	or a submap.
 */
vm_offset_t
kmem_alloc(map, size)
	register vm_map_t map;
	register vm_size_t size;
{
	vm_offset_t addr;
	register vm_offset_t offset;
	vm_offset_t i;

	size = round_page(size);

	/*
	 * Use the kernel object for wired-down kernel pages. Assume that no
	 * region of the kernel object is referenced more than once.
	 */

	/*
	 * Locate sufficient space in the map.  This will give us the final
	 * virtual address for the new memory, and thus will tell us the
	 * offset within the kernel map.
	 */
	vm_map_lock(map);
	if (vm_map_findspace(map, 0, size, &addr)) {
		vm_map_unlock(map);
		return (0);
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kernel_object);
	vm_map_insert(map, kernel_object, offset, addr, addr + size);
	vm_map_unlock(map);

	/*
	 * Guarantee that there are pages already in this object before
	 * calling vm_map_pageable.  This is to prevent the following
	 * scenario:
	 *
	 * 1) Threads have swapped out, so that there is a pager for the
	 * kernel_object. 2) The kmsg zone is empty, and so we are
	 * kmem_allocing a new page for it. 3) vm_map_pageable calls vm_fault;
	 * there is no page, but there is a pager, so we call
	 * pager_data_request.  But the kmsg zone is empty, so we must
	 * kmem_alloc. 4) goto 1 5) Even if the kmsg zone is not empty: when
	 * we get the data back from the pager, it will be (very stale)
	 * non-zero data.  kmem_alloc is defined to return zero-filled memory.
	 *
	 * We're intentionally not activating the pages we allocate to prevent a
	 * race with page-out.  vm_map_pageable will wire the pages.
	 */

	vm_object_lock(kernel_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		vm_page_t mem;

		while ((mem = vm_page_alloc(kernel_object, offset + i, VM_ALLOC_NORMAL)) == NULL) {
			vm_object_unlock(kernel_object);
			VM_WAIT;
			vm_object_lock(kernel_object);
		}
		vm_page_zero_fill(mem);
		mem->flags &= ~PG_BUSY;
		mem->valid = VM_PAGE_BITS_ALL;
	}
	vm_object_unlock(kernel_object);

	/*
	 * And finally, mark the data as non-pageable.
	 */

	(void) vm_map_pageable(map, (vm_offset_t) addr, addr + size, FALSE);

	/*
	 * Try to coalesce the map
	 */
	vm_map_simplify(map, addr);

	return (addr);
}

/*
 *	kmem_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kmem_alloc, and return the physical pages
 *	associated with that region.
 */
void
kmem_free(map, addr, size)
	vm_map_t map;
	register vm_offset_t addr;
	vm_size_t size;
{
	(void) vm_map_remove(map, trunc_page(addr), round_page(addr + size));
}

/*
 *	kmem_suballoc:
 *
 *	Allocates a map to manage a subrange
 *	of the kernel virtual address space.
 *
 *	Arguments are as follows:
 *
 *	parent		Map to take range from
 *	size		Size of range to find
 *	min, max	Returned endpoints of map
 *	pageable	Can the region be paged
 */
vm_map_t
kmem_suballoc(parent, min, max, size, pageable)
	register vm_map_t parent;
	vm_offset_t *min, *max;
	register vm_size_t size;
	boolean_t pageable;
{
	register int ret;
	vm_map_t result;

	size = round_page(size);

	*min = (vm_offset_t) vm_map_min(parent);
	ret = vm_map_find(parent, NULL, (vm_offset_t) 0,
	    min, size, TRUE);
	if (ret != KERN_SUCCESS) {
		printf("kmem_suballoc: bad status return of %d.\n", ret);
		panic("kmem_suballoc");
	}
	*max = *min + size;
	pmap_reference(vm_map_pmap(parent));
	result = vm_map_create(vm_map_pmap(parent), *min, *max, pageable);
	if (result == NULL)
		panic("kmem_suballoc: cannot create submap");
	if ((ret = vm_map_submap(parent, *min, *max, result)) != KERN_SUCCESS)
		panic("kmem_suballoc: unable to change range to submap");
	return (result);
}

/*
 * Allocate wired-down memory in the kernel's address map for the higher
 * level kernel memory allocator (kern/kern_malloc.c).  We cannot use
 * kmem_alloc() because we may need to allocate memory at interrupt
 * level where we cannot block (canwait == FALSE).
 *
 * This routine has its own private kernel submap (kmem_map) and object
 * (kmem_object).  This, combined with the fact that only malloc uses
 * this routine, ensures that we will never block in map or object waits.
 *
 * Note that this still only works in a uni-processor environment and
 * when called at splhigh().
 *
 * We don't worry about expanding the map (adding entries) since entries
 * for wired maps are statically allocated.
 */
vm_offset_t
kmem_malloc(map, size, waitflag)
	register vm_map_t map;
	register vm_size_t size;
	boolean_t waitflag;
{
	register vm_offset_t offset, i;
	vm_map_entry_t entry;
	vm_offset_t addr;
	vm_page_t m;

	if (map != kmem_map && map != mb_map)
		panic("kmem_malloc: map != {kmem,mb}_map");

	size = round_page(size);
	addr = vm_map_min(map);

	/*
	 * Locate sufficient space in the map.  This will give us the final
	 * virtual address for the new memory, and thus will tell us the
	 * offset within the kernel map.
	 */
	vm_map_lock(map);
	if (vm_map_findspace(map, 0, size, &addr)) {
		vm_map_unlock(map);
		if (map == mb_map) {
			mb_map_full = TRUE;
			log(LOG_ERR, "Out of mbuf clusters - increase maxusers!\n");
			return (0);
		}
		if (waitflag == M_WAITOK)
			panic("kmem_malloc: kmem_map too small");
		return (0);
	}
	offset = addr - vm_map_min(kmem_map);
	vm_object_reference(kmem_object);
	vm_map_insert(map, kmem_object, offset, addr, addr + size);

	/*
	 * If we can wait, just mark the range as wired (will fault pages as
	 * necessary).
	 */
	if (waitflag == M_WAITOK) {
		vm_map_unlock(map);
		(void) vm_map_pageable(map, (vm_offset_t) addr, addr + size,
		    FALSE);
		vm_map_simplify(map, addr);
		return (addr);
	}
	/*
	 * If we cannot wait then we must allocate all memory up front,
	 * pulling it off the active queue to prevent pageout.
	 */
	vm_object_lock(kmem_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		m = vm_page_alloc(kmem_object, offset + i,
			(waitflag == M_NOWAIT) ? VM_ALLOC_INTERRUPT : VM_ALLOC_SYSTEM);

		/*
		 * Ran out of space, free everything up and return. Don't need
		 * to lock page queues here as we know that the pages we got
		 * aren't on any queues.
		 */
		if (m == NULL) {
			while (i != 0) {
				i -= PAGE_SIZE;
				m = vm_page_lookup(kmem_object, offset + i);
				PAGE_WAKEUP(m);
				vm_page_free(m);
			}
			vm_object_unlock(kmem_object);
			vm_map_delete(map, addr, addr + size);
			vm_map_unlock(map);
			return (0);
		}
#if 0
		vm_page_zero_fill(m);
#endif
		m->flags &= ~PG_BUSY;
		m->valid = VM_PAGE_BITS_ALL;
	}
	vm_object_unlock(kmem_object);

	/*
	 * Mark map entry as non-pageable. Assert: vm_map_insert() will never
	 * be able to extend the previous entry so there will be a new entry
	 * exactly corresponding to this address range and it will have
	 * wired_count == 0.
	 */
	if (!vm_map_lookup_entry(map, addr, &entry) ||
	    entry->start != addr || entry->end != addr + size ||
	    entry->wired_count)
		panic("kmem_malloc: entry not found or misaligned");
	entry->wired_count++;

	/*
	 * Loop thru pages, entering them in the pmap. (We cannot add them to
	 * the wired count without wrapping the vm_page_queue_lock in
	 * splimp...)
	 */
	for (i = 0; i < size; i += PAGE_SIZE) {
		vm_object_lock(kmem_object);
		m = vm_page_lookup(kmem_object, offset + i);
		vm_object_unlock(kmem_object);
		pmap_kenter(addr + i, VM_PAGE_TO_PHYS(m));
	}
	vm_map_unlock(map);

	vm_map_simplify(map, addr);
	return (addr);
}

/*
 *	kmem_alloc_wait
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 */
vm_offset_t
kmem_alloc_wait(map, size)
	vm_map_t map;
	vm_size_t size;
{
	vm_offset_t addr;

	size = round_page(size);

	for (;;) {
		/*
		 * To make this work for more than one map, use the map's lock
		 * to lock out sleepers/wakers.
		 */
		vm_map_lock(map);
		if (vm_map_findspace(map, 0, size, &addr) == 0)
			break;
		/* no space now; see if we can ever get space */
		if (vm_map_max(map) - vm_map_min(map) < size) {
			vm_map_unlock(map);
			return (0);
		}
		assert_wait((int) map, TRUE);
		vm_map_unlock(map);
		thread_block("kmaw");
	}
	vm_map_insert(map, NULL, (vm_offset_t) 0, addr, addr + size);
	vm_map_unlock(map);
	return (addr);
}

/*
 *	kmem_free_wakeup
 *
 *	Returns memory to a submap of the kernel, and wakes up any threads
 *	waiting for memory in that map.
 */
void
kmem_free_wakeup(map, addr, size)
	vm_map_t map;
	vm_offset_t addr;
	vm_size_t size;
{
	vm_map_lock(map);
	(void) vm_map_delete(map, trunc_page(addr), round_page(addr + size));
	thread_wakeup((int) map);
	vm_map_unlock(map);
}

/*
 * Create the kernel map; insert a mapping covering kernel text, data, bss,
 * and all space allocated thus far (`boostrap' data).  The new map will thus
 * map the range between VM_MIN_KERNEL_ADDRESS and `start' as allocated, and
 * the range between `start' and `end' as free.
 */
void
kmem_init(start, end)
	vm_offset_t start, end;
{
	register vm_map_t m;

	m = vm_map_create(kernel_pmap, VM_MIN_KERNEL_ADDRESS, end, FALSE);
	vm_map_lock(m);
	/* N.B.: cannot use kgdb to debug, starting with this assignment ... */
	kernel_map = m;
	(void) vm_map_insert(m, NULL, (vm_offset_t) 0,
	    VM_MIN_KERNEL_ADDRESS, start);
	/* ... and ending with the completion of the above `insert' */
	vm_map_unlock(m);
}
