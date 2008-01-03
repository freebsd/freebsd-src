/*-
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
 */

/*
 *	Kernel memory management.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* for ticks and hz */
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

vm_map_t kernel_map=0;
vm_map_t kmem_map=0;
vm_map_t exec_map=0;
vm_map_t pipe_map;
vm_map_t buffer_map=0;

/*
 *	kmem_alloc_nofault:
 *
 *	Allocate a virtual address range with no underlying object and
 *	no initial mapping to physical memory.  Any mapping from this
 *	range to physical memory must be explicitly created prior to
 *	its use, typically with pmap_qenter().  Any attempt to create
 *	a mapping on demand through vm_fault() will result in a panic. 
 */
vm_offset_t
kmem_alloc_nofault(map, size)
	vm_map_t map;
	vm_size_t size;
{
	vm_offset_t addr;
	int result;

	size = round_page(size);
	addr = vm_map_min(map);
	result = vm_map_find(map, NULL, 0,
	    &addr, size, TRUE, VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
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
	vm_map_t map;
	vm_size_t size;
{
	vm_offset_t addr;
	vm_offset_t offset;
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
	if (vm_map_findspace(map, vm_map_min(map), size, &addr)) {
		vm_map_unlock(map);
		return (0);
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kernel_object);
	vm_map_insert(map, kernel_object, offset, addr, addr + size,
		VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);

	/*
	 * Guarantee that there are pages already in this object before
	 * calling vm_map_wire.  This is to prevent the following
	 * scenario:
	 *
	 * 1) Threads have swapped out, so that there is a pager for the
	 * kernel_object. 2) The kmsg zone is empty, and so we are
	 * kmem_allocing a new page for it. 3) vm_map_wire calls vm_fault;
	 * there is no page, but there is a pager, so we call
	 * pager_data_request.  But the kmsg zone is empty, so we must
	 * kmem_alloc. 4) goto 1 5) Even if the kmsg zone is not empty: when
	 * we get the data back from the pager, it will be (very stale)
	 * non-zero data.  kmem_alloc is defined to return zero-filled memory.
	 *
	 * We're intentionally not activating the pages we allocate to prevent a
	 * race with page-out.  vm_map_wire will wire the pages.
	 */
	VM_OBJECT_LOCK(kernel_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		vm_page_t mem;

		mem = vm_page_grab(kernel_object, OFF_TO_IDX(offset + i),
		    VM_ALLOC_NOBUSY | VM_ALLOC_ZERO | VM_ALLOC_RETRY);
		mem->valid = VM_PAGE_BITS_ALL;
		KASSERT((mem->flags & PG_UNMANAGED) != 0,
		    ("kmem_alloc: page %p is managed", mem));
	}
	VM_OBJECT_UNLOCK(kernel_object);

	/*
	 * And finally, mark the data as non-pageable.
	 */
	(void) vm_map_wire(map, addr, addr + size,
	    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);

	return (addr);
}

/*
 *	kmem_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kmem_alloc, and return the physical pages
 *	associated with that region.
 *
 *	This routine may not block on kernel maps.
 */
void
kmem_free(map, addr, size)
	vm_map_t map;
	vm_offset_t addr;
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
 *	min, max	Returned endpoints of map
 *	size		Size of range to find
 */
vm_map_t
kmem_suballoc(parent, min, max, size)
	vm_map_t parent;
	vm_offset_t *min, *max;
	vm_size_t size;
{
	int ret;
	vm_map_t result;

	size = round_page(size);

	*min = (vm_offset_t) vm_map_min(parent);
	ret = vm_map_find(parent, NULL, (vm_offset_t) 0,
	    min, size, TRUE, VM_PROT_ALL, VM_PROT_ALL, 0);
	if (ret != KERN_SUCCESS) {
		printf("kmem_suballoc: bad status return of %d.\n", ret);
		panic("kmem_suballoc");
	}
	*max = *min + size;
	result = vm_map_create(vm_map_pmap(parent), *min, *max);
	if (result == NULL)
		panic("kmem_suballoc: cannot create submap");
	if (vm_map_submap(parent, *min, *max, result) != KERN_SUCCESS)
		panic("kmem_suballoc: unable to change range to submap");
	return (result);
}

/*
 *	kmem_malloc:
 *
 * 	Allocate wired-down memory in the kernel's address map for the higher
 * 	level kernel memory allocator (kern/kern_malloc.c).  We cannot use
 * 	kmem_alloc() because we may need to allocate memory at interrupt
 * 	level where we cannot block (canwait == FALSE).
 *
 * 	This routine has its own private kernel submap (kmem_map) and object
 * 	(kmem_object).  This, combined with the fact that only malloc uses
 * 	this routine, ensures that we will never block in map or object waits.
 *
 * 	Note that this still only works in a uni-processor environment and
 * 	when called at splhigh().
 *
 * 	We don't worry about expanding the map (adding entries) since entries
 * 	for wired maps are statically allocated.
 *
 *	NOTE:  This routine is not supposed to block if M_NOWAIT is set, but
 *	I have not verified that it actually does not block.
 *
 *	`map' is ONLY allowed to be kmem_map or one of the mbuf submaps to
 *	which we never free.
 */
vm_offset_t
kmem_malloc(map, size, flags)
	vm_map_t map;
	vm_size_t size;
	int flags;
{
	vm_offset_t offset, i;
	vm_map_entry_t entry;
	vm_offset_t addr;
	vm_page_t m;
	int pflags;

	size = round_page(size);
	addr = vm_map_min(map);

	/*
	 * Locate sufficient space in the map.  This will give us the final
	 * virtual address for the new memory, and thus will tell us the
	 * offset within the kernel map.
	 */
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), size, &addr)) {
		vm_map_unlock(map);
		if ((flags & M_NOWAIT) == 0) {
			EVENTHANDLER_INVOKE(vm_lowmem, 0);
			uma_reclaim();
			vm_map_lock(map);
			if (vm_map_findspace(map, vm_map_min(map), size, &addr)) {
				vm_map_unlock(map);
				panic("kmem_malloc(%ld): kmem_map too small: %ld total allocated",
					(long)size, (long)map->size);
			}
		} else {
			return (0);
		}
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kmem_object);
	vm_map_insert(map, kmem_object, offset, addr, addr + size,
		VM_PROT_ALL, VM_PROT_ALL, 0);

	/*
	 * Note: if M_NOWAIT specified alone, allocate from 
	 * interrupt-safe queues only (just the free list).  If 
	 * M_USE_RESERVE is also specified, we can also
	 * allocate from the cache.  Neither of the latter two
	 * flags may be specified from an interrupt since interrupts
	 * are not allowed to mess with the cache queue.
	 */

	if ((flags & (M_NOWAIT|M_USE_RESERVE)) == M_NOWAIT)
		pflags = VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED;
	else
		pflags = VM_ALLOC_SYSTEM | VM_ALLOC_WIRED;

	if (flags & M_ZERO)
		pflags |= VM_ALLOC_ZERO;

	VM_OBJECT_LOCK(kmem_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
retry:
		m = vm_page_alloc(kmem_object, OFF_TO_IDX(offset + i), pflags);

		/*
		 * Ran out of space, free everything up and return. Don't need
		 * to lock page queues here as we know that the pages we got
		 * aren't on any queues.
		 */
		if (m == NULL) {
			if ((flags & M_NOWAIT) == 0) {
				VM_OBJECT_UNLOCK(kmem_object);
				vm_map_unlock(map);
				VM_WAIT;
				vm_map_lock(map);
				VM_OBJECT_LOCK(kmem_object);
				goto retry;
			}
			/* 
			 * Free the pages before removing the map entry.
			 * They are already marked busy.  Calling
			 * vm_map_delete before the pages has been freed or
			 * unbusied will cause a deadlock.
			 */
			while (i != 0) {
				i -= PAGE_SIZE;
				m = vm_page_lookup(kmem_object,
						   OFF_TO_IDX(offset + i));
				vm_page_lock_queues();
				vm_page_unwire(m, 0);
				vm_page_free(m);
				vm_page_unlock_queues();
			}
			VM_OBJECT_UNLOCK(kmem_object);
			vm_map_delete(map, addr, addr + size);
			vm_map_unlock(map);
			return (0);
		}
		if (flags & M_ZERO && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
		KASSERT((m->flags & PG_UNMANAGED) != 0,
		    ("kmem_malloc: page %p is managed", m));
	}
	VM_OBJECT_UNLOCK(kmem_object);

	/*
	 * Mark map entry as non-pageable. Assert: vm_map_insert() will never
	 * be able to extend the previous entry so there will be a new entry
	 * exactly corresponding to this address range and it will have
	 * wired_count == 0.
	 */
	if (!vm_map_lookup_entry(map, addr, &entry) ||
	    entry->start != addr || entry->end != addr + size ||
	    entry->wired_count != 0)
		panic("kmem_malloc: entry not found or misaligned");
	entry->wired_count = 1;

	/*
	 * At this point, the kmem_object must be unlocked because
	 * vm_map_simplify_entry() calls vm_object_deallocate(), which
	 * locks the kmem_object.
	 */
	vm_map_simplify_entry(map, entry);

	/*
	 * Loop thru pages, entering them in the pmap.
	 */
	VM_OBJECT_LOCK(kmem_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		m = vm_page_lookup(kmem_object, OFF_TO_IDX(offset + i));
		/*
		 * Because this is kernel_pmap, this call will not block.
		 */
		pmap_enter(kernel_pmap, addr + i, VM_PROT_ALL, m, VM_PROT_ALL,
		    TRUE);
		vm_page_wakeup(m);
	}
	VM_OBJECT_UNLOCK(kmem_object);
	vm_map_unlock(map);

	return (addr);
}

