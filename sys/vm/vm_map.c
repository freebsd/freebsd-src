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
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/sysent.h>
#include <sys/stdint.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>
#include <vm/uma.h>

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

static struct mtx map_sleep_mtx;
static uma_zone_t mapentzone;
static uma_zone_t kmapentzone;
static uma_zone_t mapzone;
static uma_zone_t vmspace_zone;
static struct vm_object kmapentobj;
static void vmspace_zinit(void *mem, int size);
static void vmspace_zfini(void *mem, int size);
static void vm_map_zinit(void *mem, int size);
static void vm_map_zfini(void *mem, int size);
static void _vm_map_init(vm_map_t map, vm_offset_t min, vm_offset_t max);

#ifdef INVARIANTS
static void vm_map_zdtor(void *mem, int size, void *arg);
static void vmspace_zdtor(void *mem, int size, void *arg);
#endif

void
vm_map_startup(void)
{
	mtx_init(&map_sleep_mtx, "vm map sleep mutex", NULL, MTX_DEF);
	mapzone = uma_zcreate("MAP", sizeof(struct vm_map), NULL,
#ifdef INVARIANTS
	    vm_map_zdtor,
#else
	    NULL,
#endif
	    vm_map_zinit, vm_map_zfini, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_prealloc(mapzone, MAX_KMAP);
	kmapentzone = uma_zcreate("KMAP ENTRY", sizeof(struct vm_map_entry), 
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_MTXCLASS | UMA_ZONE_VM);
	uma_prealloc(kmapentzone, MAX_KMAPENT);
	mapentzone = uma_zcreate("MAP ENTRY", sizeof(struct vm_map_entry), 
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_prealloc(mapentzone, MAX_MAPENT);
}

static void
vmspace_zfini(void *mem, int size)
{
	struct vmspace *vm;

	vm = (struct vmspace *)mem;

	vm_map_zfini(&vm->vm_map, sizeof(vm->vm_map));
}

static void
vmspace_zinit(void *mem, int size)
{
	struct vmspace *vm;

	vm = (struct vmspace *)mem;

	vm_map_zinit(&vm->vm_map, sizeof(vm->vm_map));
}

static void
vm_map_zfini(void *mem, int size)
{
	vm_map_t map;

	map = (vm_map_t)mem;
	mtx_destroy(&map->system_mtx);
	lockdestroy(&map->lock);
}

static void
vm_map_zinit(void *mem, int size)
{
	vm_map_t map;

	map = (vm_map_t)mem;
	map->nentries = 0;
	map->size = 0;
	map->infork = 0;
	mtx_init(&map->system_mtx, "system map", NULL, MTX_DEF);
	lockinit(&map->lock, PVM, "thrd_sleep", 0, LK_NOPAUSE);
}

#ifdef INVARIANTS
static void
vmspace_zdtor(void *mem, int size, void *arg)
{
	struct vmspace *vm;

	vm = (struct vmspace *)mem;

	vm_map_zdtor(&vm->vm_map, sizeof(vm->vm_map), arg);
}
static void
vm_map_zdtor(void *mem, int size, void *arg)
{
	vm_map_t map;

	map = (vm_map_t)mem;
	KASSERT(map->nentries == 0,
	    ("map %p nentries == %d on free.", 
	    map, map->nentries));
	KASSERT(map->size == 0,
	    ("map %p size == %lu on free.",
	    map, (unsigned long)map->size));
	KASSERT(map->infork == 0,
	    ("map %p infork == %d on free.",
	    map, map->infork));
}
#endif	/* INVARIANTS */

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

	GIANT_REQUIRED;
	vm = uma_zalloc(vmspace_zone, M_WAITOK);
	CTR1(KTR_VM, "vmspace_alloc: %p", vm);
	_vm_map_init(&vm->vm_map, min, max);
	pmap_pinit(vmspace_pmap(vm));
	vm->vm_map.pmap = vmspace_pmap(vm);		/* XXX */
	vm->vm_refcnt = 1;
	vm->vm_shm = NULL;
	vm->vm_exitingcnt = 0;
	return (vm);
}

void
vm_init2(void) 
{
	uma_zone_set_obj(kmapentzone, &kmapentobj, lmin(cnt.v_page_count,
	    (VM_MAX_KERNEL_ADDRESS - KERNBASE) / PAGE_SIZE) / 8);
	vmspace_zone = uma_zcreate("VMSPACE", sizeof(struct vmspace), NULL,
#ifdef INVARIANTS
	    vmspace_zdtor,
#else
	    NULL,
#endif
	    vmspace_zinit, vmspace_zfini, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	pmap_init2();
}

static __inline void
vmspace_dofree(struct vmspace *vm)
{
	CTR1(KTR_VM, "vmspace_free: %p", vm);
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
	uma_zfree(vmspace_zone, vm);
}

void
vmspace_free(struct vmspace *vm)
{
	GIANT_REQUIRED;

	if (vm->vm_refcnt == 0)
		panic("vmspace_free: attempt to free already freed vmspace");

	if (--vm->vm_refcnt == 0 && vm->vm_exitingcnt == 0)
		vmspace_dofree(vm);
}

void
vmspace_exitfree(struct proc *p)
{
	struct vmspace *vm;

	GIANT_REQUIRED;
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

	vm_map_lock_read(map);
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
	vm_map_unlock_read(map);
	return (count);
}

void
_vm_map_lock(vm_map_t map, const char *file, int line)
{
	int error;

	if (map->system_map)
		_mtx_lock_flags(&map->system_mtx, 0, file, line);
	else {
		error = lockmgr(&map->lock, LK_EXCLUSIVE, NULL, curthread);
		KASSERT(error == 0, ("%s: failed to get lock", __func__));
	}
	map->timestamp++;
}

void
_vm_map_unlock(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		_mtx_unlock_flags(&map->system_mtx, 0, file, line);
	else
		lockmgr(&map->lock, LK_RELEASE, NULL, curthread);
}

void
_vm_map_lock_read(vm_map_t map, const char *file, int line)
{
	int error;

	if (map->system_map)
		_mtx_lock_flags(&map->system_mtx, 0, file, line);
	else {
		error = lockmgr(&map->lock, LK_EXCLUSIVE, NULL, curthread);
		KASSERT(error == 0, ("%s: failed to get lock", __func__));
	}
}

void
_vm_map_unlock_read(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		_mtx_unlock_flags(&map->system_mtx, 0, file, line);
	else
		lockmgr(&map->lock, LK_RELEASE, NULL, curthread);
}

int
_vm_map_trylock(vm_map_t map, const char *file, int line)
{
	int error;

	error = map->system_map ?
	    !_mtx_trylock(&map->system_mtx, 0, file, line) :
	    lockmgr(&map->lock, LK_EXCLUSIVE | LK_NOWAIT, NULL, curthread);
	if (error == 0)
		map->timestamp++;
	return (error == 0);
}

int
_vm_map_lock_upgrade(vm_map_t map, const char *file, int line)
{

	if (map->system_map) {
#ifdef INVARIANTS
		_mtx_assert(&map->system_mtx, MA_OWNED, file, line);
#endif
	} else
		KASSERT(lockstatus(&map->lock, curthread) == LK_EXCLUSIVE,
		    ("%s: lock not held", __func__));
	map->timestamp++;
	return (0);
}

void
_vm_map_lock_downgrade(vm_map_t map, const char *file, int line)
{

	if (map->system_map) {
#ifdef INVARIANTS
		_mtx_assert(&map->system_mtx, MA_OWNED, file, line);
#endif
	} else
		KASSERT(lockstatus(&map->lock, curthread) == LK_EXCLUSIVE,
		    ("%s: lock not held", __func__));
}

/*
 *	vm_map_unlock_and_wait:
 */
int
vm_map_unlock_and_wait(vm_map_t map, boolean_t user_wait)
{

	mtx_lock(&map_sleep_mtx);
	vm_map_unlock(map);
	return (msleep(&map->root, &map_sleep_mtx, PDROP | PVM, "vmmaps", 0));
}

/*
 *	vm_map_wakeup:
 */
void
vm_map_wakeup(vm_map_t map)
{

	/*
	 * Acquire and release map_sleep_mtx to prevent a wakeup()
	 * from being performed (and lost) between the vm_map_unlock()
	 * and the msleep() in vm_map_unlock_and_wait().
	 */
	mtx_lock(&map_sleep_mtx);
	mtx_unlock(&map_sleep_mtx);
	wakeup(&map->root);
}

long
vmspace_resident_count(struct vmspace *vmspace)
{
	return pmap_resident_count(vmspace_pmap(vmspace));
}

/*
 *	vm_map_create:
 *
 *	Creates and returns a new empty VM map with
 *	the given physical map structure, and having
 *	the given lower and upper address bounds.
 */
vm_map_t
vm_map_create(pmap_t pmap, vm_offset_t min, vm_offset_t max)
{
	vm_map_t result;

	result = uma_zalloc(mapzone, M_WAITOK);
	CTR1(KTR_VM, "vm_map_create: %p", result);
	_vm_map_init(result, min, max);
	result->pmap = pmap;
	return (result);
}

/*
 * Initialize an existing vm_map structure
 * such as that in the vmspace structure.
 * The pmap is set elsewhere.
 */
static void
_vm_map_init(vm_map_t map, vm_offset_t min, vm_offset_t max)
{

	map->header.next = map->header.prev = &map->header;
	map->needs_wakeup = FALSE;
	map->system_map = 0;
	map->min_offset = min;
	map->max_offset = max;
	map->first_free = &map->header;
	map->root = NULL;
	map->timestamp = 0;
}

void
vm_map_init(vm_map_t map, vm_offset_t min, vm_offset_t max)
{
	_vm_map_init(map, min, max);
	mtx_init(&map->system_mtx, "system map", NULL, MTX_DEF);
	lockinit(&map->lock, PVM, "thrd_sleep", 0, LK_NOPAUSE);
}

/*
 *	vm_map_entry_dispose:	[ internal use only ]
 *
 *	Inverse of vm_map_entry_create.
 */
static void
vm_map_entry_dispose(vm_map_t map, vm_map_entry_t entry)
{
	uma_zfree(map->system_map ? kmapentzone : mapentzone, entry);
}

/*
 *	vm_map_entry_create:	[ internal use only ]
 *
 *	Allocates a VM map entry for insertion.
 *	No entry fields are filled in.
 */
static vm_map_entry_t
vm_map_entry_create(vm_map_t map)
{
	vm_map_entry_t new_entry;

	if (map->system_map)
		new_entry = uma_zalloc(kmapentzone, M_NOWAIT);
	else
		new_entry = uma_zalloc(mapentzone, M_WAITOK);
	if (new_entry == NULL)
		panic("vm_map_entry_create: kernel resources exhausted");
	return (new_entry);
}

/*
 *	vm_map_entry_set_behavior:
 *
 *	Set the expected access behavior, either normal, random, or
 *	sequential.
 */
static __inline void
vm_map_entry_set_behavior(vm_map_entry_t entry, u_char behavior)
{
	entry->eflags = (entry->eflags & ~MAP_ENTRY_BEHAV_MASK) |
	    (behavior & MAP_ENTRY_BEHAV_MASK);
}

/*
 *	vm_map_entry_splay:
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the vm_map_entry containing the given address.  If, however, that
 *	address is not found in the vm_map, returns a vm_map_entry that is
 *	adjacent to the address, coming before or after it.
 */
static vm_map_entry_t
vm_map_entry_splay(vm_offset_t address, vm_map_entry_t root)
{
	struct vm_map_entry dummy;
	vm_map_entry_t lefttreemax, righttreemin, y;

	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = &dummy;
	for (;; root = y) {
		if (address < root->start) {
			if ((y = root->left) == NULL)
				break;
			if (address < y->start) {
				/* Rotate right. */
				root->left = y->right;
				y->right = root;
				root = y;
				if ((y = root->left) == NULL)
					break;
			}
			/* Link into the new root's right tree. */
			righttreemin->left = root;
			righttreemin = root;
		} else if (address >= root->end) {
			if ((y = root->right) == NULL)
				break;
			if (address >= y->end) {
				/* Rotate left. */
				root->right = y->left;
				y->left = root;
				root = y;
				if ((y = root->right) == NULL)
					break;
			}
			/* Link into the new root's left tree. */
			lefttreemax->right = root;
			lefttreemax = root;
		} else
			break;
	}
	/* Assemble the new root. */
	lefttreemax->right = root->left;
	righttreemin->left = root->right;
	root->left = dummy.right;
	root->right = dummy.left;
	return (root);
}

/*
 *	vm_map_entry_{un,}link:
 *
 *	Insert/remove entries from maps.
 */
static void
vm_map_entry_link(vm_map_t map,
		  vm_map_entry_t after_where,
		  vm_map_entry_t entry)
{

	CTR4(KTR_VM,
	    "vm_map_entry_link: map %p, nentries %d, entry %p, after %p", map,
	    map->nentries, entry, after_where);
	map->nentries++;
	entry->prev = after_where;
	entry->next = after_where->next;
	entry->next->prev = entry;
	after_where->next = entry;

	if (after_where != &map->header) {
		if (after_where != map->root)
			vm_map_entry_splay(after_where->start, map->root);
		entry->right = after_where->right;
		entry->left = after_where;
		after_where->right = NULL;
	} else {
		entry->right = map->root;
		entry->left = NULL;
	}
	map->root = entry;
}

static void
vm_map_entry_unlink(vm_map_t map,
		    vm_map_entry_t entry)
{
	vm_map_entry_t next, prev, root;

	if (entry != map->root)
		vm_map_entry_splay(entry->start, map->root);
	if (entry->left == NULL)
		root = entry->right;
	else {
		root = vm_map_entry_splay(entry->start, entry->left);
		root->right = entry->right;
	}
	map->root = root;

	prev = entry->prev;
	next = entry->next;
	next->prev = prev;
	prev->next = next;
	map->nentries--;
	CTR3(KTR_VM, "vm_map_entry_unlink: map %p, nentries %d, entry %p", map,
	    map->nentries, entry);
}

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
vm_map_lookup_entry(
	vm_map_t map,
	vm_offset_t address,
	vm_map_entry_t *entry)	/* OUT */
{
	vm_map_entry_t cur;

	cur = vm_map_entry_splay(address, map->root);
	if (cur == NULL)
		*entry = &map->header;
	else {
		map->root = cur;

		if (address >= cur->start) {
			*entry = cur;
			if (cur->end > address)
				return (TRUE);
		} else
			*entry = cur->prev;
	}
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
		vm_object_lock(object);
		if ((object->ref_count > 1) || (object->shadow_count != 0)) {
			vm_object_clear_flag(object, OBJ_ONEMAPPING);
		}
		vm_object_unlock(object);
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
		mtx_lock(&Giant);
		pmap_object_init_pt(map->pmap, start,
				    object, OFF_TO_IDX(offset), end - start,
				    cow & MAP_PREFAULT_PARTIAL);
		mtx_unlock(&Giant);
	}

	return (KERN_SUCCESS);
}

/*
 * Find sufficient space for `length' bytes in the given map, starting at
 * `start'.  The map must be locked.  Returns 0 on success, 1 on no space.
 */
int
vm_map_findspace(
	vm_map_t map,
	vm_offset_t start,
	vm_size_t length,
	vm_offset_t *addr)
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
	*addr = start;
	if (map == kernel_map) {
		vm_offset_t ksize;
		if ((ksize = round_page(start + length)) > kernel_vm_end) {
			mtx_lock(&Giant);
			pmap_growkernel(ksize);
			mtx_unlock(&Giant);
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

	if (map == kmem_map)
		s = splvm();

	vm_map_lock(map);
	if (find_space) {
		if (vm_map_findspace(map, start, length, addr)) {
			vm_map_unlock(map);
			if (map == kmem_map)
				splx(s);
			return (KERN_NO_SPACE);
		}
		start = *addr;
	}
	result = vm_map_insert(map, object, offset,
		start, start + length, prot, max, cow);
	vm_map_unlock(map);

	if (map == kmem_map)
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
 *	both neighbors.
 */
void
vm_map_simplify_entry(vm_map_t map, vm_map_entry_t entry)
{
	vm_map_entry_t next, prev;
	vm_size_t prevsize, esize;

	if (entry->eflags & (MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_IS_SUB_MAP))
		return;

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
_vm_map_clip_start(vm_map_t map, vm_map_entry_t entry, vm_offset_t start)
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
	if ((endaddr) < (entry->end)) \
		_vm_map_clip_end((map), (entry), (endaddr)); \
}

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_end(vm_map_t map, vm_map_entry_t entry, vm_offset_t end)
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
vm_map_submap(
	vm_map_t map,
	vm_offset_t start,
	vm_offset_t end,
	vm_map_t submap)
{
	vm_map_entry_t entry;
	int result = KERN_INVALID_ARGUMENT;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	} else
		entry = entry->next;

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
			mtx_lock(&Giant);
			vm_page_lock_queues();
#define MASK(entry)	(((entry)->eflags & MAP_ENTRY_COW) ? ~VM_PROT_WRITE : \
							VM_PROT_ALL)
			pmap_protect(map->pmap, current->start,
			    current->end,
			    current->protection & MASK(current));
#undef	MASK
			vm_page_unlock_queues();
			mtx_unlock(&Giant);
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
vm_map_madvise(
	vm_map_t map,
	vm_offset_t start, 
	vm_offset_t end,
	int behav)
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
				mtx_lock(&Giant);
				pmap_object_init_pt(
				    map->pmap, 
				    useStart,
				    current->object.vm_object,
				    pindex, 
				    (count << PAGE_SHIFT),
				    MAP_PREFAULT_MADVISE
				);
				mtx_unlock(&Giant);
			}
		}
		vm_map_unlock_read(map);
	}
	return (0);
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
 *	vm_map_unwire:
 *
 *	Implements both kernel and user unwiring.
 */
