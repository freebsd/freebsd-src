/*-
 * SPDX-License-Identifier: (BSD-3-Clause AND MIT-CMU)
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* for ticks and hz */
#include <sys/domainset.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/vmem.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_domainset.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_radix.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

vm_map_t kernel_map;
vm_map_t exec_map;
vm_map_t pipe_map;

const void *zero_region;
CTASSERT((ZERO_REGION_SIZE & PAGE_MASK) == 0);

/* NB: Used by kernel debuggers. */
const u_long vm_maxuser_address = VM_MAXUSER_ADDRESS;

u_int exec_map_entry_size;
u_int exec_map_entries;

SYSCTL_ULONG(_vm, OID_AUTO, min_kernel_address, CTLFLAG_RD,
    SYSCTL_NULL_ULONG_PTR, VM_MIN_KERNEL_ADDRESS, "Min kernel address");

SYSCTL_ULONG(_vm, OID_AUTO, max_kernel_address, CTLFLAG_RD,
#if defined(__arm__) || defined(__sparc64__)
    &vm_max_kernel_address, 0,
#else
    SYSCTL_NULL_ULONG_PTR, VM_MAX_KERNEL_ADDRESS,
#endif
    "Max kernel address");

/*
 *	kva_alloc:
 *
 *	Allocate a virtual address range with no underlying object and
 *	no initial mapping to physical memory.  Any mapping from this
 *	range to physical memory must be explicitly created prior to
 *	its use, typically with pmap_qenter().  Any attempt to create
 *	a mapping on demand through vm_fault() will result in a panic. 
 */
vm_offset_t
kva_alloc(vm_size_t size)
{
	vm_offset_t addr;

	size = round_page(size);
	if (vmem_alloc(kernel_arena, size, M_BESTFIT | M_NOWAIT, &addr))
		return (0);

	return (addr);
}

/*
 *	kva_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kva_alloc, and return the physical pages
 *	associated with that region.
 *
 *	This routine may not block on kernel maps.
 */
void
kva_free(vm_offset_t addr, vm_size_t size)
{

	size = round_page(size);
	vmem_free(kernel_arena, addr, size);
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
kmem_alloc_attr_domain(int domain, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, vm_memattr_t memattr)
{
	vmem_t *vmem;
	vm_object_t object = kernel_object;
	vm_offset_t addr, i, offset;
	vm_page_t m;
	int pflags, tries;

	size = round_page(size);
	vmem = vm_dom[domain].vmd_kernel_arena;
	if (vmem_alloc(vmem, size, M_BESTFIT | flags, &addr))
		return (0);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	pflags = malloc2vm_flags(flags) | VM_ALLOC_NOBUSY | VM_ALLOC_WIRED;
	pflags &= ~(VM_ALLOC_NOWAIT | VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL);
	pflags |= VM_ALLOC_NOWAIT;
	VM_OBJECT_WLOCK(object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		tries = 0;
retry:
		m = vm_page_alloc_contig_domain(object, atop(offset + i),
		    domain, pflags, 1, low, high, PAGE_SIZE, 0, memattr);
		if (m == NULL) {
			VM_OBJECT_WUNLOCK(object);
			if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
				if (!vm_page_reclaim_contig_domain(domain,
				    pflags, 1, low, high, PAGE_SIZE, 0) &&
				    (flags & M_WAITOK) != 0)
					vm_wait_domain(domain);
				VM_OBJECT_WLOCK(object);
				tries++;
				goto retry;
			}
			kmem_unback(object, addr, i);
			vmem_free(vmem, addr, size);
			return (0);
		}
		KASSERT(vm_phys_domain(m) == domain,
		    ("kmem_alloc_attr_domain: Domain mismatch %d != %d",
		    vm_phys_domain(m), domain));
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
		pmap_enter(kernel_pmap, addr + i, m, VM_PROT_RW,
		    VM_PROT_RW | PMAP_ENTER_WIRED, 0);
	}
	VM_OBJECT_WUNLOCK(object);
	return (addr);
}

