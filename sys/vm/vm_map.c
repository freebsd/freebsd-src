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
 *	from: @(#)vm_map.c	8.3 (Berkeley) 1/12/94
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
 * $FreeBSD$
 */

/*
 *	Virtual memory mapping module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/file.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>
#include <vm/vm_zone.h>

/*
 *	Virtual memory maps provide for the mapping, protection,
 *	and sharing of virtual memory objects.  In addition,
 *	this module provides for an efficient virtual copy of
 *	memory from one map to another.
 *
 *	Synchronization is required prior to most operations.
 *
 *	Maps consist of an ordered doubly-linked list of simple
 *	entries; a single hint is used to speed up lookups.
 *
 *	Since portions of maps are specified by start/end addresses,
 *	which may not align with existing map entries, all
 *	routines merely "clip" entries to these start/end values.
 *	[That is, an entry is split into two, bordering at a
 *	start or end value.]  Note that these clippings may not
 *	always be necessary (as the two resulting entries are then
 *	not changed); however, the clipping is done for convenience.
 *
 *	As mentioned above, virtual copy operations are performed
 *	by copying VM object references from one map to
 *	another, and then marking both regions as copy-on-write.
 */

/*
 *	vm_map_startup:
 *
 *	Initialize the vm_map module.  Must be called before
 *	any other vm_map routines.
 *
 *	Map and entry structures are allocated from the general
 *	purpose memory pool with some exceptions:
 *
 *	- The kernel map and kmem submap are allocated statically.
 *	- Kernel map entries are allocated out of a static pool.
 *
 *	These restrictions are necessary since malloc() uses the
 *	maps and requires map entries.
 */

static struct vm_zone kmapentzone_store, mapentzone_store, mapzone_store;
static vm_zone_t mapentzone, kmapentzone, mapzone, vmspace_zone;
static struct vm_object kmapentobj, mapentobj, mapobj;

static struct vm_map_entry map_entry_init[MAX_MAPENT];
static struct vm_map_entry kmap_entry_init[MAX_KMAPENT];
static struct vm_map map_init[MAX_KMAP];

static void _vm_map_clip_end __P((vm_map_t, vm_map_entry_t, vm_offset_t));
static void _vm_map_clip_start __P((vm_map_t, vm_map_entry_t, vm_offset_t));
static vm_map_entry_t vm_map_entry_create __P((vm_map_t));
static void vm_map_entry_delete __P((vm_map_t, vm_map_entry_t));
static void vm_map_entry_dispose __P((vm_map_t, vm_map_entry_t));
static void vm_map_entry_unwire __P((vm_map_t, vm_map_entry_t));
static void vm_map_copy_entry __P((vm_map_t, vm_map_t, vm_map_entry_t,
		vm_map_entry_t));
static void vm_map_split __P((vm_map_entry_t));
static void vm_map_unclip_range __P((vm_map_t map, vm_map_entry_t start_entry, vm_offset_t start, vm_offset_t end, int flags));

static int old_msync;
SYSCTL_INT(_vm, OID_AUTO, old_msync, CTLFLAG_RW, &old_msync, 0,
    "Use old (insecure) msync behavior");

void
vm_map_startup()
{
	mapzone = &mapzone_store;
	zbootinit(mapzone, "MAP", sizeof (struct vm_map),
		map_init, MAX_KMAP);
	kmapentzone = &kmapentzone_store;
	zbootinit(kmapentzone, "KMAP ENTRY", sizeof (struct vm_map_entry),
		kmap_entry_init, MAX_KMAPENT);
	mapentzone = &mapentzone_store;
	zbootinit(mapentzone, "MAP ENTRY", sizeof (struct vm_map_entry),
		map_entry_init, MAX_MAPENT);
}

/*
 * Allocate a vmspace structure, including a vm_map and pmap,
 * and initialize those structures.  The refcnt is set to 1.
 * The remaining fields must be initialized by the caller.
 */
struct vmspace *
vmspace_alloc(min, max)
	vm_offset_t min, max;
{
	struct vmspace *vm;

	vm = zalloc(vmspace_zone);
	vm_map_init(&vm->vm_map, min, max);
	pmap_pinit(vmspace_pmap(vm));
	vm->vm_map.pmap = vmspace_pmap(vm);		/* XXX */
	vm->vm_refcnt = 1;
	vm->vm_shm = NULL;
	vm->vm_exitingcnt = 0;
	return (vm);
}

void
vm_init2(void) {
	zinitna(kmapentzone, &kmapentobj,
		NULL, 0, lmin((VM_MAX_KERNEL_ADDRESS - KERNBASE) / PAGE_SIZE,
		cnt.v_page_count) / 8 + maxproc * 2 + maxfiles,
		ZONE_INTERRUPT, 1);
	zinitna(mapentzone, &mapentobj,
		NULL, 0, 0, 0, 1);
	zinitna(mapzone, &mapobj,
		NULL, 0, 0, 0, 1);
	vmspace_zone = zinit("VMSPACE", sizeof (struct vmspace), 0, 0, 3);
	pmap_init2();
	vm_object_init2();
}

static __inline void
vmspace_dofree(struct vmspace *vm)
{
	/*
	 * Lock the map, to wait out all other references to it.
	 * Delete all of the mappings and pages they hold, then call
	 * the pmap module to reclaim anything left.
	 */
	vm_map_lock(&vm->vm_map);
	(void) vm_map_delete(&vm->vm_map, vm->vm_map.min_offset,
	    vm->vm_map.max_offset);
	vm_map_unlock(&vm->vm_map);

	pmap_release(vmspace_pmap(vm));
	zfree(vmspace_zone, vm);
}

void
vmspace_free(struct vmspace *vm)
{
	if (vm->vm_refcnt == 0)
		panic("vmspace_free: attempt to free already freed vmspace");

	if (--vm->vm_refcnt == 0 && vm->vm_exitingcnt == 0)
		vmspace_dofree(vm);
}

void
vmspace_exitfree(struct proc *p)
{
	struct vmspace *vm;

	vm = p->p_vmspace;
	p->p_vmspace = NULL;

	/*
	 * cleanup by parent process wait()ing on exiting child.  vm_refcnt
	 * may not be 0 (e.g. fork() and child exits without exec()ing).
	 * exitingcnt may increment above 0 and drop back down to zero
	 * several times while vm_refcnt is held non-zero.  vm_refcnt
	 * may also increment above 0 and drop back down to zero several
	 * times while vm_exitingcnt is held non-zero.
	 *
	 * The last wait on the exiting child's vmspace will clean up
	 * the remainder of the vmspace.
	 */
	if (--vm->vm_exitingcnt == 0 && vm->vm_refcnt == 0)
		vmspace_dofree(vm);
}

/*
 * vmspace_swap_count() - count the approximate swap useage in pages for a
 *			  vmspace.
 *
 *	Swap useage is determined by taking the proportional swap used by
 *	VM objects backing the VM map.  To make up for fractional losses,
 *	if the VM object has any swap use at all the associated map entries
 *	count for at least 1 swap page.
 */
int
vmspace_swap_count(struct vmspace *vmspace)
{
	vm_map_t map = &vmspace->vm_map;
	vm_map_entry_t cur;
	int count = 0;

	for (cur = map->header.next; cur != &map->header; cur = cur->next) {
		vm_object_t object;

		if ((cur->eflags & MAP_ENTRY_IS_SUB_MAP) == 0 &&
		    (object = cur->object.vm_object) != NULL &&
		    object->type == OBJT_SWAP
		) {
			int n = (cur->end - cur->start) / PAGE_SIZE;

			if (object->un_pager.swp.swp_bcount) {
				count += object->un_pager.swp.swp_bcount *
				    SWAP_META_PAGES * n / object->size + 1;
			}
		}
	}
	return(count);
}


/*
 *	vm_map_create:
 *
 *	Creates and returns a new empty VM map with
 *	the given physical map structure, and having
 *	the given lower and upper address bounds.
 */
vm_map_t
vm_map_create(pmap, min, max)
	pmap_t pmap;
	vm_offset_t min, max;
{
	vm_map_t result;

	result = zalloc(mapzone);
	vm_map_init(result, min, max);
	result->pmap = pmap;
	return (result);
}

/*
 * Initialize an existing vm_map structure
 * such as that in the vmspace structure.
 * The pmap is set elsewhere.
 */
void
vm_map_init(map, min, max)
	struct vm_map *map;
	vm_offset_t min, max;
{
	map->header.next = map->header.prev = &map->header;
	map->nentries = 0;
	map->size = 0;
	map->system_map = 0;
	map->infork = 0;
	map->min_offset = min;
	map->max_offset = max;
	map->first_free = &map->header;
	map->hint = &map->header;
	map->timestamp = 0;
	lockinit(&map->lock, PVM, "thrd_sleep", 0, LK_NOPAUSE);
}

/*
 *	vm_map_entry_dispose:	[ internal use only ]
 *
 *	Inverse of vm_map_entry_create.
 */
static void
vm_map_entry_dispose(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	if (map->system_map || !mapentzone)
		zfreei(kmapentzone, entry);
	else
		zfree(mapentzone, entry);
}

/*
 *	vm_map_entry_create:	[ internal use only ]
 *
 *	Allocates a VM map entry for insertion.
 *	No entry fields are filled in.  This routine is
 */
static vm_map_entry_t
vm_map_entry_create(map)
	vm_map_t map;
{
	vm_map_entry_t new_entry;

	if (map->system_map || !mapentzone)
		new_entry = zalloci(kmapentzone);
	else
		new_entry = zalloc(mapentzone);
	if (new_entry == NULL)
	    panic("vm_map_entry_create: kernel resources exhausted");
	return(new_entry);
}

/*
 *	vm_map_entry_{un,}link:
 *
 *	Insert/remove entries from maps.
 */
static __inline void
vm_map_entry_link(vm_map_t map,
		  vm_map_entry_t after_where,
		  vm_map_entry_t entry)
{
	map->nentries++;
	entry->prev = after_where;
	entry->next = after_where->next;
	entry->next->prev = entry;
	after_where->next = entry;
}

static __inline void
vm_map_entry_unlink(vm_map_t map,
		    vm_map_entry_t entry)
{
	vm_map_entry_t prev;
	vm_map_entry_t next;

	if (entry->eflags & MAP_ENTRY_IN_TRANSITION)
		panic("vm_map_entry_unlink: attempt to mess with locked entry! %p", entry);
	prev = entry->prev;
	next = entry->next;
	next->prev = prev;
	prev->next = next;
	map->nentries--;
}

/*
 *	SAVE_HINT:
 *
 *	Saves the specified entry as the hint for
 *	future lookups.
 */
#define	SAVE_HINT(map,value) \
		(map)->hint = (value);

/*
 *	vm_map_lookup_entry:	[ internal use only ]
 *
 *	Finds the map entry containing (or
 *	immediately preceding) the specified address
 *	in the given map; the entry is returned
 *	in the "entry" parameter.  The boolean
 *	result indicates whether the address is
 *	actually contained in the map.
 */
boolean_t
vm_map_lookup_entry(map, address, entry)
	vm_map_t map;
	vm_offset_t address;
	vm_map_entry_t *entry;	/* OUT */
{
	vm_map_entry_t cur;
	vm_map_entry_t last;

	/*
	 * Start looking either from the head of the list, or from the hint.
	 */

	cur = map->hint;

	if (cur == &map->header)
		cur = cur->next;

	if (address >= cur->start) {
		/*
		 * Go from hint to end of list.
		 *
		 * But first, make a quick check to see if we are already looking
		 * at the entry we want (which is usually the case). Note also
		 * that we don't need to save the hint here... it is the same
		 * hint (unless we are at the header, in which case the hint
		 * didn't buy us anything anyway).
		 */
		last = &map->header;
		if ((cur != last) && (cur->end > address)) {
			*entry = cur;
			return (TRUE);
		}
	} else {
		/*
		 * Go from start to hint, *inclusively*
		 */
		last = cur->next;
		cur = map->header.next;
	}

	/*
	 * Search linearly
	 */

	while (cur != last) {
		if (cur->end > address) {
			if (address >= cur->start) {
				/*
				 * Save this lookup for future hints, and
				 * return
				 */

				*entry = cur;
				SAVE_HINT(map, cur);
				return (TRUE);
			}
			break;
		}
		cur = cur->next;
	}
	*entry = cur->prev;
	SAVE_HINT(map, *entry);
	return (FALSE);
}

/*
 *	vm_map_insert:
 *
 *	Inserts the given whole VM object into the target
 *	map at the specified address range.  The object's
 *	size should match that of the address range.
 *
 *	Requires that the map be locked, and leaves it so.
 *
 *	If object is non-NULL, ref count must be bumped by caller
 *	prior to making call to account for the new entry.
 */
int
vm_map_insert(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
	      vm_offset_t start, vm_offset_t end, vm_prot_t prot, vm_prot_t max,
	      int cow)
{
	vm_map_entry_t new_entry;
	vm_map_entry_t prev_entry;
	vm_map_entry_t temp_entry;
	vm_eflags_t protoeflags;

	/*
	 * Check that the start and end points are not bogus.
	 */

	if ((start < map->min_offset) || (end > map->max_offset) ||
	    (start >= end))
		return (KERN_INVALID_ADDRESS);

	/*
	 * Find the entry prior to the proposed starting address; if it's part
	 * of an existing entry, this range is bogus.
	 */

	if (vm_map_lookup_entry(map, start, &temp_entry))
		return (KERN_NO_SPACE);

	prev_entry = temp_entry;

	/*
	 * Assert that the next entry doesn't overlap the end point.
	 */

	if ((prev_entry->next != &map->header) &&
	    (prev_entry->next->start < end))
		return (KERN_NO_SPACE);

	protoeflags = 0;

	if (cow & MAP_COPY_ON_WRITE)
		protoeflags |= MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY;

	if (cow & MAP_NOFAULT) {
		protoeflags |= MAP_ENTRY_NOFAULT;

		KASSERT(object == NULL,
			("vm_map_insert: paradoxical MAP_NOFAULT request"));
	}
	if (cow & MAP_DISABLE_SYNCER)
		protoeflags |= MAP_ENTRY_NOSYNC;
	if (cow & MAP_DISABLE_COREDUMP)
		protoeflags |= MAP_ENTRY_NOCOREDUMP;

	if (object) {
		/*
		 * When object is non-NULL, it could be shared with another
		 * process.  We have to set or clear OBJ_ONEMAPPING 
		 * appropriately.
		 */
		if ((object->ref_count > 1) || (object->shadow_count != 0)) {
			vm_object_clear_flag(object, OBJ_ONEMAPPING);
		}
	}
	else if ((prev_entry != &map->header) &&
		 (prev_entry->eflags == protoeflags) &&
		 (prev_entry->end == start) &&
		 (prev_entry->wired_count == 0) &&
		 ((prev_entry->object.vm_object == NULL) ||
		  vm_object_coalesce(prev_entry->object.vm_object,
				     OFF_TO_IDX(prev_entry->offset),
				     (vm_size_t)(prev_entry->end - prev_entry->start),
				     (vm_size_t)(end - prev_entry->end)))) {
		/*
		 * We were able to extend the object.  Determine if we
		 * can extend the previous map entry to include the 
		 * new range as well.
		 */
		if ((prev_entry->inheritance == VM_INHERIT_DEFAULT) &&
		    (prev_entry->protection == prot) &&
		    (prev_entry->max_protection == max)) {
			map->size += (end - prev_entry->end);
			prev_entry->end = end;
			vm_map_simplify_entry(map, prev_entry);
			return (KERN_SUCCESS);
		}

		/*
		 * If we can extend the object but cannot extend the
		 * map entry, we have to create a new map entry.  We
		 * must bump the ref count on the extended object to
		 * account for it.  object may be NULL.
		 */
		object = prev_entry->object.vm_object;
		offset = prev_entry->offset +
			(prev_entry->end - prev_entry->start);
		vm_object_reference(object);
	}

	/*
	 * NOTE: if conditionals fail, object can be NULL here.  This occurs
	 * in things like the buffer map where we manage kva but do not manage
	 * backing objects.
	 */

	/*
	 * Create a new entry
	 */

	new_entry = vm_map_entry_create(map);
	new_entry->start = start;
	new_entry->end = end;

	new_entry->eflags = protoeflags;
	new_entry->object.vm_object = object;
	new_entry->offset = offset;
	new_entry->avail_ssize = 0;

	new_entry->inheritance = VM_INHERIT_DEFAULT;
	new_entry->protection = prot;
	new_entry->max_protection = max;
	new_entry->wired_count = 0;

	/*
	 * Insert the new entry into the list
	 */

	vm_map_entry_link(map, prev_entry, new_entry);
	map->size += new_entry->end - new_entry->start;

	/*
	 * Update the free space hint
	 */
	if ((map->first_free == prev_entry) &&
	    (prev_entry->end >= new_entry->start)) {
		map->first_free = new_entry;
	}

#if 0
	/*
	 * Temporarily removed to avoid MAP_STACK panic, due to
	 * MAP_STACK being a huge hack.  Will be added back in
	 * when MAP_STACK (and the user stack mapping) is fixed.
	 */
	/*
	 * It may be possible to simplify the entry
	 */
	vm_map_simplify_entry(map, new_entry);
#endif

	if (cow & (MAP_PREFAULT|MAP_PREFAULT_PARTIAL)) {
		pmap_object_init_pt(map->pmap, start, prot,
				    object, OFF_TO_IDX(offset), end - start,
				    cow & MAP_PREFAULT_PARTIAL);
	}

	return (KERN_SUCCESS);
}

/*
 * Find sufficient space for `length' bytes in the given map, starting at
 * `start'.  The map must be locked.  Returns 0 on success, 1 on no space.
 */
int
vm_map_findspace(map, start, length, addr)
	vm_map_t map;
	vm_offset_t start;
	vm_size_t length;
	vm_offset_t *addr;
{
	vm_map_entry_t entry, next;
	vm_offset_t end;

	if (start < map->min_offset)
		start = map->min_offset;
	if (start > map->max_offset)
		return (1);

	/*
	 * Look for the first possible address; if there's already something
	 * at this address, we have to start after it.
	 */
	if (start == map->min_offset) {
		if ((entry = map->first_free) != &map->header)
			start = entry->end;
	} else {
		vm_map_entry_t tmp;

		if (vm_map_lookup_entry(map, start, &tmp))
			start = tmp->end;
		entry = tmp;
	}

	/*
	 * Look through the rest of the map, trying to fit a new region in the
	 * gap between existing regions, or after the very last region.
	 */
	for (;; start = (entry = next)->end) {
		/*
		 * Find the end of the proposed new region.  Be sure we didn't
		 * go beyond the end of the map, or wrap around the address;
		 * if so, we lose.  Otherwise, if this is the last entry, or
		 * if the proposed new region fits before the next entry, we
		 * win.
		 */
		end = start + length;
		if (end > map->max_offset || end < start)
			return (1);
		next = entry->next;
		if (next == &map->header || next->start >= end)
			break;
	}
	SAVE_HINT(map, entry);
	*addr = start;
	if (map == kernel_map) {
		vm_offset_t ksize;
		if ((ksize = round_page(start + length)) > kernel_vm_end) {
			pmap_growkernel(ksize);
		}
	}
	return (0);
}

/*
 *	vm_map_find finds an unallocated region in the target address
 *	map with the given length.  The search is defined to be
 *	first-fit from the specified address; the region found is
 *	returned in the same parameter.
 *
 *	If object is non-NULL, ref count must be bumped by caller
 *	prior to making call to account for the new entry.
 */
int
vm_map_find(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
	    vm_offset_t *addr,	/* IN/OUT */
	    vm_size_t length, boolean_t find_space, vm_prot_t prot,
	    vm_prot_t max, int cow)
{
	vm_offset_t start;
	int result, s = 0;

	start = *addr;

	if (map == kmem_map || map == mb_map)
		s = splvm();

	vm_map_lock(map);
	if (find_space) {
		if (vm_map_findspace(map, start, length, addr)) {
			vm_map_unlock(map);
			if (map == kmem_map || map == mb_map)
				splx(s);
			return (KERN_NO_SPACE);
		}
		start = *addr;
	}
	result = vm_map_insert(map, object, offset,
		start, start + length, prot, max, cow);
	vm_map_unlock(map);

	if (map == kmem_map || map == mb_map)
		splx(s);

	return (result);
}

/*
 *	vm_map_simplify_entry:
 *
 *	Simplify the given map entry by merging with either neighbor.  This
 *	routine also has the ability to merge with both neighbors.
 *
 *	The map must be locked.
 *
 *	This routine guarentees that the passed entry remains valid (though
 *	possibly extended).  When merging, this routine may delete one or
 *	both neighbors.  No action is taken on entries which have their
 *	in-transition flag set.
 */
void
vm_map_simplify_entry(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	vm_map_entry_t next, prev;
	vm_size_t prevsize, esize;

	if (entry->eflags & (MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_IS_SUB_MAP)) {
		++cnt.v_intrans_coll;
		return;
	}

	prev = entry->prev;
	if (prev != &map->header) {
		prevsize = prev->end - prev->start;
		if ( (prev->end == entry->start) &&
		     (prev->object.vm_object == entry->object.vm_object) &&
		     (!prev->object.vm_object ||
			(prev->offset + prevsize == entry->offset)) &&
		     (prev->eflags == entry->eflags) &&
		     (prev->protection == entry->protection) &&
		     (prev->max_protection == entry->max_protection) &&
		     (prev->inheritance == entry->inheritance) &&
		     (prev->wired_count == entry->wired_count)) {
			if (map->first_free == prev)
				map->first_free = entry;
			if (map->hint == prev)
				map->hint = entry;
			vm_map_entry_unlink(map, prev);
			entry->start = prev->start;
			entry->offset = prev->offset;
			if (prev->object.vm_object)
				vm_object_deallocate(prev->object.vm_object);
			vm_map_entry_dispose(map, prev);
		}
	}

	next = entry->next;
	if (next != &map->header) {
		esize = entry->end - entry->start;
		if ((entry->end == next->start) &&
		    (next->object.vm_object == entry->object.vm_object) &&
		     (!entry->object.vm_object ||
			(entry->offset + esize == next->offset)) &&
		    (next->eflags == entry->eflags) &&
		    (next->protection == entry->protection) &&
		    (next->max_protection == entry->max_protection) &&
		    (next->inheritance == entry->inheritance) &&
		    (next->wired_count == entry->wired_count)) {
			if (map->first_free == next)
				map->first_free = entry;
			if (map->hint == next)
				map->hint = entry;
			vm_map_entry_unlink(map, next);
			entry->end = next->end;
			if (next->object.vm_object)
				vm_object_deallocate(next->object.vm_object);
			vm_map_entry_dispose(map, next);
	        }
	}
}
/*
 *	vm_map_clip_start:	[ internal use only ]
 *
 *	Asserts that the given entry begins at or after
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
#define vm_map_clip_start(map, entry, startaddr) \
{ \
	if (startaddr > entry->start) \
		_vm_map_clip_start(map, entry, startaddr); \
}

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_start(map, entry, start)
	vm_map_t map;
	vm_map_entry_t entry;
	vm_offset_t start;
{
	vm_map_entry_t new_entry;

	/*
	 * Split off the front portion -- note that we must insert the new
	 * entry BEFORE this one, so that this entry has the specified
	 * starting address.
	 */

	vm_map_simplify_entry(map, entry);

	/*
	 * If there is no object backing this entry, we might as well create
	 * one now.  If we defer it, an object can get created after the map
	 * is clipped, and individual objects will be created for the split-up
	 * map.  This is a bit of a hack, but is also about the best place to
	 * put this improvement.
	 */

	if (entry->object.vm_object == NULL && !map->system_map) {
		vm_object_t object;
		object = vm_object_allocate(OBJT_DEFAULT,
				atop(entry->end - entry->start));
		entry->object.vm_object = object;
		entry->offset = 0;
	}

	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->end = start;
	entry->offset += (start - entry->start);
	entry->start = start;

	vm_map_entry_link(map, entry->prev, new_entry);

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_reference(new_entry->object.vm_object);
	}
}

