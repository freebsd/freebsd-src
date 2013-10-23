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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>

#include <vps/vps_account.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

vm_map_t kernel_map;
vm_map_t kmem_map;
vm_map_t exec_map;
vm_map_t pipe_map;

const void *zero_region;
CTASSERT((ZERO_REGION_SIZE & PAGE_MASK) == 0);

SYSCTL_ULONG(_vm, OID_AUTO, min_kernel_address, CTLFLAG_RD,
    NULL, VM_MIN_KERNEL_ADDRESS, "Min kernel address");

SYSCTL_ULONG(_vm, OID_AUTO, max_kernel_address, CTLFLAG_RD,
#if defined(__arm__) || defined(__sparc64__)
    &vm_max_kernel_address, 0,
#else
    NULL, VM_MAX_KERNEL_ADDRESS,
#endif
    "Max kernel address");

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
	result = vm_map_find(map, NULL, 0, &addr, size, VMFS_ANY_SPACE,
	    VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
	if (result != KERN_SUCCESS) {
		return (0);
	}
	return (addr);
}

/*
 *	kmem_alloc_nofault_space:
 *
 *	Allocate a virtual address range with no underlying object and
 *	no initial mapping to physical memory within the specified
 *	address space.  Any mapping from this range to physical memory
 *	must be explicitly created prior to its use, typically with
 *	pmap_qenter().  Any attempt to create a mapping on demand
 *	through vm_fault() will result in a panic. 
 */
vm_offset_t
kmem_alloc_nofault_space(map, size, find_space)
	vm_map_t map;
	vm_size_t size;
	int find_space;
{
	vm_offset_t addr;
	int result;

	size = round_page(size);
	addr = vm_map_min(map);
	result = vm_map_find(map, NULL, 0, &addr, size, find_space,
	    VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
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
	 * And finally, mark the data as non-pageable.
	 */
	(void) vm_map_wire(map, addr, addr + size,
	    VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES);

	return (addr);
}

/*
 *	Allocates a region from the kernel address map and physical pages
 *	within the specified address range to the kernel object.  Creates a
 *	wired mapping from this region to these pages, and returns the
 *	region's starting virtual address.  The allocated pages are not
 *	necessarily physically contiguous.  If M_ZERO is specified through the
 *	given flags, then the pages are zeroed before they are mapped.
 */
vm_offset_t
kmem_alloc_attr(vm_map_t map, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, vm_memattr_t memattr)
{
	vm_object_t object = kernel_object;
	vm_offset_t addr;
	vm_ooffset_t end_offset, offset;
	vm_page_t m;
	int pflags, tries;

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
	pflags = malloc2vm_flags(flags) | VM_ALLOC_NOBUSY;
	VM_OBJECT_WLOCK(object);
	end_offset = offset + size;
	for (; offset < end_offset; offset += PAGE_SIZE) {
		tries = 0;
retry:
		m = vm_page_alloc_contig(object, OFF_TO_IDX(offset), pflags, 1,
		    low, high, PAGE_SIZE, 0, memattr);
		if (m == NULL) {
			VM_OBJECT_WUNLOCK(object);
			if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
				vm_map_unlock(map);
				vm_pageout_grow_cache(tries, low, high);
				vm_map_lock(map);
				VM_OBJECT_WLOCK(object);
				tries++;
				goto retry;
			}

			/*
			 * Since the pages that were allocated by any previous
			 * iterations of this loop are not busy, they can be
			 * freed by vm_object_page_remove(), which is called
			 * by vm_map_delete().
			 */
			vm_map_delete(map, addr, addr + size);
			vm_map_unlock(map);
			return (0);
		}
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
	}
	VM_OBJECT_WUNLOCK(object);
	vm_map_unlock(map);
	vm_map_wire(map, addr, addr + size, VM_MAP_WIRE_SYSTEM |
	    VM_MAP_WIRE_NOHOLES);
	return (addr);
}