int
vm_map_unwire(vm_map_t map, vm_offset_t start, vm_offset_t end,
	boolean_t user_unwire)
{
	vm_map_entry_t entry, first_entry, tmp_entry;
	vm_offset_t saved_start;
	unsigned int last_timestamp;
	int rv;
	boolean_t need_wakeup, result;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}
	last_timestamp = map->timestamp;
	entry = first_entry;
	while (entry != &map->header && entry->start < end) {
		if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
			/*
			 * We have not yet clipped the entry.
			 */
			saved_start = (start >= entry->start) ? start :
			    entry->start;
			entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			if (vm_map_unlock_and_wait(map, user_unwire)) {
				/*
				 * Allow interruption of user unwiring?
				 */
			}
			vm_map_lock(map);
			if (last_timestamp+1 != map->timestamp) {
				/*
				 * Look again for the entry because the map was
				 * modified while it was unlocked.
				 * Specifically, the entry may have been
				 * clipped, merged, or deleted.
				 */
				if (!vm_map_lookup_entry(map, saved_start,
				    &tmp_entry)) {
					if (saved_start == start) {
						/*
						 * First_entry has been deleted.
						 */
						vm_map_unlock(map);
						return (KERN_INVALID_ADDRESS);
					}
					end = saved_start;
					rv = KERN_INVALID_ADDRESS;
					goto done;
				}
				if (entry == first_entry)
					first_entry = tmp_entry;
				else
					first_entry = NULL;
				entry = tmp_entry;
			}
			last_timestamp = map->timestamp;
			continue;
		}
		vm_map_clip_start(map, entry, start);
		vm_map_clip_end(map, entry, end);
		/*
		 * Mark the entry in case the map lock is released.  (See
		 * above.)
		 */
		entry->eflags |= MAP_ENTRY_IN_TRANSITION;
		/*
		 * Check the map for holes in the specified region.
		 */
		if (entry->end < end && (entry->next == &map->header ||
		    entry->next->start > entry->end)) {
			end = entry->end;
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}
		/*
		 * Require that the entry is wired.
		 */
		if (entry->wired_count == 0 || (user_unwire &&
		    (entry->eflags & MAP_ENTRY_USER_WIRED) == 0)) {
			end = entry->end;
			rv = KERN_INVALID_ARGUMENT;
			goto done;
		}
		entry = entry->next;
	}
	rv = KERN_SUCCESS;