/*
 *	vm_map_clip_end:	[ internal use only ]
 *
 *	Asserts that the given entry ends at or before
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */

#define vm_map_clip_end(map, entry, endaddr) \
{ \
	if (endaddr < entry->end) \
		_vm_map_clip_end(map, entry, endaddr); \
}

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_end(map, entry, end)
	vm_map_t map;
	vm_map_entry_t entry;
	vm_offset_t end;
{
	vm_map_entry_t new_entry;

	/*
	 * If there is no object backing this entry, we might as well create
	 * one now.  If we defer it, an object can get created after the map
	 * is clipped, and individual objects will be created for the split-up
	 * map.  This is a bit of a hack, but is also about the best place to
	 * put this improvement.
	 */

	if (entry->object.vm_object == NULL && !map->system_map) {
		vm_object_t object;
		object = vm_object_allocate(OBJT_DEFAULT,
				atop(entry->end - entry->start));
		entry->object.vm_object = object;
		entry->offset = 0;
	}

	/*
	 * Create a new entry and insert it AFTER the specified entry
	 */

	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->start = entry->end = end;
	new_entry->offset += (end - entry->start);

	vm_map_entry_link(map, entry, new_entry);

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_reference(new_entry->object.vm_object);
	}
}

/*
 *	VM_MAP_RANGE_CHECK:	[ internal use only ]
 *
 *	Asserts that the starting and ending region
 *	addresses fall within the valid range of the map.
 */