/*
 *	Allocates a region from the kernel address map and physically
 *	contiguous pages within the specified address range to the kernel
 *	object.  Creates a wired mapping from this region to these pages, and
 *	returns the region's starting virtual address.  If M_ZERO is specified
 *	through the given flags, then the pages are zeroed before they are
 *	mapped.
 */
vm_offset_t
kmem_alloc_contig(vm_map_t map, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, u_long alignment, vm_paddr_t boundary,
    vm_memattr_t memattr)
{
	vm_object_t object = kernel_object;
	vm_offset_t addr;
	vm_ooffset_t offset;
	vm_page_t end_m, m;
	int pflags, tries;
 
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
	pflags = malloc2vm_flags(flags) | VM_ALLOC_NOBUSY;
	VM_OBJECT_WLOCK(object);
	tries = 0;
retry:
	m = vm_page_alloc_contig(object, OFF_TO_IDX(offset), pflags,
	    atop(size), low, high, alignment, boundary, memattr);
	if (m == NULL) {
		VM_OBJECT_WUNLOCK(object);
		if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
			vm_map_unlock(map);
			vm_pageout_grow_cache(tries, low, high);
			vm_map_lock(map);
			VM_OBJECT_WLOCK(object);
			tries++;
			goto retry;
		}
		vm_map_delete(map, addr, addr + size);
		vm_map_unlock(map);
		return (0);
	}
	end_m = m + atop(size);
	for (; m < end_m; m++) {
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
	}
	VM_OBJECT_WUNLOCK(object);
	vm_map_unlock(map);
	vm_map_wire(map, addr, addr + size, VM_MAP_WIRE_SYSTEM |
	    VM_MAP_WIRE_NOHOLES);
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
 *	superpage_align	Request that min is superpage aligned
 */
vm_map_t
kmem_suballoc(vm_map_t parent, vm_offset_t *min, vm_offset_t *max,
    vm_size_t size, boolean_t superpage_align)
{
	int ret;
	vm_map_t result;

	size = round_page(size);

	*min = vm_map_min(parent);
	ret = vm_map_find(parent, NULL, 0, min, size, superpage_align ?
	    VMFS_ALIGNED_SPACE : VMFS_ANY_SPACE, VM_PROT_ALL, VM_PROT_ALL,
	    MAP_ACC_NO_CHARGE);
	if (ret != KERN_SUCCESS)
		panic("kmem_suballoc: bad status return of %d", ret);
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
 * 	We don't worry about expanding the map (adding entries) since entries
 * 	for wired maps are statically allocated.
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
	vm_offset_t addr;
	int i, rv;

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
			for (i = 0; i < 8; i++) {
				EVENTHANDLER_INVOKE(vm_lowmem, 0);
				uma_reclaim();
				vm_map_lock(map);
				if (vm_map_findspace(map, vm_map_min(map),
				    size, &addr) == 0) {
					break;
				}
				vm_map_unlock(map);
				tsleep(&i, 0, "nokva", (hz / 4) * (i + 1));
			}
			if (i == 8) {
				panic("kmem_malloc(%ld): kmem_map too small: %ld total allocated",
				    (long)size, (long)map->size);
			}
		} else {
			return (0);
		}
	}

	rv = kmem_back(map, addr, size, flags);
	vm_map_unlock(map);
	return (rv == KERN_SUCCESS ? addr : 0);
}

/*
 *	kmem_back:
 *
 *	Allocate physical pages for the specified virtual address range.
 */
