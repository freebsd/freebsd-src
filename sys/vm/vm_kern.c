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
#include <vm/vm_pagequeue.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

struct vm_map kernel_map_store;
struct vm_map exec_map_store;
struct vm_map pipe_map_store;

const void *zero_region;
CTASSERT((ZERO_REGION_SIZE & PAGE_MASK) == 0);

/* NB: Used by kernel debuggers. */
const u_long vm_maxuser_address = VM_MAXUSER_ADDRESS;

u_int exec_map_entry_size;
u_int exec_map_entries;

SYSCTL_ULONG(_vm, OID_AUTO, min_kernel_address, CTLFLAG_RD,
    SYSCTL_NULL_ULONG_PTR, VM_MIN_KERNEL_ADDRESS, "Min kernel address");

SYSCTL_ULONG(_vm, OID_AUTO, max_kernel_address, CTLFLAG_RD,
#if defined(__arm__)
    &vm_max_kernel_address, 0,
#else
    SYSCTL_NULL_ULONG_PTR, VM_MAX_KERNEL_ADDRESS,
#endif
    "Max kernel address");

#if VM_NRESERVLEVEL > 0
#define	KVA_QUANTUM_SHIFT	(VM_LEVEL_0_ORDER + PAGE_SHIFT)
#else
/* On non-superpage architectures we want large import sizes. */
#define	KVA_QUANTUM_SHIFT	(8 + PAGE_SHIFT)
#endif
#define	KVA_QUANTUM		(1ul << KVA_QUANTUM_SHIFT)
#define	KVA_NUMA_IMPORT_QUANTUM	(KVA_QUANTUM * 128)

extern void     uma_startup2(void);

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

static vm_page_t
kmem_alloc_contig_pages(vm_object_t object, vm_pindex_t pindex, int domain,
    int pflags, u_long npages, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary, vm_memattr_t memattr)
{
	vm_page_t m;
	int tries;
	bool wait;

	VM_OBJECT_ASSERT_WLOCKED(object);

	wait = (pflags & VM_ALLOC_WAITOK) != 0;
	pflags &= ~(VM_ALLOC_NOWAIT | VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL);
	pflags |= VM_ALLOC_NOWAIT;
	for (tries = wait ? 3 : 1;; tries--) {
		m = vm_page_alloc_contig_domain(object, pindex, domain, pflags,
		    npages, low, high, alignment, boundary, memattr);
		if (m != NULL || tries == 0)
			break;

		VM_OBJECT_WUNLOCK(object);
		if (!vm_page_reclaim_contig_domain(domain, pflags, npages,
		    low, high, alignment, boundary) && wait)
			vm_wait_domain(domain);
		VM_OBJECT_WLOCK(object);
	}
	return (m);
}

/*
 *	Allocates a region from the kernel address map and physical pages
 *	within the specified address range to the kernel object.  Creates a
 *	wired mapping from this region to these pages, and returns the
 *	region's starting virtual address.  The allocated pages are not
 *	necessarily physically contiguous.  If M_ZERO is specified through the
 *	given flags, then the pages are zeroed before they are mapped.
 */
static vm_offset_t
kmem_alloc_attr_domain(int domain, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, vm_memattr_t memattr)
{
	vmem_t *vmem;
	vm_object_t object;
	vm_offset_t addr, i, offset;
	vm_page_t m;
	int pflags;
	vm_prot_t prot;

	object = kernel_object;
	size = round_page(size);
	vmem = vm_dom[domain].vmd_kernel_arena;
	if (vmem_alloc(vmem, size, M_BESTFIT | flags, &addr))
		return (0);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	pflags = malloc2vm_flags(flags) | VM_ALLOC_WIRED;
	prot = (flags & M_EXEC) != 0 ? VM_PROT_ALL : VM_PROT_RW;
	VM_OBJECT_WLOCK(object);
	for (i = 0; i < size; i += PAGE_SIZE) {
		m = kmem_alloc_contig_pages(object, atop(offset + i),
		    domain, pflags, 1, low, high, PAGE_SIZE, 0, memattr);
		if (m == NULL) {
			VM_OBJECT_WUNLOCK(object);
			kmem_unback(object, addr, i);
			vmem_free(vmem, addr, size);
			return (0);
		}
		KASSERT(vm_page_domain(m) == domain,
		    ("kmem_alloc_attr_domain: Domain mismatch %d != %d",
		    vm_page_domain(m), domain));
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		vm_page_valid(m);
		pmap_enter(kernel_pmap, addr + i, m, prot,
		    prot | PMAP_ENTER_WIRED, 0);
	}
	VM_OBJECT_WUNLOCK(object);
	return (addr);
}