#define	VM_MAP_RANGE_CHECK(map, start, end)		\
		{					\
		if (start < vm_map_min(map))		\
			start = vm_map_min(map);	\
		if (end > vm_map_max(map))		\
			end = vm_map_max(map);		\
		if (start > end)			\
			start = end;			\
		}

/*
 *	vm_map_transition_wait:	[ kernel use only ]
 *
 *	Used to block when an in-transition collison occurs.  The map
 *	is unlocked for the sleep and relocked before the return.
 */
static
void
vm_map_transition_wait(vm_map_t map)
{
	vm_map_unlock(map);
	tsleep(map, PVM, "vment", 0);
	vm_map_lock(map);
}

/*
 * CLIP_CHECK_BACK
 * CLIP_CHECK_FWD
 *
 *	When we do blocking operations with the map lock held it is
 *	possible that a clip might have occured on our in-transit entry,
 *	requiring an adjustment to the entry in our loop.  These macros
 *	help the pageable and clip_range code deal with the case.  The
 *	conditional costs virtually nothing if no clipping has occured.
 */

#define CLIP_CHECK_BACK(entry, save_start)		\
    do {						\
	    while (entry->start != save_start) {	\
		    entry = entry->prev;		\
		    KASSERT(entry != &map->header, ("bad entry clip")); \
	    }						\
    } while(0)

#define CLIP_CHECK_FWD(entry, save_end)			\
    do {						\
	    while (entry->end != save_end) {		\
		    entry = entry->next;		\
		    KASSERT(entry != &map->header, ("bad entry clip")); \
	    }						\
    } while(0)


/*
 *	vm_map_clip_range:	[ kernel use only ]
 *
 *	Clip the specified range and return the base entry.  The
 *	range may cover several entries starting at the returned base
 *	and the first and last entry in the covering sequence will be
 *	properly clipped to the requested start and end address.
 *
 *	If no holes are allowed you should pass the MAP_CLIP_NO_HOLES
 *	flag.  
 *
 *	The MAP_ENTRY_IN_TRANSITION flag will be set for the entries
 *	covered by the requested range.
 *
 *	The map must be exclusively locked on entry and will remain locked
 *	on return. If no range exists or the range contains holes and you
 *	specified that no holes were allowed, NULL will be returned.  This
 *	routine may temporarily unlock the map in order avoid a deadlock when
 *	sleeping.
 */
static
vm_map_entry_t
vm_map_clip_range(vm_map_t map, vm_offset_t start, vm_offset_t end, int flags)
{
	vm_map_entry_t start_entry;
	vm_map_entry_t entry;

	/*
	 * Locate the entry and effect initial clipping.  The in-transition
	 * case does not occur very often so do not try to optimize it.
	 */
again:
	if (vm_map_lookup_entry(map, start, &start_entry) == FALSE)
		return (NULL);
	entry = start_entry;
	if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
		entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
		++cnt.v_intrans_coll;
		++cnt.v_intrans_wait;
		vm_map_transition_wait(map);
		/*
		 * entry and/or start_entry may have been clipped while
		 * we slept, or may have gone away entirely.  We have
		 * to restart from the lookup.
		 */
		goto again;
	}
	/*
	 * Since we hold an exclusive map lock we do not have to restart
	 * after clipping, even though clipping may block in zalloc.
	 */
	vm_map_clip_start(map, entry, start);
	vm_map_clip_end(map, entry, end);
	entry->eflags |= MAP_ENTRY_IN_TRANSITION;

	/*
	 * Scan entries covered by the range.  When working on the next
	 * entry a restart need only re-loop on the current entry which
	 * we have already locked, since 'next' may have changed.  Also,
	 * even though entry is safe, it may have been clipped so we
	 * have to iterate forwards through the clip after sleeping.
	 */
	while (entry->next != &map->header && entry->next->start < end) {
		vm_map_entry_t next = entry->next;

		if (flags & MAP_CLIP_NO_HOLES) {
			if (next->start > entry->end) {
				vm_map_unclip_range(map, start_entry,
					start, entry->end, flags);
				return(NULL);
			}
		}

		if (next->eflags & MAP_ENTRY_IN_TRANSITION) {
			vm_offset_t save_end = entry->end;
			next->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			++cnt.v_intrans_coll;
			++cnt.v_intrans_wait;
			vm_map_transition_wait(map);

			/*
			 * clips might have occured while we blocked.
			 */
			CLIP_CHECK_FWD(entry, save_end);
			CLIP_CHECK_BACK(start_entry, start);
			continue;
		}
		/*
		 * No restart necessary even though clip_end may block, we
		 * are holding the map lock.
		 */
		vm_map_clip_end(map, next, end);
		next->eflags |= MAP_ENTRY_IN_TRANSITION;
		entry = next;
	}
	if (flags & MAP_CLIP_NO_HOLES) {
		if (entry->end != end) {
			vm_map_unclip_range(map, start_entry,
				start, entry->end, flags);
			return(NULL);
		}
	}
	return(start_entry);
}

/*
 *	vm_map_unclip_range:	[ kernel use only ]
 *
 *	Undo the effect of vm_map_clip_range().  You should pass the same
 *	flags and the same range that you passed to vm_map_clip_range().
 *	This code will clear the in-transition flag on the entries and
 *	wake up anyone waiting.  This code will also simplify the sequence 
 *	and attempt to merge it with entries before and after the sequence.
 *
 *	The map must be locked on entry and will remain locked on return.
 *
 *	Note that you should also pass the start_entry returned by 
 *	vm_map_clip_range().  However, if you block between the two calls
 *	with the map unlocked please be aware that the start_entry may
 *	have been clipped and you may need to scan it backwards to find
 *	the entry corresponding with the original start address.  You are
 *	responsible for this, vm_map_unclip_range() expects the correct
 *	start_entry to be passed to it and will KASSERT otherwise.
 */
static
void
vm_map_unclip_range(
	vm_map_t map,
	vm_map_entry_t start_entry,
	vm_offset_t start,
	vm_offset_t end,
	int flags)
{
	vm_map_entry_t entry;

	entry = start_entry;

	KASSERT(entry->start == start, ("unclip_range: illegal base entry"));
	while (entry != &map->header && entry->start < end) {
		KASSERT(entry->eflags & MAP_ENTRY_IN_TRANSITION, ("in-transition flag not set during unclip on: %p", entry));
		KASSERT(entry->end <= end, ("unclip_range: tail wasn't clipped"));
		entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			wakeup(map);
		}
		entry = entry->next;
	}

	/*
	 * Simplification does not block so there is no restart case.
	 */
	entry = start_entry;
	while (entry != &map->header && entry->start < end) {
		vm_map_simplify_entry(map, entry);
		entry = entry->next;
	}
}

/*
 *	vm_map_submap:		[ kernel use only ]
 *
 *	Mark the given range as handled by a subordinate map.
 *
 *	This range must have been created with vm_map_find,
 *	and no other operations may have been performed on this
 *	range prior to calling vm_map_submap.
 *
 *	Only a limited number of operations can be performed
 *	within this rage after calling vm_map_submap:
 *		vm_fault
 *	[Don't try vm_map_copy!]
 *
 *	To remove a submapping, one must first remove the
 *	range from the superior map, and then destroy the
 *	submap (if desired).  [Better yet, don't try it.]
 */
int
vm_map_submap(map, start, end, submap)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t end;
	vm_map_t submap;
{
	vm_map_entry_t entry;
	int result = KERN_INVALID_ARGUMENT;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else {
		entry = entry->next;
	}

	vm_map_clip_end(map, entry, end);

	if ((entry->start == start) && (entry->end == end) &&
	    ((entry->eflags & MAP_ENTRY_COW) == 0) &&
	    (entry->object.vm_object == NULL)) {
		entry->object.sub_map = submap;
		entry->eflags |= MAP_ENTRY_IS_SUB_MAP;
		result = KERN_SUCCESS;
	}
	vm_map_unlock(map);

	return (result);
}

/*
 *	vm_map_protect:
 *
 *	Sets the protection of the specified address
 *	region in the target map.  If "set_max" is
 *	specified, the maximum protection is to be set;
 *	otherwise, only the current protection is affected.
 */
int
vm_map_protect(vm_map_t map, vm_offset_t start, vm_offset_t end,
	       vm_prot_t new_prot, boolean_t set_max)
{
	vm_map_entry_t current;
	vm_map_entry_t entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else {
		entry = entry->next;
	}

	/*
	 * Make a first pass to check for protection violations.
	 */

	current = entry;
	while ((current != &map->header) && (current->start < end)) {
		if (current->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_unlock(map);
			return (KERN_INVALID_ARGUMENT);
		}
		if ((new_prot & current->max_protection) != new_prot) {
			vm_map_unlock(map);
			return (KERN_PROTECTION_FAILURE);
		}
		current = current->next;
	}

	/*
	 * Go back and fix up protections. [Note that clipping is not
	 * necessary the second time.]
	 */

	current = entry;

	while ((current != &map->header) && (current->start < end)) {
		vm_prot_t old_prot;

		vm_map_clip_end(map, current, end);

		old_prot = current->protection;
		if (set_max)
			current->protection =
			    (current->max_protection = new_prot) &
			    old_prot;
		else
			current->protection = new_prot;

		/*
		 * Update physical map if necessary. Worry about copy-on-write
		 * here -- CHECK THIS XXX
		 */

		if (current->protection != old_prot) {
#define MASK(entry)	(((entry)->eflags & MAP_ENTRY_COW) ? ~VM_PROT_WRITE : \
							VM_PROT_ALL)

			pmap_protect(map->pmap, current->start,
			    current->end,
			    current->protection & MASK(current));
#undef	MASK
		}

		vm_map_simplify_entry(map, current);

		current = current->next;
	}

	vm_map_unlock(map);
	return (KERN_SUCCESS);
}