done:
	need_wakeup = FALSE;
	if (first_entry == NULL) {
		result = vm_map_lookup_entry(map, start, &first_entry);
		KASSERT(result, ("vm_map_unwire: lookup failed"));
	}
	entry = first_entry;
	while (entry != &map->header && entry->start < end) {
		if (rv == KERN_SUCCESS) {
			if (user_unwire)
				entry->eflags &= ~MAP_ENTRY_USER_WIRED;
			entry->wired_count--;
			if (entry->wired_count == 0) {
				/*
				 * Retain the map lock.
				 */
				vm_fault_unwire(map, entry->start, entry->end);
			}
		}
		KASSERT(entry->eflags & MAP_ENTRY_IN_TRANSITION,
			("vm_map_unwire: in-transition flag missing"));
		entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = TRUE;
		}
		vm_map_simplify_entry(map, entry);
		entry = entry->next;
	}
	vm_map_unlock(map);
	if (need_wakeup)
		vm_map_wakeup(map);
	return (rv);
}

/*
 *	vm_map_wire:
 *
 *	Implements both kernel and user wiring.
 */
int
vm_map_wire(vm_map_t map, vm_offset_t start, vm_offset_t end,
	boolean_t user_wire)
{
	vm_map_entry_t entry, first_entry, tmp_entry;
	vm_offset_t saved_end, saved_start;
	unsigned int last_timestamp;
	int rv;
	boolean_t need_wakeup, result;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		vm_map_unlock(map);
		return (KERN_INVALID_ADDRESS);
	}
	last_timestamp = map->timestamp;
	entry = first_entry;
	while (entry != &map->header && entry->start < end) {
		if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
			/*
			 * We have not yet clipped the entry.
			 */
			saved_start = (start >= entry->start) ? start :
			    entry->start;
			entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			if (vm_map_unlock_and_wait(map, user_wire)) {
				/*
				 * Allow interruption of user wiring?
				 */
			}
			vm_map_lock(map);
			if (last_timestamp + 1 != map->timestamp) {
				/*
				 * Look again for the entry because the map was
				 * modified while it was unlocked.
				 * Specifically, the entry may have been
				 * clipped, merged, or deleted.
				 */
				if (!vm_map_lookup_entry(map, saved_start,
				    &tmp_entry)) {
					if (saved_start == start) {
						/*
						 * first_entry has been deleted.
						 */
						vm_map_unlock(map);
						return (KERN_INVALID_ADDRESS);
					}
					end = saved_start;
					rv = KERN_INVALID_ADDRESS;
					goto done;
				}
				if (entry == first_entry)
					first_entry = tmp_entry;
				else
					first_entry = NULL;
				entry = tmp_entry;
			}
			last_timestamp = map->timestamp;
			continue;
		}
		vm_map_clip_start(map, entry, start);
		vm_map_clip_end(map, entry, end);
		/*
		 * Mark the entry in case the map lock is released.  (See
		 * above.)
		 */
		entry->eflags |= MAP_ENTRY_IN_TRANSITION;
		/*
		 *
		 */
		if (entry->wired_count == 0) {
			entry->wired_count++;
			saved_start = entry->start;
			saved_end = entry->end;
			/*
			 * Release the map lock, relying on the in-transition
			 * mark.
			 */
			vm_map_unlock(map);
			rv = vm_fault_wire(map, saved_start, saved_end,
			    user_wire);
			vm_map_lock(map);
			if (last_timestamp + 1 != map->timestamp) {
				/*
				 * Look again for the entry because the map was
				 * modified while it was unlocked.  The entry
				 * may have been clipped, but NOT merged or
				 * deleted.
				 */
				result = vm_map_lookup_entry(map, saved_start,
				    &tmp_entry);
				KASSERT(result, ("vm_map_wire: lookup failed"));
				if (entry == first_entry)
					first_entry = tmp_entry;
				else
					first_entry = NULL;
				entry = tmp_entry;
				while (entry->end < saved_end) {
					if (rv != KERN_SUCCESS) {
						KASSERT(entry->wired_count == 1,
						    ("vm_map_wire: bad count"));
						entry->wired_count = -1;
					}
					entry = entry->next;
				}
			}
			last_timestamp = map->timestamp;
			if (rv != KERN_SUCCESS) {
				KASSERT(entry->wired_count == 1,
				    ("vm_map_wire: bad count"));
				/*
				 * Assign an out-of-range value to represent
				 * the failure to wire this entry.
				 */
				entry->wired_count = -1;
				end = entry->end;
				goto done;
			}
		} else if (!user_wire ||
			   (entry->eflags & MAP_ENTRY_USER_WIRED) == 0) {
			entry->wired_count++;
		}
		/*
		 * Check the map for holes in the specified region.
		 */
		if (entry->end < end && (entry->next == &map->header ||
		    entry->next->start > entry->end)) {
			end = entry->end;
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}
		entry = entry->next;
	}
	rv = KERN_SUCCESS;