/*
 *	kmem_alloc_wait:
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 *	This routine may block.
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
		if (vm_map_findspace(map, vm_map_min(map), size, &addr) == 0)
			break;
		/* no space now; see if we can ever get space */
		if (vm_map_max(map) - vm_map_min(map) < size) {
			vm_map_unlock(map);
			return (0);
		}
		map->needs_wakeup = TRUE;
		vm_map_unlock_and_wait(map, 0);
	}
	vm_map_insert(map, NULL, 0, addr, addr + size, VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);
	return (addr);
}

/*
 *	kmem_free_wakeup:
 *
 *	Returns memory to a submap of the kernel, and wakes up any processes
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
	if (map->needs_wakeup) {
		map->needs_wakeup = FALSE;
		vm_map_wakeup(map);
	}
	vm_map_unlock(map);
}

/*
 * 	kmem_init:
 *
 *	Create the kernel map; insert a mapping covering kernel text, 
 *	data, bss, and all space allocated thus far (`boostrap' data).  The 
 *	new map will thus map the range between VM_MIN_KERNEL_ADDRESS and 
 *	`start' as allocated, and the range between `start' and `end' as free.
 */
void
kmem_init(start, end)
	vm_offset_t start, end;
{
	vm_map_t m;

	m = vm_map_create(kernel_pmap, VM_MIN_KERNEL_ADDRESS, end);
	m->system_map = 1;
	vm_map_lock(m);
	/* N.B.: cannot use kgdb to debug, starting with this assignment ... */
	kernel_map = m;
	(void) vm_map_insert(m, NULL, (vm_ooffset_t) 0,
	    VM_MIN_KERNEL_ADDRESS, start, VM_PROT_ALL, VM_PROT_ALL,
	    MAP_NOFAULT);
	/* ... and ending with the completion of the above `insert' */
	vm_map_unlock(m);
}