/*
 *	vm_map_madvise:
 *
 * 	This routine traverses a processes map handling the madvise
 *	system call.  Advisories are classified as either those effecting
 *	the vm_map_entry structure, or those effecting the underlying 
 *	objects.
 */

int
vm_map_madvise(map, start, end, behav)
	vm_map_t map;
	vm_offset_t start, end;
	int behav;
{
	vm_map_entry_t current, entry;
	int modify_map = 0;

	/*
	 * Some madvise calls directly modify the vm_map_entry, in which case
	 * we need to use an exclusive lock on the map and we need to perform 
	 * various clipping operations.  Otherwise we only need a read-lock
	 * on the map.
	 */

	switch(behav) {
	case MADV_NORMAL:
	case MADV_SEQUENTIAL:
	case MADV_RANDOM:
	case MADV_NOSYNC:
	case MADV_AUTOSYNC:
	case MADV_NOCORE:
	case MADV_CORE:
		modify_map = 1;
		vm_map_lock(map);
		break;
	case MADV_WILLNEED:
	case MADV_DONTNEED:
	case MADV_FREE:
		vm_map_lock_read(map);
		break;
	default:
		return (KERN_INVALID_ARGUMENT);
	}

	/*
	 * Locate starting entry and clip if necessary.
	 */

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		if (modify_map)
			vm_map_clip_start(map, entry, start);
	} else {
		entry = entry->next;
	}

	if (modify_map) {
		/*
		 * madvise behaviors that are implemented in the vm_map_entry.
		 *
		 * We clip the vm_map_entry so that behavioral changes are
		 * limited to the specified address range.
		 */
		for (current = entry;
		     (current != &map->header) && (current->start < end);
		     current = current->next
		) {
			if (current->eflags & MAP_ENTRY_IS_SUB_MAP)
				continue;

			vm_map_clip_end(map, current, end);

			switch (behav) {
			case MADV_NORMAL:
				vm_map_entry_set_behavior(current, MAP_ENTRY_BEHAV_NORMAL);
				break;
			case MADV_SEQUENTIAL:
				vm_map_entry_set_behavior(current, MAP_ENTRY_BEHAV_SEQUENTIAL);
				break;
			case MADV_RANDOM:
				vm_map_entry_set_behavior(current, MAP_ENTRY_BEHAV_RANDOM);
				break;
			case MADV_NOSYNC:
				current->eflags |= MAP_ENTRY_NOSYNC;
				break;
			case MADV_AUTOSYNC:
				current->eflags &= ~MAP_ENTRY_NOSYNC;
				break;
			case MADV_NOCORE:
				current->eflags |= MAP_ENTRY_NOCOREDUMP;
				break;
			case MADV_CORE:
				current->eflags &= ~MAP_ENTRY_NOCOREDUMP;
				break;
			default:
				break;
			}
			vm_map_simplify_entry(map, current);
		}
		vm_map_unlock(map);
	} else {
		vm_pindex_t pindex;
		int count;

		/*
		 * madvise behaviors that are implemented in the underlying
		 * vm_object.
		 *
		 * Since we don't clip the vm_map_entry, we have to clip
		 * the vm_object pindex and count.
		 */
		for (current = entry;
		     (current != &map->header) && (current->start < end);
		     current = current->next
		) {
			vm_offset_t useStart;

			if (current->eflags & MAP_ENTRY_IS_SUB_MAP)
				continue;

			pindex = OFF_TO_IDX(current->offset);
			count = atop(current->end - current->start);
			useStart = current->start;

			if (current->start < start) {
				pindex += atop(start - current->start);
				count -= atop(start - current->start);
				useStart = start;
			}
			if (current->end > end)
				count -= atop(current->end - end);

			if (count <= 0)
				continue;

			vm_object_madvise(current->object.vm_object,
					  pindex, count, behav);
			if (behav == MADV_WILLNEED) {
				pmap_object_init_pt(
				    map->pmap, 
				    useStart,
				    current->protection,
				    current->object.vm_object,
				    pindex, 
				    (count << PAGE_SHIFT),
				    MAP_PREFAULT_MADVISE
				);
			}
		}
		vm_map_unlock_read(map);
	}
	return(0);
}	


/*
 *	vm_map_inherit:
 *
 *	Sets the inheritance of the specified address
 *	range in the target map.  Inheritance
 *	affects how the map will be shared with
 *	child maps at the time of vm_map_fork.
 */
int
vm_map_inherit(vm_map_t map, vm_offset_t start, vm_offset_t end,
	       vm_inherit_t new_inheritance)
{
	vm_map_entry_t entry;
	vm_map_entry_t temp_entry;

	switch (new_inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
		break;
	default:
		return (KERN_INVALID_ARGUMENT);
	}

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &temp_entry)) {
		entry = temp_entry;
		vm_map_clip_start(map, entry, start);
	} else
		entry = temp_entry->next;

	while ((entry != &map->header) && (entry->start < end)) {
		vm_map_clip_end(map, entry, end);

		entry->inheritance = new_inheritance;

		vm_map_simplify_entry(map, entry);

		entry = entry->next;
	}

	vm_map_unlock(map);
	return (KERN_SUCCESS);
}

/*
 * Implement the semantics of mlock
 */
int
vm_map_user_pageable(map, start, real_end, new_pageable)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t real_end;
	boolean_t new_pageable;
{
	vm_map_entry_t entry;
	vm_map_entry_t start_entry;
	vm_offset_t end;
	int rv = KERN_SUCCESS;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, real_end);
	end = real_end;

	start_entry = vm_map_clip_range(map, start, end, MAP_CLIP_NO_HOLES);
	if (start_entry == NULL) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}

	if (new_pageable == 0) {
		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			vm_offset_t save_start;
			vm_offset_t save_end;

			/*
			 * Already user wired or hard wired (trivial cases)
			 */
			if (entry->eflags & MAP_ENTRY_USER_WIRED) {
				entry = entry->next;
				continue;
			}
			if (entry->wired_count != 0) {
				entry->wired_count++;
				entry->eflags |= MAP_ENTRY_USER_WIRED;
				entry = entry->next;
				continue;
			}

			/*
			 * A new wiring requires instantiation of appropriate
			 * management structures and the faulting in of the
			 * page.
			 */
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
				int copyflag = entry->eflags & MAP_ENTRY_NEEDS_COPY;
				if (copyflag && ((entry->protection & VM_PROT_WRITE) != 0)) {

					vm_object_shadow(&entry->object.vm_object,
					    &entry->offset,
					    atop(entry->end - entry->start));
					entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;

				} else if (entry->object.vm_object == NULL &&
					   !map->system_map) {

					entry->object.vm_object =
					    vm_object_allocate(OBJT_DEFAULT,
						atop(entry->end - entry->start));
					entry->offset = (vm_offset_t) 0;

				}
			}
			entry->wired_count++;
			entry->eflags |= MAP_ENTRY_USER_WIRED;

			/*
			 * Now fault in the area.  The map lock needs to be
			 * manipulated to avoid deadlocks.  The in-transition
			 * flag protects the entries. 
			 */
			save_start = entry->start;
			save_end = entry->end;
			vm_map_unlock(map);
			map->timestamp++;
			rv = vm_fault_user_wire(map, save_start, save_end);
			vm_map_lock(map);
			if (rv) {
				CLIP_CHECK_BACK(entry, save_start);
				for (;;) {
					KASSERT(entry->wired_count == 1, ("bad wired_count on entry"));
					entry->eflags &= ~MAP_ENTRY_USER_WIRED;
					entry->wired_count = 0;
					if (entry->end == save_end)
						break;
					entry = entry->next;
					KASSERT(entry != &map->header, ("bad entry clip during backout"));
				}
				end = save_start;	/* unwire the rest */
				break;
			}
			/*
			 * note that even though the entry might have been
			 * clipped, the USER_WIRED flag we set prevents
			 * duplication so we do not have to do a 
			 * clip check.
			 */
			entry = entry->next;
		}

		/*
		 * If we failed fall through to the unwiring section to
		 * unwire what we had wired so far.  'end' has already
		 * been adjusted.
		 */
		if (rv)
			new_pageable = 1;

		/*
		 * start_entry might have been clipped if we unlocked the
		 * map and blocked.  No matter how clipped it has gotten
		 * there should be a fragment that is on our start boundary.
		 */
		CLIP_CHECK_BACK(start_entry, start);
	}

	/*
	 * Deal with the unwiring case.
	 */
	if (new_pageable) {
		/*
		 * This is the unwiring case.  We must first ensure that the
		 * range to be unwired is really wired down.  We know there
		 * are no holes.
		 */
		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			if ((entry->eflags & MAP_ENTRY_USER_WIRED) == 0) {
				rv = KERN_INVALID_ARGUMENT;
				goto done;
			}
			KASSERT(entry->wired_count != 0, ("wired count was 0 with USER_WIRED set! %p", entry));
			entry = entry->next;
		}

		/*
		 * Now decrement the wiring count for each region. If a region
		 * becomes completely unwired, unwire its physical pages and
		 * mappings.
		 */
		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			KASSERT(entry->eflags & MAP_ENTRY_USER_WIRED, ("expected USER_WIRED on entry %p", entry));
			entry->eflags &= ~MAP_ENTRY_USER_WIRED;
			entry->wired_count--;
			if (entry->wired_count == 0)
				vm_fault_unwire(map, entry->start, entry->end);
			entry = entry->next;
		}
	}
done:
	vm_map_unclip_range(map, start_entry, start, real_end, 
		MAP_CLIP_NO_HOLES);
	map->timestamp++;
	vm_map_unlock(map);
	return (rv);
}

/*
 *	vm_map_pageable:
 *
 *	Sets the pageability of the specified address
 *	range in the target map.  Regions specified
 *	as not pageable require locked-down physical
 *	memory and physical page maps.
 *
 *	The map must not be locked, but a reference
 *	must remain to the map throughout the call.
 */