done:
	need_wakeup = FALSE;
	if (first_entry == NULL) {
		result = vm_map_lookup_entry(map, start, &first_entry);
		KASSERT(result, ("vm_map_wire: lookup failed"));
	}
	entry = first_entry;
	while (entry != &map->header && entry->start < end) {
		if (rv == KERN_SUCCESS) {
			if (user_wire)
				entry->eflags |= MAP_ENTRY_USER_WIRED;
		} else if (entry->wired_count == -1) {
			/*
			 * Wiring failed on this entry.  Thus, unwiring is
			 * unnecessary.
			 */
			entry->wired_count = 0;
		} else {
			if (!user_wire ||
			    (entry->eflags & MAP_ENTRY_USER_WIRED) == 0)
				entry->wired_count--;
			if (entry->wired_count == 0) {
				/*
				 * Retain the map lock.
				 */
				vm_fault_unwire(map, entry->start, entry->end);
			}
		}
		KASSERT(entry->eflags & MAP_ENTRY_IN_TRANSITION,
			("vm_map_wire: in-transition flag missing"));
		entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = TRUE;
		}
		vm_map_simplify_entry(map, entry);
		entry = entry->next;
	}
	vm_map_unlock(map);
	if (need_wakeup)
		vm_map_wakeup(map);
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
vm_map_clean(
	vm_map_t map,
	vm_offset_t start,
	vm_offset_t end,
	boolean_t syncio,
	boolean_t invalidate)
{
	vm_map_entry_t current;
	vm_map_entry_t entry;
	vm_size_t size;
	vm_object_t object;
	vm_ooffset_t offset;

	GIANT_REQUIRED;

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

	if (invalidate) {
		vm_page_lock_queues();
		pmap_remove(map->pmap, start, end);
		vm_page_unlock_queues();
	}
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
			if (object->size < OFF_TO_IDX(offset + size))
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
			vn_lock(object->handle, LK_EXCLUSIVE | LK_RETRY, curthread);
			flags = (syncio || invalidate) ? OBJPC_SYNC : 0;
			flags |= invalidate ? OBJPC_INVAL : 0;
			vm_object_page_clean(object,
			    OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + size + PAGE_MASK),
			    flags);
			VOP_UNLOCK(object->handle, 0, curthread);
			vm_object_deallocate(object);
		}
		if (object && invalidate &&
		    ((object->type == OBJT_VNODE) ||
		     (object->type == OBJT_DEVICE))) {
			vm_object_reference(object);
			vm_object_lock(object);
			vm_object_page_remove(object,
			    OFF_TO_IDX(offset),
			    OFF_TO_IDX(offset + size + PAGE_MASK),
			    FALSE);
			vm_object_unlock(object);
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
vm_map_entry_unwire(vm_map_t map, vm_map_entry_t entry)
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
vm_map_entry_delete(vm_map_t map, vm_map_entry_t entry)
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
vm_map_delete(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	vm_object_t object;
	vm_map_entry_t entry;
	vm_map_entry_t first_entry;

	/*
	 * Find the start of the region, and clip it
	 */
	if (!vm_map_lookup_entry(map, start, &first_entry))
		entry = first_entry->next;
	else {
		entry = first_entry;
		vm_map_clip_start(map, entry, start);
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
		 * Wait for wiring or unwiring of an entry to complete.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0) {
			unsigned int last_timestamp;
			vm_offset_t saved_start;
			vm_map_entry_t tmp_entry;

			saved_start = entry->start;
			entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			last_timestamp = map->timestamp;
			(void) vm_map_unlock_and_wait(map, FALSE);
			vm_map_lock(map);
			if (last_timestamp + 1 != map->timestamp) {
				/*
				 * Look again for the entry because the map was
				 * modified while it was unlocked.
				 * Specifically, the entry may have been
				 * clipped, merged, or deleted.
				 */
				if (!vm_map_lookup_entry(map, saved_start,
							 &tmp_entry))
					entry = tmp_entry->next;
				else {
					entry = tmp_entry;
					vm_map_clip_start(map, entry,
							  saved_start);
				}
			}
			continue;
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
			vm_object_lock(object);
			vm_object_page_remove(object, offidxstart, offidxend, FALSE);
			vm_object_unlock(object);
		} else {
			vm_object_lock(object);
			vm_page_lock_queues();
			pmap_remove(map->pmap, s, e);
			vm_page_unlock_queues();
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
			vm_object_unlock(object);
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
vm_map_remove(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	int result, s = 0;

	if (map == kmem_map)
		s = splvm();

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	result = vm_map_delete(map, start, end);
	vm_map_unlock(map);

	if (map == kmem_map)
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

	vm_map_lock_read(map);
	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		vm_map_unlock_read(map);
		return (FALSE);
	}
	entry = tmp_entry;

	while (start < end) {
		if (entry == &map->header) {
			vm_map_unlock_read(map);
			return (FALSE);
		}
		/*
		 * No holes allowed!
		 */
		if (start < entry->start) {
			vm_map_unlock_read(map);
			return (FALSE);
		}
		/*
		 * Check protection associated with entry.
		 */
		if ((entry->protection & protection) != protection) {
			vm_map_unlock_read(map);
			return (FALSE);
		}
		/* go to next entry */
		start = entry->end;
		entry = entry->next;
	}
	vm_map_unlock_read(map);
	return (TRUE);
}

/*
 *	vm_map_copy_entry:
 *
 *	Copies the contents of the source entry to the destination
 *	entry.  The entries *must* be aligned properly.
 */
static void
vm_map_copy_entry(
	vm_map_t src_map,
	vm_map_t dst_map,
	vm_map_entry_t src_entry, 
	vm_map_entry_t dst_entry)
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
			vm_page_lock_queues();
			pmap_protect(src_map->pmap,
			    src_entry->start,
			    src_entry->end,
			    src_entry->protection & ~VM_PROT_WRITE);
			vm_page_unlock_queues();
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
					vm_object_split(src_entry);
					src_object = src_entry->object.vm_object;
				}
			}

			vm_object_reference(src_object);
			vm_object_lock(src_object);
			vm_object_clear_flag(src_object, OBJ_ONEMAPPING);
			vm_object_unlock(src_object);
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
vmspace_fork(struct vmspace *vm1)
{
	struct vmspace *vm2;
	vm_map_t old_map = &vm1->vm_map;
	vm_map_t new_map;
	vm_map_entry_t old_entry;
	vm_map_entry_t new_entry;
	vm_object_t object;

	GIANT_REQUIRED;

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
			vm_object_lock(object);
			vm_object_clear_flag(object, OBJ_ONEMAPPING);
			vm_object_unlock(object);

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

	if (addrbos < vm_map_min(map))
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
	    curthread->td_proc->p_rlimit[RLIMIT_VMEM].rlim_cur) {
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
	int      rv;
	int      is_procstack;

	GIANT_REQUIRED;
	
Retry:
	vm_map_lock_read(map);

	/* If addr is already in the entry range, no need to grow.*/
	if (vm_map_lookup_entry(map, addr, &prev_entry)) {
		vm_map_unlock_read(map);
		return (KERN_SUCCESS);
	}

	if ((stack_entry = prev_entry->next) == &map->header) {
		vm_map_unlock_read(map);
		return (KERN_SUCCESS);
	} 
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
		vm_map_unlock_read(map);
		return (KERN_SUCCESS);
	} 
	
	/* Find the minimum grow amount */
	grow_amount = roundup (stack_entry->start - addr, PAGE_SIZE);
	if (grow_amount > stack_entry->avail_ssize) {
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
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
		if (vm_map_lock_upgrade(map))
			goto Retry;

		stack_entry->avail_ssize = stack_entry->start - end;

		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	is_procstack = addr >= (vm_offset_t)vm->vm_maxsaddr;

	/* If this is the main process stack, see if we're over the 
	 * stack limit.
	 */
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount >
			     p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
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
	    curthread->td_proc->p_rlimit[RLIMIT_VMEM].rlim_cur) {
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
	}

	if (vm_map_lock_upgrade(map))
		goto Retry;

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
	    p->p_sysent->sv_stackprot, VM_PROT_ALL, 0);

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

	vm_map_unlock(map);
	return (rv);
}