vm_offset_t
kmem_alloc_attr(vmem_t *vmem, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, vm_memattr_t memattr)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	KASSERT(vmem == kernel_arena,
	    ("kmem_alloc_attr: Only kernel_arena is supported."));

	vm_domainset_iter_malloc_init(&di, kernel_object, &domain, &flags);
	do {
		addr = kmem_alloc_attr_domain(domain, size, flags, low, high,
		    memattr);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_malloc(&di, &domain, &flags) == 0);

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
kmem_alloc_contig_domain(int domain, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, u_long alignment, vm_paddr_t boundary,
    vm_memattr_t memattr)
{
	vmem_t *vmem;
	vm_object_t object = kernel_object;
	vm_offset_t addr, offset, tmp;
	vm_page_t end_m, m;
	u_long npages;
	int pflags, tries;
 
	size = round_page(size);
	vmem = vm_dom[domain].vmd_kernel_arena;
	if (vmem_alloc(vmem, size, flags | M_BESTFIT, &addr))
		return (0);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	pflags = malloc2vm_flags(flags) | VM_ALLOC_NOBUSY | VM_ALLOC_WIRED;
	pflags &= ~(VM_ALLOC_NOWAIT | VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL);
	pflags |= VM_ALLOC_NOWAIT;
	npages = atop(size);
	VM_OBJECT_WLOCK(object);
	tries = 0;
retry:
	m = vm_page_alloc_contig_domain(object, atop(offset), domain, pflags,
	    npages, low, high, alignment, boundary, memattr);
	if (m == NULL) {
		VM_OBJECT_WUNLOCK(object);
		if (tries < ((flags & M_NOWAIT) != 0 ? 1 : 3)) {
			if (!vm_page_reclaim_contig_domain(domain, pflags,
			    npages, low, high, alignment, boundary) &&
			    (flags & M_WAITOK) != 0)
				vm_wait_domain(domain);
			VM_OBJECT_WLOCK(object);
			tries++;
			goto retry;
		}
		vmem_free(vmem, addr, size);
		return (0);
	}
	KASSERT(vm_phys_domain(m) == domain,
	    ("kmem_alloc_contig_domain: Domain mismatch %d != %d",
	    vm_phys_domain(m), domain));
	end_m = m + npages;
	tmp = addr;
	for (; m < end_m; m++) {
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		m->valid = VM_PAGE_BITS_ALL;
		pmap_enter(kernel_pmap, tmp, m, VM_PROT_RW,
		    VM_PROT_RW | PMAP_ENTER_WIRED, 0);
		tmp += PAGE_SIZE;
	}
	VM_OBJECT_WUNLOCK(object);
	return (addr);
}

vm_offset_t
kmem_alloc_contig(struct vmem *vmem, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, u_long alignment, vm_paddr_t boundary,
    vm_memattr_t memattr)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	KASSERT(vmem == kernel_arena,
	    ("kmem_alloc_contig: Only kernel_arena is supported."));

	vm_domainset_iter_malloc_init(&di, kernel_object, &domain, &flags);
	do {
		addr = kmem_alloc_contig_domain(domain, size, flags, low, high,
		    alignment, boundary, memattr);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_malloc(&di, &domain, &flags) == 0);

	return (addr);
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
	ret = vm_map_find(parent, NULL, 0, min, size, 0, superpage_align ?
	    VMFS_SUPER_SPACE : VMFS_ANY_SPACE, VM_PROT_ALL, VM_PROT_ALL,
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
 *	Allocate wired-down pages in the kernel's address space.
 */
vm_offset_t
kmem_malloc_domain(int domain, vm_size_t size, int flags)
{
	vmem_t *arena;
	vm_offset_t addr;
	int rv;

#if VM_NRESERVLEVEL > 0
	if (__predict_true((flags & M_EXEC) == 0))
		arena = vm_dom[domain].vmd_kernel_arena;
	else
		arena = vm_dom[domain].vmd_kernel_rwx_arena;
#else
	arena = vm_dom[domain].vmd_kernel_arena;
#endif
	size = round_page(size);
	if (vmem_alloc(arena, size, flags | M_BESTFIT, &addr))
		return (0);

	rv = kmem_back_domain(domain, kernel_object, addr, size, flags);
	if (rv != KERN_SUCCESS) {
		vmem_free(arena, addr, size);
		return (0);
	}
	return (addr);
}

vm_offset_t
kmem_malloc(struct vmem *vmem __unused, vm_size_t size, int flags)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	vm_domainset_iter_malloc_init(&di, kernel_object, &domain, &flags);
	do {
		addr = kmem_malloc_domain(domain, size, flags);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_malloc(&di, &domain, &flags) == 0);

	return (addr);
}