int
vm_map_pageable(map, start, real_end, new_pageable)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t real_end;
	boolean_t new_pageable;
{
	vm_map_entry_t entry;
	vm_map_entry_t start_entry;
	vm_offset_t end;
	int rv = KERN_SUCCESS;
	int s;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, real_end);
	end = real_end;

	start_entry = vm_map_clip_range(map, start, end, MAP_CLIP_NO_HOLES);
	if (start_entry == NULL) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}
	if (new_pageable == 0) {
		/*
		 * Wiring.  
		 *
		 * 1.  Holding the write lock, we create any shadow or zero-fill
		 * objects that need to be created. Then we clip each map
		 * entry to the region to be wired and increment its wiring
		 * count.  We create objects before clipping the map entries
		 * to avoid object proliferation.
		 *
		 * 2.  We downgrade to a read lock, and call vm_fault_wire to
		 * fault in the pages for any newly wired area (wired_count is
		 * 1).
		 *
		 * Downgrading to a read lock for vm_fault_wire avoids a 
		 * possible deadlock with another process that may have faulted
		 * on one of the pages to be wired (it would mark the page busy,
		 * blocking us, then in turn block on the map lock that we
		 * hold).  Because of problems in the recursive lock package,
		 * we cannot upgrade to a write lock in vm_map_lookup.  Thus,
		 * any actions that require the write lock must be done
		 * beforehand.  Because we keep the read lock on the map, the
		 * copy-on-write status of the entries we modify here cannot
		 * change.
		 */

		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			/*
			 * Trivial case if the entry is already wired
			 */
			if (entry->wired_count) {
				entry->wired_count++;
				entry = entry->next;
				continue;
			}

			/*
			 * The entry is being newly wired, we have to setup
			 * appropriate management structures.  A shadow 
			 * object is required for a copy-on-write region,
			 * or a normal object for a zero-fill region.  We
			 * do not have to do this for entries that point to sub
			 * maps because we won't hold the lock on the sub map.
			 */
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
				int copyflag = entry->eflags & MAP_ENTRY_NEEDS_COPY;
				if (copyflag &&
				    ((entry->protection & VM_PROT_WRITE) != 0)) {

					vm_object_shadow(&entry->object.vm_object,
					    &entry->offset,
					    atop(entry->end - entry->start));
					entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
				} else if (entry->object.vm_object == NULL &&
					   !map->system_map) {
					entry->object.vm_object =
					    vm_object_allocate(OBJT_DEFAULT,
						atop(entry->end - entry->start));
					entry->offset = (vm_offset_t) 0;
				}
			}

			entry->wired_count++;
			entry = entry->next;
		}

		/*
		 * Pass 2.
		 */

		/*
		 * HACK HACK HACK HACK
		 *
		 * Unlock the map to avoid deadlocks.  The in-transit flag
		 * protects us from most changes but note that
		 * clipping may still occur.  To prevent clipping from
		 * occuring after the unlock, except for when we are
		 * blocking in vm_fault_wire, we must run at splvm().
		 * Otherwise our accesses to entry->start and entry->end
		 * could be corrupted.  We have to set splvm() prior to
		 * unlocking so start_entry does not change out from
		 * under us at the very beginning of the loop.
		 *
		 * HACK HACK HACK HACK
		 */

		s = splvm();
		vm_map_unlock(map);

		entry = start_entry;
		while (entry != &map->header && entry->start < end) {
			/*
			 * If vm_fault_wire fails for any page we need to undo
			 * what has been done.  We decrement the wiring count
			 * for those pages which have not yet been wired (now)
			 * and unwire those that have (later).
			 */
			vm_offset_t save_start = entry->start;
			vm_offset_t save_end = entry->end;

			if (entry->wired_count == 1)
				rv = vm_fault_wire(map, entry->start, entry->end);
			if (rv) {
				CLIP_CHECK_BACK(entry, save_start);
				for (;;) {
					KASSERT(entry->wired_count == 1, ("wired_count changed unexpectedly"));
					entry->wired_count = 0;
					if (entry->end == save_end)
						break;
					entry = entry->next;
					KASSERT(entry != &map->header, ("bad entry clip during backout"));
				}
				end = save_start;
				break;
			}
			CLIP_CHECK_FWD(entry, save_end);
			entry = entry->next;
		}
		splx(s);

		/*
		 * relock.  start_entry is still IN_TRANSITION and must
		 * still exist, but may have been clipped (handled just
		 * below).
		 */
		vm_map_lock(map);

		/*
		 * If a failure occured undo everything by falling through
		 * to the unwiring code.  'end' has already been adjusted
		 * appropriately.
		 */
		if (rv)
			new_pageable = 1;

		/*
		 * start_entry might have been clipped if we unlocked the
		 * map and blocked.  No matter how clipped it has gotten
		 * there should be a fragment that is on our start boundary.
		 */
		CLIP_CHECK_BACK(start_entry, start);
	}

	if (new_pageable) {
		/*
		 * This is the unwiring case.  We must first ensure that the
		 * range to be unwired is really wired down.  We know there
		 * are no holes.
		 */
		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			if (entry->wired_count == 0) {
				rv = KERN_INVALID_ARGUMENT;
				goto done;
			}
			entry = entry->next;
		}

		/*
		 * Now decrement the wiring count for each region. If a region
		 * becomes completely unwired, unwire its physical pages and
		 * mappings.
		 */
		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			entry->wired_count--;
			if (entry->wired_count == 0)
				vm_fault_unwire(map, entry->start, entry->end);
			entry = entry->next;
		}
	}
done:
	vm_map_unclip_range(map, start_entry, start, real_end, 
		MAP_CLIP_NO_HOLES);
	map->timestamp++;
	vm_map_unlock(map);
	return (rv);
}

/*
 * vm_map_clean
 *
 * Push any dirty cached pages in the address range to their pager.
 * If syncio is TRUE, dirty pages are written synchronously.
 * If invalidate is TRUE, any cached pages are freed as well.
 *
 * Returns an error if any part of the specified range is not mapped.
 */
int
vm_map_clean(map, start, end, syncio, invalidate)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t end;
	boolean_t syncio;
	boolean_t invalidate;
{
	vm_map_entry_t current;
	vm_map_entry_t entry;
	vm_size_t size;
	vm_object_t object;
	vm_ooffset_t offset;

	vm_map_lock_read(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &entry)) {
		vm_map_unlock_read(map);
		return (KERN_INVALID_ADDRESS);
	}
	/*
	 * Make a first pass to check for holes.
	 */
	for (current = entry; current->start < end; current = current->next) {
		if (current->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_unlock_read(map);
			return (KERN_INVALID_ARGUMENT);
		}
		if (end > current->end &&
		    (current->next == &map->header ||
			current->end != current->next->start)) {
			vm_map_unlock_read(map);
			return (KERN_INVALID_ADDRESS);
		}
	}

	if (invalidate)
		pmap_remove(vm_map_pmap(map), start, end);
	/*
	 * Make a second pass, cleaning/uncaching pages from the indicated
	 * objects as we go.
	 */
	for (current = entry; current->start < end; current = current->next) {
		offset = current->offset + (start - current->start);
		size = (end <= current->end ? end : current->end) - start;
		if (current->eflags & MAP_ENTRY_IS_SUB_MAP) {
			vm_map_t smap;
			vm_map_entry_t tentry;
			vm_size_t tsize;

			smap = current->object.sub_map;
			vm_map_lock_read(smap);
			(void) vm_map_lookup_entry(smap, offset, &tentry);
			tsize = tentry->end - offset;
			if (tsize < size)
				size = tsize;
			object = tentry->object.vm_object;
			offset = tentry->offset + (offset - tentry->start);
			vm_map_unlock_read(smap);
		} else {
			object = current->object.vm_object;
		}
		/*
		 * Note that there is absolutely no sense in writing out
		 * anonymous objects, so we track down the vnode object
		 * to write out.
		 * We invalidate (remove) all pages from the address space
		 * anyway, for semantic correctness.
		 *
		 * note: certain anonymous maps, such as MAP_NOSYNC maps,
		 * may start out with a NULL object.
		 */
		while (object && object->backing_object) {
			object = object->backing_object;
			offset += object->backing_object_offset;
			if (object->size < OFF_TO_IDX( offset + size))
				size = IDX_TO_OFF(object->size) - offset;
		}
		if (object && (object->type == OBJT_VNODE) && 
		    (current->protection & VM_PROT_WRITE)) {
			/*
			 * Flush pages if writing is allowed, invalidate them
			 * if invalidation requested.  Pages undergoing I/O
			 * will be ignored by vm_object_page_remove().
			 *
			 * We cannot lock the vnode and then wait for paging
			 * to complete without deadlocking against vm_fault.
			 * Instead we simply call vm_object_page_remove() and
			 * allow it to block internally on a page-by-page 
			 * basis when it encounters pages undergoing async 
			 * I/O.
			 */
			int flags;

			vm_object_reference(object);
			vn_lock(object->handle, LK_EXCLUSIVE | LK_RETRY, curproc);
			flags = (syncio || invalidate) ? OBJPC_SYNC : 0;
			flags |= invalidate ? OBJPC_INVAL : 0;
			vm_object_page_clean(object,
			    OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + size + PAGE_MASK),
			    flags);
			VOP_UNLOCK(object->handle, 0, curproc);
			vm_object_deallocate(object);
		}
		if (object && invalidate &&
		   ((object->type == OBJT_VNODE) ||
		    (object->type == OBJT_DEVICE))) {
			vm_object_reference(object);
			vm_object_page_remove(object,
			    OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + size + PAGE_MASK),
			    old_msync ? FALSE : TRUE);
			vm_object_deallocate(object);
		}
		start += size;
	}

	vm_map_unlock_read(map);
	return (KERN_SUCCESS);
}

/*
 *	vm_map_entry_unwire:	[ internal use only ]
 *
 *	Make the region specified by this entry pageable.
 *
 *	The map in question should be locked.
 *	[This is the reason for this routine's existence.]
 */
static void 
vm_map_entry_unwire(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	vm_fault_unwire(map, entry->start, entry->end);
	entry->wired_count = 0;
}

/*
 *	vm_map_entry_delete:	[ internal use only ]
 *
 *	Deallocate the given entry from the target map.
 */
static void
vm_map_entry_delete(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	vm_map_entry_unlink(map, entry);
	map->size -= entry->end - entry->start;

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_deallocate(entry->object.vm_object);
	}

	vm_map_entry_dispose(map, entry);
}

/*
 *	vm_map_delete:	[ internal use only ]
 *
 *	Deallocates the given address range from the target
 *	map.
 */
int
vm_map_delete(map, start, end)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t end;
{
	vm_object_t object;
	vm_map_entry_t entry;
	vm_map_entry_t first_entry;

	/*
	 * Find the start of the region, and clip it
	 */

again:
	if (!vm_map_lookup_entry(map, start, &first_entry))
		entry = first_entry->next;
	else {
		entry = first_entry;
		vm_map_clip_start(map, entry, start);
		/*
		 * Fix the lookup hint now, rather than each time though the
		 * loop.
		 */
		SAVE_HINT(map, entry->prev);
	}

	/*
	 * Save the free space hint
	 */

	if (entry == &map->header) {
		map->first_free = &map->header;
	} else if (map->first_free->start >= start) {
		map->first_free = entry->prev;
	}

	/*
	 * Step through all entries in this region
	 */

	while ((entry != &map->header) && (entry->start < end)) {
		vm_map_entry_t next;
		vm_offset_t s, e;
		vm_pindex_t offidxstart, offidxend, count;

		/*
		 * If we hit an in-transition entry we have to sleep and
		 * retry.  It's easier (and not really slower) to just retry
		 * since this case occurs so rarely and the hint is already
		 * pointing at the right place.  We have to reset the
		 * start offset so as not to accidently delete an entry
		 * another process just created in vacated space.
		 */
		if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
			entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			start = entry->start;
			++cnt.v_intrans_coll;
			++cnt.v_intrans_wait;
			vm_map_transition_wait(map);
			goto again;
		}
		vm_map_clip_end(map, entry, end);

		s = entry->start;
		e = entry->end;
		next = entry->next;

		offidxstart = OFF_TO_IDX(entry->offset);
		count = OFF_TO_IDX(e - s);
		object = entry->object.vm_object;

		/*
		 * Unwire before removing addresses from the pmap; otherwise,
		 * unwiring will put the entries back in the pmap.
		 */
		if (entry->wired_count != 0) {
			vm_map_entry_unwire(map, entry);
		}

		offidxend = offidxstart + count;

		if ((object == kernel_object) || (object == kmem_object)) {
			vm_object_page_remove(object, offidxstart, offidxend, FALSE);
		} else {
			pmap_remove(map->pmap, s, e);
			if (object != NULL &&
			    object->ref_count != 1 &&
			    (object->flags & (OBJ_NOSPLIT|OBJ_ONEMAPPING)) == OBJ_ONEMAPPING &&
			    (object->type == OBJT_DEFAULT || object->type == OBJT_SWAP)) {
				vm_object_collapse(object);
				vm_object_page_remove(object, offidxstart, offidxend, FALSE);
				if (object->type == OBJT_SWAP) {
					swap_pager_freespace(object, offidxstart, count);
				}
				if (offidxend >= object->size &&
				    offidxstart < object->size) {
					object->size = offidxstart;
				}
			}
		}

		/*
		 * Delete the entry (which may delete the object) only after
		 * removing all pmap entries pointing to its pages.
		 * (Otherwise, its page frames may be reallocated, and any
		 * modify bits will be set in the wrong object!)
		 */
		vm_map_entry_delete(map, entry);
		entry = next;
	}
	return (KERN_SUCCESS);
}

/*
 *	vm_map_remove:
 *
 *	Remove the given address range from the target map.
 *	This is the exported form of vm_map_delete.
 */