/*
 * Unshare the specified VM space for exec.  If other processes are
 * mapped to it, then create a new one.  The new vmspace is null.
 */
void
vmspace_exec(struct proc *p, vm_offset_t minuser, vm_offset_t maxuser)
{
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;

	GIANT_REQUIRED;
	newvmspace = vmspace_alloc(minuser, maxuser);
	bcopy(&oldvmspace->vm_startcopy, &newvmspace->vm_startcopy,
	    (caddr_t) (newvmspace + 1) - (caddr_t) &newvmspace->vm_startcopy);
	/*
	 * This code is written like this for prototype purposes.  The
	 * goal is to avoid running down the vmspace here, but let the
	 * other process's that are still using the vmspace to finally
	 * run it down.  Even though there is little or no chance of blocking
	 * here, it is a good idea to keep this form for future mods.
	 */
	p->p_vmspace = newvmspace;
	pmap_pinit2(vmspace_pmap(newvmspace));
	vmspace_free(oldvmspace);
	if (p == curthread->td_proc)		/* XXXKSE ? */
		pmap_activate(curthread);
}

/*
 * Unshare the specified VM space for forcing COW.  This
 * is called by rfork, for the (RFMEM|RFPROC) == 0 case.
 */
void
vmspace_unshare(struct proc *p)
{
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;

	GIANT_REQUIRED;
	if (oldvmspace->vm_refcnt == 1)
		return;
	newvmspace = vmspace_fork(oldvmspace);
	p->p_vmspace = newvmspace;
	pmap_pinit2(vmspace_pmap(newvmspace));
	vmspace_free(oldvmspace);
	if (p == curthread->td_proc)		/* XXXKSE ? */
		pmap_activate(curthread);
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

RetryLookup:;
	/*
	 * Lookup the faulting address.
	 */

	vm_map_lock_read(map);
#define	RETURN(why) \
		{ \
		vm_map_unlock_read(map); \
		return (why); \
		}

	/*
	 * If the map has an interesting hint, try it before calling full
	 * blown lookup routine.
	 */
	entry = map->root;
	*out_entry = entry;
	if (entry == NULL ||
	    (vaddr < entry->start) || (vaddr >= entry->end)) {
		/*
		 * Entry was either not a valid hint, or the vaddr was not
		 * contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, out_entry))
			RETURN(KERN_INVALID_ADDRESS);

		entry = *out_entry;
	}
	
	/*
	 * Handle submaps.
	 */
	if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		vm_map_t old_map = map;

		*var_map = map = entry->object.sub_map;
		vm_map_unlock_read(old_map);
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
			RETURN(KERN_PROTECTION_FAILURE);
	}
	if ((entry->eflags & MAP_ENTRY_USER_WIRED) &&
	    (entry->eflags & MAP_ENTRY_COW) &&
	    (fault_type & VM_PROT_WRITE) &&
	    (fault_typea & VM_PROT_OVERRIDE_WRITE) == 0) {
		RETURN(KERN_PROTECTION_FAILURE);
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
			if (vm_map_lock_upgrade(map))
				goto RetryLookup;

			vm_object_shadow(
			    &entry->object.vm_object,
			    &entry->offset,
			    atop(entry->end - entry->start));
			entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;

			vm_map_lock_downgrade(map);
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
		if (vm_map_lock_upgrade(map)) 
			goto RetryLookup;
		entry->object.vm_object = vm_object_allocate(OBJT_DEFAULT,
		    atop(entry->end - entry->start));
		entry->offset = 0;
		vm_map_lock_downgrade(map);
	}

	/*
	 * Return the object/offset from this entry.  If the entry was
	 * copy-on-write or empty, it has been fixed up.
	 */
	*pindex = OFF_TO_IDX((vaddr - entry->start) + entry->offset);
	*object = entry->object.vm_object;

	/*
	 * Return whether this is the only map sharing this data.
	 */
	*out_prot = prot;
	return (KERN_SUCCESS);

#undef	RETURN
}