int
kmem_back(vm_map_t map, vm_offset_t addr, vm_size_t size, int flags)
{
	vm_offset_t offset, i;
	vm_map_entry_t entry;
	vm_page_t m;
	int pflags;
	boolean_t found;

	KASSERT(vm_map_locked(map), ("kmem_back: map %p is not locked", map));
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kmem_object);
	vm_map_insert(map, kmem_object, offset, addr, addr + size,
	    VM_PROT_ALL, VM_PROT_ALL, 0);

	/*
	 * Assert: vm_map_insert() will never be able to extend the
	 * previous entry so vm_map_lookup_entry() will find a new
	 * entry exactly corresponding to this address range and it
	 * will have wired_count == 0.
	 */
	found = vm_map_lookup_entry(map, addr, &entry);
	KASSERT(found && entry->start == addr && entry->end == addr + size &&
	    entry->wired_count == 0 && (entry->eflags & MAP_ENTRY_IN_TRANSITION)
	    == 0, ("kmem_back: entry not found or misaligned"));

	pflags = malloc2vm_flags(flags) | VM_ALLOC_WIRED;

	VM_OBJECT_WLOCK(kmem_object);
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
				VM_OBJECT_WUNLOCK(kmem_object);
				entry->eflags |= MAP_ENTRY_IN_TRANSITION;
				vm_map_unlock(map);
				VM_WAIT;
				vm_map_lock(map);
				KASSERT(
(entry->eflags & (MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_NEEDS_WAKEUP)) ==
				    MAP_ENTRY_IN_TRANSITION,
				    ("kmem_back: volatile entry"));
				entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
				VM_OBJECT_WLOCK(kmem_object);
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
				vm_page_unwire(m, 0);
				vm_page_free(m);
			}
			VM_OBJECT_WUNLOCK(kmem_object);
			vm_map_delete(map, addr, addr + size);
			return (KERN_NO_SPACE);
		}
		if (flags & M_ZERO && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
		KASSERT((m->oflags & VPO_UNMANAGED) != 0,
		    ("kmem_malloc: page %p is managed", m));
	}
	VM_OBJECT_WUNLOCK(kmem_object);

	/*
	 * Mark map entry as non-pageable.  Repeat the assert.
	 */
	KASSERT(entry->start == addr && entry->end == addr + size &&
	    entry->wired_count == 0,
	    ("kmem_back: entry not found or misaligned after allocation"));
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
	VM_OBJECT_WLOCK(kmem_object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		m = vm_page_lookup(kmem_object, OFF_TO_IDX(offset + i));
		/*
		 * Because this is kernel_pmap, this call will not block.
		 */
		pmap_enter(kernel_pmap, addr + i, VM_PROT_ALL, m, VM_PROT_ALL,
		    TRUE);
		vm_page_wakeup(m);
	}
	VM_OBJECT_WUNLOCK(kmem_object);

	return (KERN_SUCCESS);
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
	if (!swap_reserve(size))
		return (0);

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
			swap_release(size);
			return (0);
		}
		map->needs_wakeup = TRUE;
		vm_map_unlock_and_wait(map, 0);
	}
	vm_map_insert(map, NULL, 0, addr, addr + size, VM_PROT_ALL,
	    VM_PROT_ALL, MAP_ACC_CHARGED);
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

static void
kmem_init_zero_region(void)
{
	vm_offset_t addr, i;
	vm_page_t m;
	int error;

	/*
	 * Map a single physical page of zeros to a larger virtual range.
	 * This requires less looping in places that want large amounts of
	 * zeros, while not using much more physical resources.
	 */
	addr = kmem_alloc_nofault(kernel_map, ZERO_REGION_SIZE);
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	for (i = 0; i < ZERO_REGION_SIZE; i += PAGE_SIZE)
		pmap_qenter(addr + i, &m, 1);
	error = vm_map_protect(kernel_map, addr, addr + ZERO_REGION_SIZE,
	    VM_PROT_READ, TRUE);
	KASSERT(error == 0, ("error=%d", error));

	zero_region = (const void *)addr;
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
#ifdef __amd64__
	    KERNBASE,
#else		     
	    VM_MIN_KERNEL_ADDRESS,
#endif
	    start, VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
	/* ... and ending with the completion of the above `insert' */
	vm_map_unlock(m);

	kmem_init_zero_region();
}

#ifdef DIAGNOSTIC
/*
 * Allow userspace to directly trigger the VM drain routine for testing
 * purposes.
 */
static int
debug_vm_lowmem(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error)
		return (error);
	if (i)	 
		EVENTHANDLER_INVOKE(vm_lowmem, 0);
	return (0);
}

SYSCTL_PROC(_debug, OID_AUTO, vm_lowmem, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    debug_vm_lowmem, "I", "set to trigger vm_lowmem event");
#endif