int
vm_map_remove(map, start, end)
	vm_map_t map;
	vm_offset_t start;
	vm_offset_t end;
{
	int result, s = 0;

	if (map == kmem_map || map == mb_map)
		s = splvm();

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	result = vm_map_delete(map, start, end);
	vm_map_unlock(map);

	if (map == kmem_map || map == mb_map)
		splx(s);

	return (result);
}

/*
 *	vm_map_check_protection:
 *
 *	Assert that the target map allows the specified
 *	privilege on the entire address region given.
 *	The entire region must be allocated.
 */
boolean_t
vm_map_check_protection(vm_map_t map, vm_offset_t start, vm_offset_t end,
			vm_prot_t protection)
{
	vm_map_entry_t entry;
	vm_map_entry_t tmp_entry;

	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		return (FALSE);
	}
	entry = tmp_entry;

	while (start < end) {
		if (entry == &map->header) {
			return (FALSE);
		}
		/*
		 * No holes allowed!
		 */

		if (start < entry->start) {
			return (FALSE);
		}
		/*
		 * Check protection associated with entry.
		 */

		if ((entry->protection & protection) != protection) {
			return (FALSE);
		}
		/* go to next entry */

		start = entry->end;
		entry = entry->next;
	}
	return (TRUE);
}

/*
 * Split the pages in a map entry into a new object.  This affords
 * easier removal of unused pages, and keeps object inheritance from
 * being a negative impact on memory usage.
 */
static void
vm_map_split(entry)
	vm_map_entry_t entry;
{
	vm_page_t m;
	vm_object_t orig_object, new_object, source;
	vm_offset_t s, e;
	vm_pindex_t offidxstart, offidxend, idx;
	vm_size_t size;
	vm_ooffset_t offset;

	orig_object = entry->object.vm_object;
	if (orig_object->type != OBJT_DEFAULT && orig_object->type != OBJT_SWAP)
		return;
	if (orig_object->ref_count <= 1)
		return;

	offset = entry->offset;
	s = entry->start;
	e = entry->end;

	offidxstart = OFF_TO_IDX(offset);
	offidxend = offidxstart + OFF_TO_IDX(e - s);
	size = offidxend - offidxstart;

	new_object = vm_pager_allocate(orig_object->type,
		NULL, IDX_TO_OFF(size), VM_PROT_ALL, 0LL);
	if (new_object == NULL)
		return;

	source = orig_object->backing_object;
	if (source != NULL) {
		vm_object_reference(source);	/* Referenced by new_object */
		LIST_INSERT_HEAD(&source->shadow_head,
				  new_object, shadow_list);
		vm_object_clear_flag(source, OBJ_ONEMAPPING);
		new_object->backing_object_offset = 
			orig_object->backing_object_offset + IDX_TO_OFF(offidxstart);
		new_object->backing_object = source;
		source->shadow_count++;
		source->generation++;
	}

	for (idx = 0; idx < size; idx++) {
		vm_page_t m;

	retry:
		m = vm_page_lookup(orig_object, offidxstart + idx);
		if (m == NULL)
			continue;

		/*
		 * We must wait for pending I/O to complete before we can
		 * rename the page.
		 *
		 * We do not have to VM_PROT_NONE the page as mappings should
		 * not be changed by this operation.
		 */
		if (vm_page_sleep_busy(m, TRUE, "spltwt"))
			goto retry;
			
		vm_page_busy(m);
		vm_page_rename(m, new_object, idx);
		/* page automatically made dirty by rename and cache handled */
		vm_page_busy(m);
	}

	if (orig_object->type == OBJT_SWAP) {
		vm_object_pip_add(orig_object, 1);
		/*
		 * copy orig_object pages into new_object
		 * and destroy unneeded pages in
		 * shadow object.
		 */
		swap_pager_copy(orig_object, new_object, offidxstart, 0);
		vm_object_pip_wakeup(orig_object);
	}

	for (idx = 0; idx < size; idx++) {
		m = vm_page_lookup(new_object, idx);
		if (m) {
			vm_page_wakeup(m);
		}
	}

	entry->object.vm_object = new_object;
	entry->offset = 0LL;
	vm_object_deallocate(orig_object);
}

/*
 *	vm_map_copy_entry:
 *
 *	Copies the contents of the source entry to the destination
 *	entry.  The entries *must* be aligned properly.
 */
static void
vm_map_copy_entry(src_map, dst_map, src_entry, dst_entry)
	vm_map_t src_map, dst_map;
	vm_map_entry_t src_entry, dst_entry;
{
	vm_object_t src_object;

	if ((dst_entry->eflags|src_entry->eflags) & MAP_ENTRY_IS_SUB_MAP)
		return;

	if (src_entry->wired_count == 0) {

		/*
		 * If the source entry is marked needs_copy, it is already
		 * write-protected.
		 */
		if ((src_entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) {
			pmap_protect(src_map->pmap,
			    src_entry->start,
			    src_entry->end,
			    src_entry->protection & ~VM_PROT_WRITE);
		}

		/*
		 * Make a copy of the object.
		 */
		if ((src_object = src_entry->object.vm_object) != NULL) {

			if ((src_object->handle == NULL) &&
				(src_object->type == OBJT_DEFAULT ||
				 src_object->type == OBJT_SWAP)) {
				vm_object_collapse(src_object);
				if ((src_object->flags & (OBJ_NOSPLIT|OBJ_ONEMAPPING)) == OBJ_ONEMAPPING) {
					vm_map_split(src_entry);
					src_object = src_entry->object.vm_object;
				}
			}

			vm_object_reference(src_object);
			vm_object_clear_flag(src_object, OBJ_ONEMAPPING);
			dst_entry->object.vm_object = src_object;
			src_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->offset = src_entry->offset;
		} else {
			dst_entry->object.vm_object = NULL;
			dst_entry->offset = 0;
		}

		pmap_copy(dst_map->pmap, src_map->pmap, dst_entry->start,
		    dst_entry->end - dst_entry->start, src_entry->start);
	} else {
		/*
		 * Of course, wired down pages can't be set copy-on-write.
		 * Cause wired pages to be copied into the new map by
		 * simulating faults (the new pages are pageable)
		 */
		vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry);
	}
}

/*
 * vmspace_fork:
 * Create a new process vmspace structure and vm_map
 * based on those of an existing process.  The new map
 * is based on the old map, according to the inheritance
 * values on the regions in that map.
 *
 * The source map must not be locked.
 */
struct vmspace *
vmspace_fork(vm1)
	struct vmspace *vm1;
{
	struct vmspace *vm2;
	vm_map_t old_map = &vm1->vm_map;
	vm_map_t new_map;
	vm_map_entry_t old_entry;
	vm_map_entry_t new_entry;
	vm_object_t object;

	vm_map_lock(old_map);
	old_map->infork = 1;

	vm2 = vmspace_alloc(old_map->min_offset, old_map->max_offset);
	bcopy(&vm1->vm_startcopy, &vm2->vm_startcopy,
	    (caddr_t) &vm1->vm_endcopy - (caddr_t) &vm1->vm_startcopy);
	new_map = &vm2->vm_map;	/* XXX */
	new_map->timestamp = 1;

	old_entry = old_map->header.next;

	while (old_entry != &old_map->header) {
		if (old_entry->eflags & MAP_ENTRY_IS_SUB_MAP)
			panic("vm_map_fork: encountered a submap");

		switch (old_entry->inheritance) {
		case VM_INHERIT_NONE:
			break;

		case VM_INHERIT_SHARE:
			/*
			 * Clone the entry, creating the shared object if necessary.
			 */
			object = old_entry->object.vm_object;
			if (object == NULL) {
				object = vm_object_allocate(OBJT_DEFAULT,
					atop(old_entry->end - old_entry->start));
				old_entry->object.vm_object = object;
				old_entry->offset = (vm_offset_t) 0;
			}

			/*
			 * Add the reference before calling vm_object_shadow
			 * to insure that a shadow object is created.
			 */
			vm_object_reference(object);
			if (old_entry->eflags & MAP_ENTRY_NEEDS_COPY) {
				vm_object_shadow(&old_entry->object.vm_object,
					&old_entry->offset,
					atop(old_entry->end - old_entry->start));
				old_entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
				/* Transfer the second reference too. */
				vm_object_reference(
				    old_entry->object.vm_object);
				vm_object_deallocate(object);
				object = old_entry->object.vm_object;
			}
			vm_object_clear_flag(object, OBJ_ONEMAPPING);

			/*
			 * Clone the entry, referencing the shared object.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->eflags &= ~MAP_ENTRY_USER_WIRED;
			new_entry->wired_count = 0;

			/*
			 * Insert the entry into the new map -- we know we're
			 * inserting at the end of the new map.
			 */

			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);

			/*
			 * Update the physical map
			 */

			pmap_copy(new_map->pmap, old_map->pmap,
			    new_entry->start,
			    (old_entry->end - old_entry->start),
			    old_entry->start);
			break;

		case VM_INHERIT_COPY:
			/*
			 * Clone the entry and link into the map.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->eflags &= ~MAP_ENTRY_USER_WIRED;
			new_entry->wired_count = 0;
			new_entry->object.vm_object = NULL;
			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);
			vm_map_copy_entry(old_map, new_map, old_entry,
			    new_entry);
			break;
		}
		old_entry = old_entry->next;
	}

	new_map->size = old_map->size;
	old_map->infork = 0;
	vm_map_unlock(old_map);

	return (vm2);
}

int
vm_map_stack (vm_map_t map, vm_offset_t addrbos, vm_size_t max_ssize,
	      vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_map_entry_t prev_entry;
	vm_map_entry_t new_stack_entry;
	vm_size_t      init_ssize;
	int            rv;

	if (VM_MIN_ADDRESS > 0 && addrbos < VM_MIN_ADDRESS)
		return (KERN_NO_SPACE);
	if (addrbos > map->max_offset)
		return (KERN_NO_SPACE);
	if (max_ssize < sgrowsiz)
		init_ssize = max_ssize;
	else
		init_ssize = sgrowsiz;

	vm_map_lock(map);

	/* If addr is already mapped, no go */
	if (vm_map_lookup_entry(map, addrbos, &prev_entry)) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	/* If we would blow our VMEM resource limit, no go */
	if (map->size + init_ssize >
	    curproc->p_rlimit[RLIMIT_VMEM].rlim_cur) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	/* If we can't accomodate max_ssize in the current mapping,
	 * no go.  However, we need to be aware that subsequent user
	 * mappings might map into the space we have reserved for
	 * stack, and currently this space is not protected.  
	 * 
	 * Hopefully we will at least detect this condition 
	 * when we try to grow the stack.
	 */
	if ((prev_entry->next != &map->header) &&
	    (prev_entry->next->start < addrbos + max_ssize)) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	/* We initially map a stack of only init_ssize.  We will
	 * grow as needed later.  Since this is to be a grow 
	 * down stack, we map at the top of the range.
	 *
	 * Note: we would normally expect prot and max to be
	 * VM_PROT_ALL, and cow to be 0.  Possibly we should
	 * eliminate these as input parameters, and just
	 * pass these values here in the insert call.
	 */
	rv = vm_map_insert(map, NULL, 0, addrbos + max_ssize - init_ssize,
	                   addrbos + max_ssize, prot, max, cow);

	/* Now set the avail_ssize amount */
	if (rv == KERN_SUCCESS){
		if (prev_entry != &map->header)
			vm_map_clip_end(map, prev_entry, addrbos + max_ssize - init_ssize);
		new_stack_entry = prev_entry->next;
		if (new_stack_entry->end   != addrbos + max_ssize ||
		    new_stack_entry->start != addrbos + max_ssize - init_ssize)
			panic ("Bad entry start/end for new stack entry");
		else 
			new_stack_entry->avail_ssize = max_ssize - init_ssize;
	}

	vm_map_unlock(map);
	return (rv);
}

/* Attempts to grow a vm stack entry.  Returns KERN_SUCCESS if the
 * desired address is already mapped, or if we successfully grow
 * the stack.  Also returns KERN_SUCCESS if addr is outside the
 * stack range (this is strange, but preserves compatibility with
 * the grow function in vm_machdep.c).
 */
int
vm_map_growstack (struct proc *p, vm_offset_t addr)
{
	vm_map_entry_t prev_entry;
	vm_map_entry_t stack_entry;
	vm_map_entry_t new_stack_entry;
	struct vmspace *vm = p->p_vmspace;
	vm_map_t map = &vm->vm_map;
	vm_offset_t    end;
	int      grow_amount;
	int      rv = KERN_SUCCESS;
	int      is_procstack;
	int	 use_read_lock = 1;

Retry:
	if (use_read_lock)
		vm_map_lock_read(map);
	else
		vm_map_lock(map);

	/* If addr is already in the entry range, no need to grow.*/
	if (vm_map_lookup_entry(map, addr, &prev_entry))
		goto done;

	if ((stack_entry = prev_entry->next) == &map->header)
		goto done;
	if (prev_entry == &map->header) 
		end = stack_entry->start - stack_entry->avail_ssize;
	else
		end = prev_entry->end;

	/* This next test mimics the old grow function in vm_machdep.c.
	 * It really doesn't quite make sense, but we do it anyway
	 * for compatibility.
	 *
	 * If not growable stack, return success.  This signals the
	 * caller to proceed as he would normally with normal vm.
	 */
	if (stack_entry->avail_ssize < 1 ||
	    addr >= stack_entry->start ||
	    addr <  stack_entry->start - stack_entry->avail_ssize) {
		goto done;
	} 
	
	/* Find the minimum grow amount */
	grow_amount = roundup (stack_entry->start - addr, PAGE_SIZE);
	if (grow_amount > stack_entry->avail_ssize) {
		rv = KERN_NO_SPACE;
		goto done;
	}

	/* If there is no longer enough space between the entries
	 * nogo, and adjust the available space.  Note: this 
	 * should only happen if the user has mapped into the
	 * stack area after the stack was created, and is
	 * probably an error.
	 *
	 * This also effectively destroys any guard page the user
	 * might have intended by limiting the stack size.
	 */
	if (grow_amount > stack_entry->start - end) {
		if (use_read_lock && vm_map_lock_upgrade(map)) {
			use_read_lock = 0;
			goto Retry;
		}
		use_read_lock = 0;
		stack_entry->avail_ssize = stack_entry->start - end;
		rv = KERN_NO_SPACE;
		goto done;
	}

	is_procstack = addr >= (vm_offset_t)vm->vm_maxsaddr;

	/* If this is the main process stack, see if we're over the 
	 * stack limit.
	 */
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount >
			     p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
		rv = KERN_NO_SPACE;
		goto done;
	}