/*
 *	kmem_back:
 *
 *	Allocate physical pages for the specified virtual address range.
 */
int
kmem_back_domain(int domain, vm_object_t object, vm_offset_t addr,
    vm_size_t size, int flags)
{
	vm_offset_t offset, i;
	vm_page_t m, mpred;
	vm_prot_t prot;
	int pflags;

	KASSERT(object == kernel_object,
	    ("kmem_back_domain: only supports kernel object."));

	offset = addr - VM_MIN_KERNEL_ADDRESS;
	pflags = malloc2vm_flags(flags) | VM_ALLOC_NOBUSY | VM_ALLOC_WIRED;
	pflags &= ~(VM_ALLOC_NOWAIT | VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL);
	if (flags & M_WAITOK)
		pflags |= VM_ALLOC_WAITFAIL;
	prot = (flags & M_EXEC) != 0 ? VM_PROT_ALL : VM_PROT_RW;

	i = 0;
	VM_OBJECT_WLOCK(object);
retry:
	mpred = vm_radix_lookup_le(&object->rtree, atop(offset + i));
	for (; i < size; i += PAGE_SIZE, mpred = m) {
		m = vm_page_alloc_domain_after(object, atop(offset + i),
		    domain, pflags, mpred);

		/*
		 * Ran out of space, free everything up and return. Don't need
		 * to lock page queues here as we know that the pages we got
		 * aren't on any queues.
		 */
		if (m == NULL) {
			if ((flags & M_NOWAIT) == 0)
				goto retry;
			VM_OBJECT_WUNLOCK(object);
			kmem_unback(object, addr, i);
			return (KERN_NO_SPACE);
		}
		KASSERT(vm_phys_domain(m) == domain,
		    ("kmem_back_domain: Domain mismatch %d != %d",
		    vm_phys_domain(m), domain));
		if (flags & M_ZERO && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		KASSERT((m->oflags & VPO_UNMANAGED) != 0,
		    ("kmem_malloc: page %p is managed", m));
		m->valid = VM_PAGE_BITS_ALL;
		pmap_enter(kernel_pmap, addr + i, m, prot,
		    prot | PMAP_ENTER_WIRED, 0);
	}
	VM_OBJECT_WUNLOCK(object);

	return (KERN_SUCCESS);
}

int
kmem_back(vm_object_t object, vm_offset_t addr, vm_size_t size, int flags)
{
	struct vm_domainset_iter di;
	int domain;
	int ret;

	KASSERT(object == kernel_object,
	    ("kmem_back: only supports kernel object."));

	vm_domainset_iter_malloc_init(&di, kernel_object, &domain, &flags);
	do {
		ret = kmem_back_domain(domain, object, addr, size, flags);
		if (ret == KERN_SUCCESS)
			break;
	} while (vm_domainset_iter_malloc(&di, &domain, &flags) == 0);

	return (ret);
}

/*
 *	kmem_unback:
 *
 *	Unmap and free the physical pages underlying the specified virtual
 *	address range.
 *
 *	A physical page must exist within the specified object at each index
 *	that is being unmapped.
 */
static int
_kmem_unback(vm_object_t object, vm_offset_t addr, vm_size_t size)
{
	vm_page_t m, next;
	vm_offset_t end, offset;
	int domain;

	KASSERT(object == kernel_object,
	    ("kmem_unback: only supports kernel object."));

	if (size == 0)
		return (0);
	pmap_remove(kernel_pmap, addr, addr + size);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	end = offset + size;
	VM_OBJECT_WLOCK(object);
	m = vm_page_lookup(object, atop(offset)); 
	domain = vm_phys_domain(m);
	for (; offset < end; offset += PAGE_SIZE, m = next) {
		next = vm_page_next(m);
		vm_page_unwire(m, PQ_NONE);
		vm_page_free(m);
	}
	VM_OBJECT_WUNLOCK(object);

	return (domain);
}

void
kmem_unback(vm_object_t object, vm_offset_t addr, vm_size_t size)
{

	_kmem_unback(object, addr, size);
}

/*
 *	kmem_free:
 *
 *	Free memory allocated with kmem_malloc.  The size must match the
 *	original allocation.
 */
void
kmem_free(struct vmem *vmem, vm_offset_t addr, vm_size_t size)
{
	struct vmem *arena;
	int domain;

#if VM_NRESERVLEVEL > 0
	KASSERT(vmem == kernel_arena || vmem == kernel_rwx_arena,
	    ("kmem_free: Only kernel_arena or kernel_rwx_arena are supported."));
#else
	KASSERT(vmem == kernel_arena,
	    ("kmem_free: Only kernel_arena is supported."));
#endif

	size = round_page(size);
	domain = _kmem_unback(kernel_object, addr, size);
#if VM_NRESERVLEVEL > 0
	if (__predict_true(vmem == kernel_arena))
		arena = vm_dom[domain].vmd_kernel_arena;
	else
		arena = vm_dom[domain].vmd_kernel_rwx_arena;
#else
	arena = vm_dom[domain].vmd_kernel_arena;
#endif
	vmem_free(arena, addr, size);
}

/*
 *	kmap_alloc_wait:
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 *	This routine may block.
 */
vm_offset_t
kmap_alloc_wait(vm_map_t map, vm_size_t size)
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
 *	kmap_free_wakeup:
 *
 *	Returns memory to a submap of the kernel, and wakes up any processes
 *	waiting for memory in that map.
 */
void
kmap_free_wakeup(vm_map_t map, vm_offset_t addr, vm_size_t size)
{

	vm_map_lock(map);
	(void) vm_map_delete(map, trunc_page(addr), round_page(addr + size));
	if (map->needs_wakeup) {
		map->needs_wakeup = FALSE;
		vm_map_wakeup(map);
	}
	vm_map_unlock(map);
}

void
kmem_init_zero_region(void)
{
	vm_offset_t addr, i;
	vm_page_t m;

	/*
	 * Map a single physical page of zeros to a larger virtual range.
	 * This requires less looping in places that want large amounts of
	 * zeros, while not using much more physical resources.
	 */
	addr = kva_alloc(ZERO_REGION_SIZE);
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	for (i = 0; i < ZERO_REGION_SIZE; i += PAGE_SIZE)
		pmap_qenter(addr + i, &m, 1);
	pmap_protect(kernel_pmap, addr, addr + ZERO_REGION_SIZE, VM_PROT_READ);

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
kmem_init(vm_offset_t start, vm_offset_t end)
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
}

/*
 *	kmem_bootstrap_free:
 *
 *	Free pages backing preloaded data (e.g., kernel modules) to the
 *	system.  Currently only supported on platforms that create a
 *	vm_phys segment for preloaded data.
 */
void
kmem_bootstrap_free(vm_offset_t start, vm_size_t size)
{
#if defined(__i386__) || defined(__amd64__)
	struct vm_domain *vmd;
	vm_offset_t end, va;
	vm_paddr_t pa;
	vm_page_t m;

	end = trunc_page(start + size);
	start = round_page(start);

	for (va = start; va < end; va += PAGE_SIZE) {
		pa = pmap_kextract(va);
		m = PHYS_TO_VM_PAGE(pa);

		vmd = vm_pagequeue_domain(m);
		vm_domain_free_lock(vmd);
		vm_phys_free_pages(m, 0);
		vmd->vmd_page_count++;
		vm_domain_free_unlock(vmd);

		vm_domain_freecnt_inc(vmd, 1);
		vm_cnt.v_page_count++;
	}
	pmap_remove(kernel_pmap, start, end);
	(void)vmem_add(kernel_arena, start, end - start, M_WAITOK);
#endif
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
	if ((i & ~(VM_LOW_KMEM | VM_LOW_PAGES)) != 0)
		return (EINVAL);
	if (i != 0)
		EVENTHANDLER_INVOKE(vm_lowmem, i);
	return (0);
}

SYSCTL_PROC(_debug, OID_AUTO, vm_lowmem, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    debug_vm_lowmem, "I", "set to trigger vm_lowmem event with given flags");
#endif