vm_offset_t
kmem_alloc_attr(vm_size_t size, int flags, vm_paddr_t low, vm_paddr_t high,
    vm_memattr_t memattr)
{

	return (kmem_alloc_attr_domainset(DOMAINSET_RR(), size, flags, low,
	    high, memattr));
}

vm_offset_t
kmem_alloc_attr_domainset(struct domainset *ds, vm_size_t size, int flags,
    vm_paddr_t low, vm_paddr_t high, vm_memattr_t memattr)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	vm_domainset_iter_policy_init(&di, ds, &domain, &flags);
	do {
		addr = kmem_alloc_attr_domain(domain, size, flags, low, high,
		    memattr);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_policy(&di, &domain) == 0);

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
static vm_offset_t
kmem_alloc_contig_domain(int domain, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, u_long alignment, vm_paddr_t boundary,
    vm_memattr_t memattr)
{
	vmem_t *vmem;
	vm_object_t object;
	vm_offset_t addr, offset, tmp;
	vm_page_t end_m, m;
	u_long npages;
	int pflags;

	object = kernel_object;
	size = round_page(size);
	vmem = vm_dom[domain].vmd_kernel_arena;
	if (vmem_alloc(vmem, size, flags | M_BESTFIT, &addr))
		return (0);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	pflags = malloc2vm_flags(flags) | VM_ALLOC_WIRED;
	npages = atop(size);
	VM_OBJECT_WLOCK(object);
	m = kmem_alloc_contig_pages(object, atop(offset), domain,
	    pflags, npages, low, high, alignment, boundary, memattr);
	if (m == NULL) {
		VM_OBJECT_WUNLOCK(object);
		vmem_free(vmem, addr, size);
		return (0);
	}
	KASSERT(vm_page_domain(m) == domain,
	    ("kmem_alloc_contig_domain: Domain mismatch %d != %d",
	    vm_page_domain(m), domain));
	end_m = m + npages;
	tmp = addr;
	for (; m < end_m; m++) {
		if ((flags & M_ZERO) && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		vm_page_valid(m);
		pmap_enter(kernel_pmap, tmp, m, VM_PROT_RW,
		    VM_PROT_RW | PMAP_ENTER_WIRED, 0);
		tmp += PAGE_SIZE;
	}
	VM_OBJECT_WUNLOCK(object);
	return (addr);
}

vm_offset_t
kmem_alloc_contig(vm_size_t size, int flags, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary, vm_memattr_t memattr)
{

	return (kmem_alloc_contig_domainset(DOMAINSET_RR(), size, flags, low,
	    high, alignment, boundary, memattr));
}

vm_offset_t
kmem_alloc_contig_domainset(struct domainset *ds, vm_size_t size, int flags,
    vm_paddr_t low, vm_paddr_t high, u_long alignment, vm_paddr_t boundary,
    vm_memattr_t memattr)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	vm_domainset_iter_policy_init(&di, ds, &domain, &flags);
	do {
		addr = kmem_alloc_contig_domain(domain, size, flags, low, high,
		    alignment, boundary, memattr);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_policy(&di, &domain) == 0);

	return (addr);
}

/*
 *	kmem_subinit:
 *
 *	Initializes a map to manage a subrange
 *	of the kernel virtual address space.
 *
 *	Arguments are as follows:
 *
 *	parent		Map to take range from
 *	min, max	Returned endpoints of map
 *	size		Size of range to find
 *	superpage_align	Request that min is superpage aligned
 */