	/* Round up the grow amount modulo SGROWSIZ */
	grow_amount = roundup (grow_amount, sgrowsiz);
	if (grow_amount > stack_entry->avail_ssize) {
		grow_amount = stack_entry->avail_ssize;
	}
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount >
	                     p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
		grow_amount = p->p_rlimit[RLIMIT_STACK].rlim_cur -
		              ctob(vm->vm_ssize);
	}

	/* If we would blow our VMEM resource limit, no go */
	if (map->size + grow_amount >
	    curproc->p_rlimit[RLIMIT_VMEM].rlim_cur) {
		rv = KERN_NO_SPACE;
		goto done;
	}

	if (use_read_lock && vm_map_lock_upgrade(map)) {
		use_read_lock = 0;
		goto Retry;
	}
	use_read_lock = 0;

	/* Get the preliminary new entry start value */
	addr = stack_entry->start - grow_amount;

	/* If this puts us into the previous entry, cut back our growth
	 * to the available space.  Also, see the note above.
	 */
	if (addr < end) {
		stack_entry->avail_ssize = stack_entry->start - end;
		addr = end;
	}

	rv = vm_map_insert(map, NULL, 0, addr, stack_entry->start,
			   VM_PROT_ALL,
			   VM_PROT_ALL,
			   0);

	/* Adjust the available stack space by the amount we grew. */
	if (rv == KERN_SUCCESS) {
		if (prev_entry != &map->header)
			vm_map_clip_end(map, prev_entry, addr);
		new_stack_entry = prev_entry->next;
		if (new_stack_entry->end   != stack_entry->start  ||
		    new_stack_entry->start != addr)
			panic ("Bad stack grow start/end in new stack entry");
		else {
			new_stack_entry->avail_ssize = stack_entry->avail_ssize -
							(new_stack_entry->end -
							 new_stack_entry->start);
			if (is_procstack)
				vm->vm_ssize += btoc(new_stack_entry->end -
						     new_stack_entry->start);
		}
	}

done:
	if (use_read_lock)
		vm_map_unlock_read(map);
	else
		vm_map_unlock(map);
	return (rv);
}

/*
 * Unshare the specified VM space for exec.  If other processes are
 * mapped to it, then create a new one.  The new vmspace is null.
 */

void
vmspace_exec(struct proc *p) {
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;
	vm_map_t map = &p->p_vmspace->vm_map;

	newvmspace = vmspace_alloc(map->min_offset, map->max_offset);
	bcopy(&oldvmspace->vm_startcopy, &newvmspace->vm_startcopy,
	    (caddr_t) &newvmspace->vm_endcopy -
	    (caddr_t) &newvmspace->vm_startcopy);
	/*
	 * This code is written like this for prototype purposes.  The
	 * goal is to avoid running down the vmspace here, but let the
	 * other process's that are still using the vmspace to finally
	 * run it down.  Even though there is little or no chance of blocking
	 * here, it is a good idea to keep this form for future mods.
	 */
	vmspace_free(oldvmspace);
	p->p_vmspace = newvmspace;
	pmap_pinit2(vmspace_pmap(newvmspace));
	if (p == curproc)
		pmap_activate(p);
}

/*
 * Unshare the specified VM space for forcing COW.  This
 * is called by rfork, for the (RFMEM|RFPROC) == 0 case.
 */

void
vmspace_unshare(struct proc *p) {
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;

	if (oldvmspace->vm_refcnt == 1)
		return;
	newvmspace = vmspace_fork(oldvmspace);
	vmspace_free(oldvmspace);
	p->p_vmspace = newvmspace;
	pmap_pinit2(vmspace_pmap(newvmspace));
	if (p == curproc)
		pmap_activate(p);
}
	

/*
 *	vm_map_lookup:
 *
 *	Finds the VM object, offset, and
 *	protection for a given virtual address in the
 *	specified map, assuming a page fault of the
 *	type specified.
 *
 *	Leaves the map in question locked for read; return
 *	values are guaranteed until a vm_map_lookup_done
 *	call is performed.  Note that the map argument
 *	is in/out; the returned map must be used in
 *	the call to vm_map_lookup_done.
 *
 *	A handle (out_entry) is returned for use in
 *	vm_map_lookup_done, to make that fast.
 *
 *	If a lookup is requested with "write protection"
 *	specified, the map may be changed to perform virtual
 *	copying operations, although the data referenced will
 *	remain the same.
 */