/*
 *	vm_map_lookup_done:
 *
 *	Releases locks acquired by a vm_map_lookup
 *	(according to the handle returned by that lookup).
 */
void
vm_map_lookup_done(vm_map_t map, vm_map_entry_t entry)
{
	/*
	 * Unlock the main-level map
	 */
	vm_map_unlock_read(map);
}

#ifdef ENABLE_VFS_IOOPT
/*
 * Experimental support for zero-copy I/O
 *
 * Implement uiomove with VM operations.  This handles (and collateral changes)
 * support every combination of source object modification, and COW type
 * operations.
 */
int
vm_uiomove(
	vm_map_t mapa,
	vm_object_t srcobject,
	off_t cp,
	int cnta,
	vm_offset_t uaddra,
	int *npages)
{
	vm_map_t map;
	vm_object_t first_object, oldobject, object;
	vm_map_entry_t entry;
	vm_prot_t prot;
	boolean_t wired;
	int tcnt, rv;
	vm_offset_t uaddr, start, end, tend;
	vm_pindex_t first_pindex, oindex;
	vm_size_t osize;
	off_t ooffset;
	int cnt;

	GIANT_REQUIRED;

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
			vm_size_t idx;
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
				vm_page_lock_queues();
				pmap_remove(map->pmap, uaddr, tend);
				vm_page_unlock_queues();

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
				vm_page_lock_queues();
				pmap_remove(map->pmap, uaddr, tend);
				vm_page_unlock_queues();
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
				vm_page_lock_queues();
				pmap_remove(map->pmap, uaddr, tend);
				vm_page_unlock_queues();

				/*
				 * Remove unneeded old pages
				 */
				vm_object_lock(first_object);
				vm_object_page_remove(first_object, 0, 0, 0);
				vm_object_unlock(first_object);

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
						TAILQ_REMOVE(&oldobject->shadow_head,
							first_object, shadow_list);
						oldobject->shadow_count--;
						/* XXX bump generation? */
						vm_object_deallocate(oldobject);
					}

					TAILQ_INSERT_TAIL(&srcobject->shadow_head,
						first_object, shadow_list);
					srcobject->shadow_count++;
					/* XXX bump generation? */

					first_object->backing_object = srcobject;
				}
				first_object->backing_object_offset = cp;
				map->timestamp++;
			} else {
				vm_page_lock_queues();
				pmap_remove(map->pmap, uaddr, tend);
				vm_page_unlock_queues();
			}
/*
 * Otherwise, we have to do a logical mmap.
 */
		} else {

			vm_object_set_flag(srcobject, OBJ_OPT);
			vm_object_reference(srcobject);

			vm_page_lock_queues();
			pmap_remove(map->pmap, uaddr, tend);
			vm_page_unlock_queues();

			vm_object_pmap_copy_1 (srcobject, oindex, oindex + osize);
			vm_map_lock_upgrade(map);

			if (entry == &map->header) {
				map->first_free = &map->header;
			} else if (map->first_free->start >= start) {
				map->first_free = entry->prev;
			}

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
		pmap_object_init_pt(map->pmap, uaddr,
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
#endif

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
			db_printf(", share=%p, offset=0x%jx\n",
			    (void *)entry->object.sub_map,
			    (uintmax_t)entry->offset);
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
			db_printf(", object=%p, offset=0x%jx",
			    (void *)entry->object.vm_object,
			    (uintmax_t)entry->offset);
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