void
kmem_subinit(vm_map_t map, vm_map_t parent, vm_offset_t *min, vm_offset_t *max,
    vm_size_t size, bool superpage_align)
{
	int ret;

	size = round_page(size);

	*min = vm_map_min(parent);
	ret = vm_map_find(parent, NULL, 0, min, size, 0, superpage_align ?
	    VMFS_SUPER_SPACE : VMFS_ANY_SPACE, VM_PROT_ALL, VM_PROT_ALL,
	    MAP_ACC_NO_CHARGE);
	if (ret != KERN_SUCCESS)
		panic("kmem_subinit: bad status return of %d", ret);
	*max = *min + size;
	vm_map_init(map, vm_map_pmap(parent), *min, *max);
	if (vm_map_submap(parent, *min, *max, map) != KERN_SUCCESS)
		panic("kmem_subinit: unable to change range to submap");
}

/*
 *	kmem_malloc_domain:
 *
 *	Allocate wired-down pages in the kernel's address space.
 */
static vm_offset_t
kmem_malloc_domain(int domain, vm_size_t size, int flags)
{
	vmem_t *arena;
	vm_offset_t addr;
	int rv;

	if (__predict_true((flags & M_EXEC) == 0))
		arena = vm_dom[domain].vmd_kernel_arena;
	else
		arena = vm_dom[domain].vmd_kernel_rwx_arena;
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
kmem_malloc(vm_size_t size, int flags)
{

	return (kmem_malloc_domainset(DOMAINSET_RR(), size, flags));
}

vm_offset_t
kmem_malloc_domainset(struct domainset *ds, vm_size_t size, int flags)
{
	struct vm_domainset_iter di;
	vm_offset_t addr;
	int domain;

	vm_domainset_iter_policy_init(&di, ds, &domain, &flags);
	do {
		addr = kmem_malloc_domain(domain, size, flags);
		if (addr != 0)
			break;
	} while (vm_domainset_iter_policy(&di, &domain) == 0);

	return (addr);
}

/*
 *	kmem_back_domain:
 *
 *	Allocate physical pages from the specified domain for the specified
 *	virtual address range.
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
	pflags = malloc2vm_flags(flags) | VM_ALLOC_WIRED;
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
		KASSERT(vm_page_domain(m) == domain,
		    ("kmem_back_domain: Domain mismatch %d != %d",
		    vm_page_domain(m), domain));
		if (flags & M_ZERO && (m->flags & PG_ZERO) == 0)
			pmap_zero_page(m);
		KASSERT((m->oflags & VPO_UNMANAGED) != 0,
		    ("kmem_malloc: page %p is managed", m));
		vm_page_valid(m);
		pmap_enter(kernel_pmap, addr + i, m, prot,
		    prot | PMAP_ENTER_WIRED, 0);
		if (__predict_false((prot & VM_PROT_EXECUTE) != 0))
			m->oflags |= VPO_KMEM_EXEC;
	}
	VM_OBJECT_WUNLOCK(object);

	return (KERN_SUCCESS);
}

/*
 *	kmem_back:
 *
 *	Allocate physical pages for the specified virtual address range.
 */
int
kmem_back(vm_object_t object, vm_offset_t addr, vm_size_t size, int flags)
{
	vm_offset_t end, next, start;
	int domain, rv;

	KASSERT(object == kernel_object,
	    ("kmem_back: only supports kernel object."));

	for (start = addr, end = addr + size; addr < end; addr = next) {
		/*
		 * We must ensure that pages backing a given large virtual page
		 * all come from the same physical domain.
		 */
		if (vm_ndomains > 1) {
			domain = (addr >> KVA_QUANTUM_SHIFT) % vm_ndomains;
			while (VM_DOMAIN_EMPTY(domain))
				domain++;
			next = roundup2(addr + 1, KVA_QUANTUM);
			if (next > end || next < start)
				next = end;
		} else {
			domain = 0;
			next = end;
		}
		rv = kmem_back_domain(domain, object, addr, next - addr, flags);
		if (rv != KERN_SUCCESS) {
			kmem_unback(object, start, addr - start);
			break;
		}
	}
	return (rv);
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
static struct vmem *
_kmem_unback(vm_object_t object, vm_offset_t addr, vm_size_t size)
{
	struct vmem *arena;
	vm_page_t m, next;
	vm_offset_t end, offset;
	int domain;

	KASSERT(object == kernel_object,
	    ("kmem_unback: only supports kernel object."));

	if (size == 0)
		return (NULL);
	pmap_remove(kernel_pmap, addr, addr + size);
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	end = offset + size;
	VM_OBJECT_WLOCK(object);
	m = vm_page_lookup(object, atop(offset)); 
	domain = vm_page_domain(m);
	if (__predict_true((m->oflags & VPO_KMEM_EXEC) == 0))
		arena = vm_dom[domain].vmd_kernel_arena;
	else
		arena = vm_dom[domain].vmd_kernel_rwx_arena;
	for (; offset < end; offset += PAGE_SIZE, m = next) {
		next = vm_page_next(m);
		vm_page_xbusy_claim(m);
		vm_page_unwire_noq(m);
		vm_page_free(m);
	}
	VM_OBJECT_WUNLOCK(object);

	return (arena);
}

void
kmem_unback(vm_object_t object, vm_offset_t addr, vm_size_t size)
{

	(void)_kmem_unback(object, addr, size);
}

/*
 *	kmem_free:
 *
 *	Free memory allocated with kmem_malloc.  The size must match the
 *	original allocation.
 */
void
kmem_free(vm_offset_t addr, vm_size_t size)
{
	struct vmem *arena;

	size = round_page(size);
	arena = _kmem_unback(kernel_object, addr, size);
	if (arena != NULL)
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
		addr = vm_map_findspace(map, vm_map_min(map), size);
		if (addr + size <= vm_map_max(map))
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
	vm_map_insert(map, NULL, 0, addr, addr + size, VM_PROT_RW, VM_PROT_RW,
	    MAP_ACC_CHARGED);
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
 * Import KVA from the kernel map into the kernel arena.
 */
static int
kva_import(void *unused, vmem_size_t size, int flags, vmem_addr_t *addrp)
{
	vm_offset_t addr;
	int result;

	KASSERT((size % KVA_QUANTUM) == 0,
	    ("kva_import: Size %jd is not a multiple of %d",
	    (intmax_t)size, (int)KVA_QUANTUM));
	addr = vm_map_min(kernel_map);
	result = vm_map_find(kernel_map, NULL, 0, &addr, size, 0,
	    VMFS_SUPER_SPACE, VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
	if (result != KERN_SUCCESS)
                return (ENOMEM);

	*addrp = addr;

	return (0);
}

/*
 * Import KVA from a parent arena into a per-domain arena.  Imports must be
 * KVA_QUANTUM-aligned and a multiple of KVA_QUANTUM in size.
 */
static int
kva_import_domain(void *arena, vmem_size_t size, int flags, vmem_addr_t *addrp)
{

	KASSERT((size % KVA_QUANTUM) == 0,
	    ("kva_import_domain: Size %jd is not a multiple of %d",
	    (intmax_t)size, (int)KVA_QUANTUM));
	return (vmem_xalloc(arena, size, KVA_QUANTUM, 0, 0, VMEM_ADDR_MIN,
	    VMEM_ADDR_MAX, flags, addrp));
}

/*
 * 	kmem_init:
 *
 *	Create the kernel map; insert a mapping covering kernel text, 
 *	data, bss, and all space allocated thus far (`boostrap' data).  The 
 *	new map will thus map the range between VM_MIN_KERNEL_ADDRESS and 
 *	`start' as allocated, and the range between `start' and `end' as free.
 *	Create the kernel vmem arena and its per-domain children.
 */
void
kmem_init(vm_offset_t start, vm_offset_t end)
{
	vm_size_t quantum;
	int domain;

	vm_map_init(kernel_map, kernel_pmap, VM_MIN_KERNEL_ADDRESS, end);
	kernel_map->system_map = 1;
	vm_map_lock(kernel_map);
	/* N.B.: cannot use kgdb to debug, starting with this assignment ... */
	(void)vm_map_insert(kernel_map, NULL, 0,
#ifdef __amd64__
	    KERNBASE,
#else		     
	    VM_MIN_KERNEL_ADDRESS,
#endif
	    start, VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
	/* ... and ending with the completion of the above `insert' */

#ifdef __amd64__
	/*
	 * Mark KVA used for the page array as allocated.  Other platforms
	 * that handle vm_page_array allocation can simply adjust virtual_avail
	 * instead.
	 */
	(void)vm_map_insert(kernel_map, NULL, 0, (vm_offset_t)vm_page_array,
	    (vm_offset_t)vm_page_array + round_2mpage(vm_page_array_size *
	    sizeof(struct vm_page)),
	    VM_PROT_RW, VM_PROT_RW, MAP_NOFAULT);
#endif
	vm_map_unlock(kernel_map);

	/*
	 * Use a large import quantum on NUMA systems.  This helps minimize
	 * interleaving of superpages, reducing internal fragmentation within
	 * the per-domain arenas.
	 */
	if (vm_ndomains > 1 && PMAP_HAS_DMAP)
		quantum = KVA_NUMA_IMPORT_QUANTUM;
	else
		quantum = KVA_QUANTUM;

	/*
	 * Initialize the kernel_arena.  This can grow on demand.
	 */
	vmem_init(kernel_arena, "kernel arena", 0, 0, PAGE_SIZE, 0, 0);
	vmem_set_import(kernel_arena, kva_import, NULL, NULL, quantum);

	for (domain = 0; domain < vm_ndomains; domain++) {
		/*
		 * Initialize the per-domain arenas.  These are used to color
		 * the KVA space in a way that ensures that virtual large pages
		 * are backed by memory from the same physical domain,
		 * maximizing the potential for superpage promotion.
		 */
		vm_dom[domain].vmd_kernel_arena = vmem_create(
		    "kernel arena domain", 0, 0, PAGE_SIZE, 0, M_WAITOK);
		vmem_set_import(vm_dom[domain].vmd_kernel_arena,
		    kva_import_domain, NULL, kernel_arena, quantum);

		/*
		 * In architectures with superpages, maintain separate arenas
		 * for allocations with permissions that differ from the
		 * "standard" read/write permissions used for kernel memory,
		 * so as not to inhibit superpage promotion.
		 *
		 * Use the base import quantum since this arena is rarely used.
		 */
#if VM_NRESERVLEVEL > 0
		vm_dom[domain].vmd_kernel_rwx_arena = vmem_create(
		    "kernel rwx arena domain", 0, 0, PAGE_SIZE, 0, M_WAITOK);
		vmem_set_import(vm_dom[domain].vmd_kernel_rwx_arena,
		    kva_import_domain, (vmem_release_t *)vmem_xfree,
		    kernel_arena, KVA_QUANTUM);
#else
		vm_dom[domain].vmd_kernel_rwx_arena =
		    vm_dom[domain].vmd_kernel_arena;
#endif
	}

	/*
	 * This must be the very first call so that the virtual address
	 * space used for early allocations is properly marked used in
	 * the map.
	 */
	uma_startup2();
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

#ifdef __amd64__
	/*
	 * Preloaded files do not have execute permissions by default on amd64.
	 * Restore the default permissions to ensure that the direct map alias
	 * is updated.
	 */
	pmap_change_prot(start, end - start, VM_PROT_RW);
#endif
	for (va = start; va < end; va += PAGE_SIZE) {
		pa = pmap_kextract(va);
		m = PHYS_TO_VM_PAGE(pa);

		vmd = vm_pagequeue_domain(m);
		vm_domain_free_lock(vmd);
		vm_phys_free_pages(m, 0);
		vm_domain_free_unlock(vmd);

		vm_domain_freecnt_inc(vmd, 1);
		vm_cnt.v_page_count++;
	}
	pmap_remove(kernel_pmap, start, end);
	(void)vmem_add(kernel_arena, start, end - start, M_WAITOK);
#endif
}

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

SYSCTL_PROC(_debug, OID_AUTO, vm_lowmem, CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW, 0, 0,
    debug_vm_lowmem, "I", "set to trigger vm_lowmem event with given flags");