int
vm_map_lookup(vm_map_t *var_map,		/* IN/OUT */
	      vm_offset_t vaddr,
	      vm_prot_t fault_typea,
	      vm_map_entry_t *out_entry,	/* OUT */
	      vm_object_t *object,		/* OUT */
	      vm_pindex_t *pindex,		/* OUT */
	      vm_prot_t *out_prot,		/* OUT */
	      boolean_t *wired)			/* OUT */
{
	vm_map_entry_t entry;
	vm_map_t map = *var_map;
	vm_prot_t prot;
	vm_prot_t fault_type = fault_typea;
	int use_read_lock = 1;
	int rv = KERN_SUCCESS;

RetryLookup:
	if (use_read_lock)
		vm_map_lock_read(map);
	else
		vm_map_lock(map);

	/*
	 * If the map has an interesting hint, try it before calling full
	 * blown lookup routine.
	 */
	entry = map->hint;
	*out_entry = entry;

	if ((entry == &map->header) ||
	    (vaddr < entry->start) || (vaddr >= entry->end)) {
		vm_map_entry_t tmp_entry;

		/*
		 * Entry was either not a valid hint, or the vaddr was not
		 * contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, &tmp_entry)) {
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}

		entry = tmp_entry;
		*out_entry = entry;
	}
	
	/*
	 * Handle submaps.
	 */

	if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		vm_map_t old_map = map;

		*var_map = map = entry->object.sub_map;
		if (use_read_lock)
			vm_map_unlock_read(old_map);
		else
			vm_map_unlock(old_map);
		use_read_lock = 1;
		goto RetryLookup;
	}

	/*
	 * Check whether this task is allowed to have this page.
	 * Note the special case for MAP_ENTRY_COW
	 * pages with an override.  This is to implement a forced
	 * COW for debuggers.
	 */

	if (fault_type & VM_PROT_OVERRIDE_WRITE)
		prot = entry->max_protection;
	else
		prot = entry->protection;

	fault_type &= (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	if ((fault_type & prot) != fault_type) {
		rv = KERN_PROTECTION_FAILURE;
		goto done;
	}

	if ((entry->eflags & MAP_ENTRY_USER_WIRED) &&
	    (entry->eflags & MAP_ENTRY_COW) &&
	    (fault_type & VM_PROT_WRITE) &&
	    (fault_typea & VM_PROT_OVERRIDE_WRITE) == 0) {
		rv = KERN_PROTECTION_FAILURE;
		goto done;
	}

	/*
	 * If this page is not pageable, we have to get it for all possible
	 * accesses.
	 */

	*wired = (entry->wired_count != 0);
	if (*wired)
		prot = fault_type = entry->protection;

	/*
	 * If the entry was copy-on-write, we either ...
	 */

	if (entry->eflags & MAP_ENTRY_NEEDS_COPY) {
		/*
		 * If we want to write the page, we may as well handle that
		 * now since we've got the map locked.
		 *
		 * If we don't need to write the page, we just demote the
		 * permissions allowed.
		 */

		if (fault_type & VM_PROT_WRITE) {
			/*
			 * Make a new object, and place it in the object
			 * chain.  Note that no new references have appeared
			 * -- one just moved from the map to the new
			 * object.
			 */

			if (use_read_lock && vm_map_lock_upgrade(map)) {
				use_read_lock = 0;
				goto RetryLookup;
			}
			use_read_lock = 0;

			vm_object_shadow(
			    &entry->object.vm_object,
			    &entry->offset,
			    atop(entry->end - entry->start));

			entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
		} else {
			/*
			 * We're attempting to read a copy-on-write page --
			 * don't allow writes.
			 */

			prot &= ~VM_PROT_WRITE;
		}
	}

	/*
	 * Create an object if necessary.
	 */
	if (entry->object.vm_object == NULL &&
	    !map->system_map) {
		if (use_read_lock && vm_map_lock_upgrade(map))  {
			use_read_lock = 0;
			goto RetryLookup;
		}
		use_read_lock = 0;
		entry->object.vm_object = vm_object_allocate(OBJT_DEFAULT,
		    atop(entry->end - entry->start));
		entry->offset = 0;
	}

	/*
	 * Return the object/offset from this entry.  If the entry was
	 * copy-on-write or empty, it has been fixed up.
	 */

	*pindex = OFF_TO_IDX((vaddr - entry->start) + entry->offset);
	*object = entry->object.vm_object;

	/*
	 * Return whether this is the only map sharing this data.  On
	 * success we return with a read lock held on the map.  On failure
	 * we return with the map unlocked.
	 */
	*out_prot = prot;
done:
	if (rv == KERN_SUCCESS) {
		if (use_read_lock == 0)
			vm_map_lock_downgrade(map);
	} else if (use_read_lock) {
		vm_map_unlock_read(map);
	} else {
		vm_map_unlock(map);
	}
	return (rv);
}

/*
 *	vm_map_lookup_done:
 *
 *	Releases locks acquired by a vm_map_lookup
 *	(according to the handle returned by that lookup).
 */

void
vm_map_lookup_done(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{
	/*
	 * Unlock the main-level map
	 */

	vm_map_unlock_read(map);
}

/*
 * Implement uiomove with VM operations.  This handles (and collateral changes)
 * support every combination of source object modification, and COW type
 * operations.
 */
int
vm_uiomove(mapa, srcobject, cp, cnta, uaddra, npages)
	vm_map_t mapa;
	vm_object_t srcobject;
	off_t cp;
	int cnta;
	vm_offset_t uaddra;
	int *npages;
{
	vm_map_t map;
	vm_object_t first_object, oldobject, object;
	vm_map_entry_t entry;
	vm_prot_t prot;
	boolean_t wired;
	int tcnt, rv;
	vm_offset_t uaddr, start, end, tend;
	vm_pindex_t first_pindex, osize, oindex;
	off_t ooffset;
	int cnt;

	if (npages)
		*npages = 0;

	cnt = cnta;
	uaddr = uaddra;

	while (cnt > 0) {
		map = mapa;

		if ((vm_map_lookup(&map, uaddr,
			VM_PROT_READ, &entry, &first_object,
			&first_pindex, &prot, &wired)) != KERN_SUCCESS) {
			return EFAULT;
		}

		vm_map_clip_start(map, entry, uaddr);

		tcnt = cnt;
		tend = uaddr + tcnt;
		if (tend > entry->end) {
			tcnt = entry->end - uaddr;
			tend = entry->end;
		}

		vm_map_clip_end(map, entry, tend);

		start = entry->start;
		end = entry->end;

		osize = atop(tcnt);

		oindex = OFF_TO_IDX(cp);
		if (npages) {
			vm_pindex_t idx;
			for (idx = 0; idx < osize; idx++) {
				vm_page_t m;
				if ((m = vm_page_lookup(srcobject, oindex + idx)) == NULL) {
					vm_map_lookup_done(map, entry);
					return 0;
				}
				/*
				 * disallow busy or invalid pages, but allow
				 * m->busy pages if they are entirely valid.
				 */
				if ((m->flags & PG_BUSY) ||
					((m->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL)) {
					vm_map_lookup_done(map, entry);
					return 0;
				}
			}
		}

/*
 * If we are changing an existing map entry, just redirect
 * the object, and change mappings.
 */
		if ((first_object->type == OBJT_VNODE) &&
			((oldobject = entry->object.vm_object) == first_object)) {

			if ((entry->offset != cp) || (oldobject != srcobject)) {
				/*
   				* Remove old window into the file
   				*/
				pmap_remove (map->pmap, uaddr, tend);

				/*
   				* Force copy on write for mmaped regions
   				*/
				vm_object_pmap_copy_1 (srcobject, oindex, oindex + osize);

				/*
   				* Point the object appropriately
   				*/
				if (oldobject != srcobject) {

				/*
   				* Set the object optimization hint flag
   				*/
					vm_object_set_flag(srcobject, OBJ_OPT);
					vm_object_reference(srcobject);
					entry->object.vm_object = srcobject;

					if (oldobject) {
						vm_object_deallocate(oldobject);
					}
				}

				entry->offset = cp;
				map->timestamp++;
			} else {
				pmap_remove (map->pmap, uaddr, tend);
			}

		} else if ((first_object->ref_count == 1) &&
			(first_object->size == osize) &&
			((first_object->type == OBJT_DEFAULT) ||
				(first_object->type == OBJT_SWAP)) ) {

			oldobject = first_object->backing_object;

			if ((first_object->backing_object_offset != cp) ||
				(oldobject != srcobject)) {
				/*
   				* Remove old window into the file
   				*/
				pmap_remove (map->pmap, uaddr, tend);

				/*
				 * Remove unneeded old pages
				 */
				vm_object_page_remove(first_object, 0, 0, 0);

				/*
				 * Invalidate swap space
				 */
				if (first_object->type == OBJT_SWAP) {
					swap_pager_freespace(first_object,
						0,
						first_object->size);
				}

				/*
   				* Force copy on write for mmaped regions
   				*/
				vm_object_pmap_copy_1 (srcobject, oindex, oindex + osize);

				/*
   				* Point the object appropriately
   				*/
				if (oldobject != srcobject) {

				/*
   				* Set the object optimization hint flag
   				*/
					vm_object_set_flag(srcobject, OBJ_OPT);
					vm_object_reference(srcobject);

					if (oldobject) {
						LIST_REMOVE(
							first_object, shadow_list);
						oldobject->shadow_count--;
						/* XXX bump generation? */
						vm_object_deallocate(oldobject);
					}

					LIST_INSERT_HEAD(&srcobject->shadow_head,
						first_object, shadow_list);
					srcobject->shadow_count++;
					/* XXX bump generation? */

					first_object->backing_object = srcobject;
				}
				first_object->backing_object_offset = cp;
				map->timestamp++;
			} else {
				pmap_remove (map->pmap, uaddr, tend);
			}
/*
 * Otherwise, we have to do a logical mmap.
 */
		} else {

			vm_object_set_flag(srcobject, OBJ_OPT);
			vm_object_reference(srcobject);

			pmap_remove (map->pmap, uaddr, tend);

			vm_object_pmap_copy_1 (srcobject, oindex, oindex + osize);
			vm_map_lock_upgrade(map);

			if (entry == &map->header) {
				map->first_free = &map->header;
			} else if (map->first_free->start >= start) {
				map->first_free = entry->prev;
			}

			SAVE_HINT(map, entry->prev);
			vm_map_entry_delete(map, entry);

			object = srcobject;
			ooffset = cp;

			rv = vm_map_insert(map, object, ooffset, start, tend,
				VM_PROT_ALL, VM_PROT_ALL, MAP_COPY_ON_WRITE);

			if (rv != KERN_SUCCESS)
				panic("vm_uiomove: could not insert new entry: %d", rv);
		}

/*
 * Map the window directly, if it is already in memory
 */
		pmap_object_init_pt(map->pmap, uaddr, entry->protection,
			srcobject, oindex, tcnt, 0);

		map->timestamp++;
		vm_map_unlock(map);

		cnt -= tcnt;
		uaddr += tcnt;
		cp += tcnt;
		if (npages)
			*npages += osize;
	}
	return 0;
}

/*
 * Performs the copy_on_write operations necessary to allow the virtual copies
 * into user space to work.  This has to be called for write(2) system calls
 * from other processes, file unlinking, and file size shrinkage.
 */
void
vm_freeze_copyopts(object, froma, toa)
	vm_object_t object;
	vm_pindex_t froma, toa;
{
	int rv;
	vm_object_t robject;
	vm_pindex_t idx;

	if ((object == NULL) ||
		((object->flags & OBJ_OPT) == 0))
		return;

	if (object->shadow_count > object->ref_count)
		panic("vm_freeze_copyopts: sc > rc");

	while((robject = LIST_FIRST(&object->shadow_head)) != NULL) {
		vm_pindex_t bo_pindex;
		vm_page_t m_in, m_out;

		bo_pindex = OFF_TO_IDX(robject->backing_object_offset);

		vm_object_reference(robject);

		vm_object_pip_wait(robject, "objfrz");

		if (robject->ref_count == 1) {
			vm_object_deallocate(robject);
			continue;
		}

		vm_object_pip_add(robject, 1);

		for (idx = 0; idx < robject->size; idx++) {

			m_out = vm_page_grab(robject, idx,
						VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

			if (m_out->valid == 0) {
				m_in = vm_page_grab(object, bo_pindex + idx,
						VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
				if (m_in->valid == 0) {
					rv = vm_pager_get_pages(object, &m_in, 1, 0);
					if (rv != VM_PAGER_OK) {
						printf("vm_freeze_copyopts: cannot read page from file: %lx\n", (long)m_in->pindex);
						continue;
					}
					vm_page_deactivate(m_in);
				}

				vm_page_protect(m_in, VM_PROT_NONE);
				pmap_copy_page(VM_PAGE_TO_PHYS(m_in), VM_PAGE_TO_PHYS(m_out));
				m_out->valid = m_in->valid;
				vm_page_dirty(m_out);
				vm_page_activate(m_out);
				vm_page_wakeup(m_in);
			}
			vm_page_wakeup(m_out);
		}

		object->shadow_count--;
		object->ref_count--;
		LIST_REMOVE(robject, shadow_list);
		robject->backing_object = NULL;
		robject->backing_object_offset = 0;

		vm_object_pip_wakeup(robject);
		vm_object_deallocate(robject);
	}

	vm_object_clear_flag(object, OBJ_OPT);
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

/*
 *	vm_map_print:	[ debug ]
 */
DB_SHOW_COMMAND(map, vm_map_print)
{
	static int nlines;
	/* XXX convert args. */
	vm_map_t map = (vm_map_t)addr;
	boolean_t full = have_addr;

	vm_map_entry_t entry;

	db_iprintf("Task map %p: pmap=%p, nentries=%d, version=%u\n",
	    (void *)map,
	    (void *)map->pmap, map->nentries, map->timestamp);
	nlines++;

	if (!full && db_indent)
		return;

	db_indent += 2;
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		db_iprintf("map entry %p: start=%p, end=%p\n",
		    (void *)entry, (void *)entry->start, (void *)entry->end);
		nlines++;
		{
			static char *inheritance_name[4] =
			{"share", "copy", "none", "donate_copy"};

			db_iprintf(" prot=%x/%x/%s",
			    entry->protection,
			    entry->max_protection,
			    inheritance_name[(int)(unsigned char)entry->inheritance]);
			if (entry->wired_count != 0)
				db_printf(", wired");
		}
		if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
			/* XXX no %qd in kernel.  Truncate entry->offset. */
			db_printf(", share=%p, offset=0x%lx\n",
			    (void *)entry->object.sub_map,
			    (long)entry->offset);
			nlines++;
			if ((entry->prev == &map->header) ||
			    (entry->prev->object.sub_map !=
				entry->object.sub_map)) {
				db_indent += 2;
				vm_map_print((db_expr_t)(intptr_t)
					     entry->object.sub_map,
					     full, 0, (char *)0);
				db_indent -= 2;
			}
		} else {
			/* XXX no %qd in kernel.  Truncate entry->offset. */
			db_printf(", object=%p, offset=0x%lx",
			    (void *)entry->object.vm_object,
			    (long)entry->offset);
			if (entry->eflags & MAP_ENTRY_COW)
				db_printf(", copy (%s)",
				    (entry->eflags & MAP_ENTRY_NEEDS_COPY) ? "needed" : "done");
			db_printf("\n");
			nlines++;

			if ((entry->prev == &map->header) ||
			    (entry->prev->object.vm_object !=
				entry->object.vm_object)) {
				db_indent += 2;
				vm_object_print((db_expr_t)(intptr_t)
						entry->object.vm_object,
						full, 0, (char *)0);
				nlines += 4;
				db_indent -= 2;
			}
		}
	}
	db_indent -= 2;
	if (db_indent == 0)
		nlines = 0;
}


DB_SHOW_COMMAND(procvm, procvm)
{
	struct proc *p;

	if (have_addr) {
		p = (struct proc *) addr;
	} else {
		p = curproc;
	}

	db_printf("p = %p, vmspace = %p, map = %p, pmap = %p\n",
	    (void *)p, (void *)p->p_vmspace, (void *)&p->p_vmspace->vm_map,
	    (void *)vmspace_pmap(p->p_vmspace));

	vm_map_print((db_expr_t)(intptr_t)&p->p_vmspace->vm_map, 1, 0, NULL);
}

#endif /* DDB */
