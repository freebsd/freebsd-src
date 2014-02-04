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
 */

/*
 *	Virtual memory mapping module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/shm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vnode_pager.h>
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
 *	entries; a self-adjusting binary search tree of these
 *	entries is used to speed up lookups.
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

static struct mtx map_sleep_mtx;
static uma_zone_t mapentzone;
static uma_zone_t kmapentzone;
static uma_zone_t mapzone;
static uma_zone_t vmspace_zone;
static int vmspace_zinit(void *mem, int size, int flags);
static int vm_map_zinit(void *mem, int ize, int flags);
static void _vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min,
    vm_offset_t max);
static void vm_map_entry_deallocate(vm_map_entry_t entry, boolean_t system_map);
static void vm_map_entry_dispose(vm_map_t map, vm_map_entry_t entry);
#ifdef INVARIANTS
static void vm_map_zdtor(void *mem, int size, void *arg);
static void vmspace_zdtor(void *mem, int size, void *arg);
#endif

#define	ENTRY_CHARGED(e) ((e)->cred != NULL || \
    ((e)->object.vm_object != NULL && (e)->object.vm_object->cred != NULL && \
     !((e)->eflags & MAP_ENTRY_NEEDS_COPY)))

/* 
 * PROC_VMSPACE_{UN,}LOCK() can be a noop as long as vmspaces are type
 * stable.
 */
#define PROC_VMSPACE_LOCK(p) do { } while (0)
#define PROC_VMSPACE_UNLOCK(p) do { } while (0)

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
	    vm_map_zinit, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	uma_prealloc(mapzone, MAX_KMAP);
	kmapentzone = uma_zcreate("KMAP ENTRY", sizeof(struct vm_map_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_MTXCLASS | UMA_ZONE_VM);
	mapentzone = uma_zcreate("MAP ENTRY", sizeof(struct vm_map_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	vmspace_zone = uma_zcreate("VMSPACE", sizeof(struct vmspace), NULL,
#ifdef INVARIANTS
	    vmspace_zdtor,
#else
	    NULL,
#endif
	    vmspace_zinit, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
}

static int
vmspace_zinit(void *mem, int size, int flags)
{
	struct vmspace *vm;

	vm = (struct vmspace *)mem;

	vm->vm_map.pmap = NULL;
	(void)vm_map_zinit(&vm->vm_map, sizeof(vm->vm_map), flags);
	PMAP_LOCK_INIT(vmspace_pmap(vm));
	return (0);
}

static int
vm_map_zinit(void *mem, int size, int flags)
{
	vm_map_t map;

	map = (vm_map_t)mem;
	memset(map, 0, sizeof(*map));
	mtx_init(&map->system_mtx, "vm map (system)", NULL, MTX_DEF | MTX_DUPOK);
	sx_init(&map->lock, "vm map (user)");
	return (0);
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
}
#endif	/* INVARIANTS */

/*
 * Allocate a vmspace structure, including a vm_map and pmap,
 * and initialize those structures.  The refcnt is set to 1.
 *
 * If 'pinit' is NULL then the embedded pmap is initialized via pmap_pinit().
 */
struct vmspace *
vmspace_alloc(vm_offset_t min, vm_offset_t max, pmap_pinit_t pinit)
{
	struct vmspace *vm;

	vm = uma_zalloc(vmspace_zone, M_WAITOK);

	KASSERT(vm->vm_map.pmap == NULL, ("vm_map.pmap must be NULL"));

	if (pinit == NULL)
		pinit = &pmap_pinit;

	if (!pinit(vmspace_pmap(vm))) {
		uma_zfree(vmspace_zone, vm);
		return (NULL);
	}
	CTR1(KTR_VM, "vmspace_alloc: %p", vm);
	_vm_map_init(&vm->vm_map, vmspace_pmap(vm), min, max);
	vm->vm_refcnt = 1;
	vm->vm_shm = NULL;
	vm->vm_swrss = 0;
	vm->vm_tsize = 0;
	vm->vm_dsize = 0;
	vm->vm_ssize = 0;
	vm->vm_taddr = 0;
	vm->vm_daddr = 0;
	vm->vm_maxsaddr = 0;
	return (vm);
}

static void
vmspace_container_reset(struct proc *p)
{

#ifdef RACCT
	PROC_LOCK(p);
	racct_set(p, RACCT_DATA, 0);
	racct_set(p, RACCT_STACK, 0);
	racct_set(p, RACCT_RSS, 0);
	racct_set(p, RACCT_MEMLOCK, 0);
	racct_set(p, RACCT_VMEM, 0);
	PROC_UNLOCK(p);
#endif
}

static inline void
vmspace_dofree(struct vmspace *vm)
{

	CTR1(KTR_VM, "vmspace_free: %p", vm);

	/*
	 * Make sure any SysV shm is freed, it might not have been in
	 * exit1().
	 */
	shmexit(vm);

	/*
	 * Lock the map, to wait out all other references to it.
	 * Delete all of the mappings and pages they hold, then call
	 * the pmap module to reclaim anything left.
	 */
	(void)vm_map_remove(&vm->vm_map, vm->vm_map.min_offset,
	    vm->vm_map.max_offset);

	pmap_release(vmspace_pmap(vm));
	vm->vm_map.pmap = NULL;
	uma_zfree(vmspace_zone, vm);
}

void
vmspace_free(struct vmspace *vm)
{

	if (vm->vm_refcnt == 0)
		panic("vmspace_free: attempt to free already freed vmspace");

	if (atomic_fetchadd_int(&vm->vm_refcnt, -1) == 1)
		vmspace_dofree(vm);
}

void
vmspace_exitfree(struct proc *p)
{
	struct vmspace *vm;

	PROC_VMSPACE_LOCK(p);
	vm = p->p_vmspace;
	p->p_vmspace = NULL;
	PROC_VMSPACE_UNLOCK(p);
	KASSERT(vm == &vmspace0, ("vmspace_exitfree: wrong vmspace"));
	vmspace_free(vm);
}

void
vmspace_exit(struct thread *td)
{
	int refcnt;
	struct vmspace *vm;
	struct proc *p;

	/*
	 * Release user portion of address space.
	 * This releases references to vnodes,
	 * which could cause I/O if the file has been unlinked.
	 * Need to do this early enough that we can still sleep.
	 *
	 * The last exiting process to reach this point releases as
	 * much of the environment as it can. vmspace_dofree() is the
	 * slower fallback in case another process had a temporary
	 * reference to the vmspace.
	 */

	p = td->td_proc;
	vm = p->p_vmspace;
	atomic_add_int(&vmspace0.vm_refcnt, 1);
	do {
		refcnt = vm->vm_refcnt;
		if (refcnt > 1 && p->p_vmspace != &vmspace0) {
			/* Switch now since other proc might free vmspace */
			PROC_VMSPACE_LOCK(p);
			p->p_vmspace = &vmspace0;
			PROC_VMSPACE_UNLOCK(p);
			pmap_activate(td);
		}
	} while (!atomic_cmpset_int(&vm->vm_refcnt, refcnt, refcnt - 1));
	if (refcnt == 1) {
		if (p->p_vmspace != vm) {
			/* vmspace not yet freed, switch back */
			PROC_VMSPACE_LOCK(p);
			p->p_vmspace = vm;
			PROC_VMSPACE_UNLOCK(p);
			pmap_activate(td);
		}
		pmap_remove_pages(vmspace_pmap(vm));
		/* Switch now since this proc will free vmspace */
		PROC_VMSPACE_LOCK(p);
		p->p_vmspace = &vmspace0;
		PROC_VMSPACE_UNLOCK(p);
		pmap_activate(td);
		vmspace_dofree(vm);
	}
	vmspace_container_reset(p);
}

/* Acquire reference to vmspace owned by another process. */

struct vmspace *
vmspace_acquire_ref(struct proc *p)
{
	struct vmspace *vm;
	int refcnt;

	PROC_VMSPACE_LOCK(p);
	vm = p->p_vmspace;
	if (vm == NULL) {
		PROC_VMSPACE_UNLOCK(p);
		return (NULL);
	}
	do {
		refcnt = vm->vm_refcnt;
		if (refcnt <= 0) { 	/* Avoid 0->1 transition */
			PROC_VMSPACE_UNLOCK(p);
			return (NULL);
		}
	} while (!atomic_cmpset_int(&vm->vm_refcnt, refcnt, refcnt + 1));
	if (vm != p->p_vmspace) {
		PROC_VMSPACE_UNLOCK(p);
		vmspace_free(vm);
		return (NULL);
	}
	PROC_VMSPACE_UNLOCK(p);
	return (vm);
}

void
_vm_map_lock(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		mtx_lock_flags_(&map->system_mtx, 0, file, line);
	else
		sx_xlock_(&map->lock, file, line);
	map->timestamp++;
}

static void
vm_map_process_deferred(void)
{
	struct thread *td;
	vm_map_entry_t entry, next;
	vm_object_t object;

	td = curthread;
	entry = td->td_map_def_user;
	td->td_map_def_user = NULL;
	while (entry != NULL) {
		next = entry->next;
		if ((entry->eflags & MAP_ENTRY_VN_WRITECNT) != 0) {
			/*
			 * Decrement the object's writemappings and
			 * possibly the vnode's v_writecount.
			 */
			KASSERT((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0,
			    ("Submap with writecount"));
			object = entry->object.vm_object;
			KASSERT(object != NULL, ("No object for writecount"));
			vnode_pager_release_writecount(object, entry->start,
			    entry->end);
		}
		vm_map_entry_deallocate(entry, FALSE);
		entry = next;
	}
}

void
_vm_map_unlock(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	else {
		sx_xunlock_(&map->lock, file, line);
		vm_map_process_deferred();
	}
}

void
_vm_map_lock_read(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		mtx_lock_flags_(&map->system_mtx, 0, file, line);
	else
		sx_slock_(&map->lock, file, line);
}

void
_vm_map_unlock_read(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	else {
		sx_sunlock_(&map->lock, file, line);
		vm_map_process_deferred();
	}
}

int
_vm_map_trylock(vm_map_t map, const char *file, int line)
{
	int error;

	error = map->system_map ?
	    !mtx_trylock_flags_(&map->system_mtx, 0, file, line) :
	    !sx_try_xlock_(&map->lock, file, line);
	if (error == 0)
		map->timestamp++;
	return (error == 0);
}

int
_vm_map_trylock_read(vm_map_t map, const char *file, int line)
{
	int error;

	error = map->system_map ?
	    !mtx_trylock_flags_(&map->system_mtx, 0, file, line) :
	    !sx_try_slock_(&map->lock, file, line);
	return (error == 0);
}

/*
 *	_vm_map_lock_upgrade:	[ internal use only ]
 *
 *	Tries to upgrade a read (shared) lock on the specified map to a write
 *	(exclusive) lock.  Returns the value "0" if the upgrade succeeds and a
 *	non-zero value if the upgrade fails.  If the upgrade fails, the map is
 *	returned without a read or write lock held.
 *
 *	Requires that the map be read locked.
 */
int
_vm_map_lock_upgrade(vm_map_t map, const char *file, int line)
{
	unsigned int last_timestamp;

	if (map->system_map) {
		mtx_assert_(&map->system_mtx, MA_OWNED, file, line);
	} else {
		if (!sx_try_upgrade_(&map->lock, file, line)) {
			last_timestamp = map->timestamp;
			sx_sunlock_(&map->lock, file, line);
			vm_map_process_deferred();
			/*
			 * If the map's timestamp does not change while the
			 * map is unlocked, then the upgrade succeeds.
			 */
			sx_xlock_(&map->lock, file, line);
			if (last_timestamp != map->timestamp) {
				sx_xunlock_(&map->lock, file, line);
				return (1);
			}
		}
	}
	map->timestamp++;
	return (0);
}

void
_vm_map_lock_downgrade(vm_map_t map, const char *file, int line)
{

	if (map->system_map) {
		mtx_assert_(&map->system_mtx, MA_OWNED, file, line);
	} else
		sx_downgrade_(&map->lock, file, line);
}

/*
 *	vm_map_locked:
 *
 *	Returns a non-zero value if the caller holds a write (exclusive) lock
 *	on the specified map and the value "0" otherwise.
 */
int
vm_map_locked(vm_map_t map)
{

	if (map->system_map)
		return (mtx_owned(&map->system_mtx));
	else
		return (sx_xlocked(&map->lock));
}

#ifdef INVARIANTS
static void
_vm_map_assert_locked(vm_map_t map, const char *file, int line)
{

	if (map->system_map)
		mtx_assert_(&map->system_mtx, MA_OWNED, file, line);
	else
		sx_assert_(&map->lock, SA_XLOCKED, file, line);
}

#define	VM_MAP_ASSERT_LOCKED(map) \
    _vm_map_assert_locked(map, LOCK_FILE, LOCK_LINE)
#else
#define	VM_MAP_ASSERT_LOCKED(map)
#endif

/*
 *	_vm_map_unlock_and_wait:
 *
 *	Atomically releases the lock on the specified map and puts the calling
 *	thread to sleep.  The calling thread will remain asleep until either
 *	vm_map_wakeup() is performed on the map or the specified timeout is
 *	exceeded.
 *
 *	WARNING!  This function does not perform deferred deallocations of
 *	objects and map	entries.  Therefore, the calling thread is expected to
 *	reacquire the map lock after reawakening and later perform an ordinary
 *	unlock operation, such as vm_map_unlock(), before completing its
 *	operation on the map.
 */
int
_vm_map_unlock_and_wait(vm_map_t map, int timo, const char *file, int line)
{

	mtx_lock(&map_sleep_mtx);
	if (map->system_map)
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	else
		sx_xunlock_(&map->lock, file, line);
	return (msleep(&map->root, &map_sleep_mtx, PDROP | PVM, "vmmaps",
	    timo));
}

/*
 *	vm_map_wakeup:
 *
 *	Awaken any threads that have slept on the map using
 *	vm_map_unlock_and_wait().
 */
void
vm_map_wakeup(vm_map_t map)
{

	/*
	 * Acquire and release map_sleep_mtx to prevent a wakeup()
	 * from being performed (and lost) between the map unlock
	 * and the msleep() in _vm_map_unlock_and_wait().
	 */
	mtx_lock(&map_sleep_mtx);
	mtx_unlock(&map_sleep_mtx);
	wakeup(&map->root);
}

void
vm_map_busy(vm_map_t map)
{

	VM_MAP_ASSERT_LOCKED(map);
	map->busy++;
}

void
vm_map_unbusy(vm_map_t map)
{

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT(map->busy, ("vm_map_unbusy: not busy"));
	if (--map->busy == 0 && (map->flags & MAP_BUSY_WAKEUP)) {
		vm_map_modflags(map, 0, MAP_BUSY_WAKEUP);
		wakeup(&map->busy);
	}
}

void 
vm_map_wait_busy(vm_map_t map)
{

	VM_MAP_ASSERT_LOCKED(map);
	while (map->busy) {
		vm_map_modflags(map, MAP_BUSY_WAKEUP, 0);
		if (map->system_map)
			msleep(&map->busy, &map->system_mtx, 0, "mbusy", 0);
		else
			sx_sleep(&map->busy, &map->lock, 0, "mbusy", 0);
	}
	map->timestamp++;
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
	_vm_map_init(result, pmap, min, max);
	return (result);
}

/*
 * Initialize an existing vm_map structure
 * such as that in the vmspace structure.
 */
static void
_vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min, vm_offset_t max)
{

	map->header.next = map->header.prev = &map->header;
	map->needs_wakeup = FALSE;
	map->system_map = 0;
	map->pmap = pmap;
	map->min_offset = min;
	map->max_offset = max;
	map->flags = 0;
	map->root = NULL;
	map->timestamp = 0;
	map->busy = 0;
}

void
vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min, vm_offset_t max)
{

	_vm_map_init(map, pmap, min, max);
	mtx_init(&map->system_mtx, "system map", NULL, MTX_DEF | MTX_DUPOK);
	sx_init(&map->lock, "user map");
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
static inline void
vm_map_entry_set_behavior(vm_map_entry_t entry, u_char behavior)
{
	entry->eflags = (entry->eflags & ~MAP_ENTRY_BEHAV_MASK) |
	    (behavior & MAP_ENTRY_BEHAV_MASK);
}

/*
 *	vm_map_entry_set_max_free:
 *
 *	Set the max_free field in a vm_map_entry.
 */
static inline void
vm_map_entry_set_max_free(vm_map_entry_t entry)
{

	entry->max_free = entry->adj_free;
	if (entry->left != NULL && entry->left->max_free > entry->max_free)
		entry->max_free = entry->left->max_free;
	if (entry->right != NULL && entry->right->max_free > entry->max_free)
		entry->max_free = entry->right->max_free;
}

/*
 *	vm_map_entry_splay:
 *
 *	The Sleator and Tarjan top-down splay algorithm with the
 *	following variation.  Max_free must be computed bottom-up, so
 *	on the downward pass, maintain the left and right spines in
 *	reverse order.  Then, make a second pass up each side to fix
 *	the pointers and compute max_free.  The time bound is O(log n)
 *	amortized.
 *
 *	The new root is the vm_map_entry containing "addr", or else an
 *	adjacent entry (lower or higher) if addr is not in the tree.
 *
 *	The map must be locked, and leaves it so.
 *
 *	Returns: the new root.
 */
static vm_map_entry_t
vm_map_entry_splay(vm_offset_t addr, vm_map_entry_t root)
{
	vm_map_entry_t llist, rlist;
	vm_map_entry_t ltree, rtree;
	vm_map_entry_t y;

	/* Special case of empty tree. */
	if (root == NULL)
		return (root);

	/*
	 * Pass One: Splay down the tree until we find addr or a NULL
	 * pointer where addr would go.  llist and rlist are the two
	 * sides in reverse order (bottom-up), with llist linked by
	 * the right pointer and rlist linked by the left pointer in
	 * the vm_map_entry.  Wait until Pass Two to set max_free on
	 * the two spines.
	 */
	llist = NULL;
	rlist = NULL;
	for (;;) {
		/* root is never NULL in here. */
		if (addr < root->start) {
			y = root->left;
			if (y == NULL)
				break;
			if (addr < y->start && y->left != NULL) {
				/* Rotate right and put y on rlist. */
				root->left = y->right;
				y->right = root;
				vm_map_entry_set_max_free(root);
				root = y->left;
				y->left = rlist;
				rlist = y;
			} else {
				/* Put root on rlist. */
				root->left = rlist;
				rlist = root;
				root = y;
			}
		} else if (addr >= root->end) {
			y = root->right;
			if (y == NULL)
				break;
			if (addr >= y->end && y->right != NULL) {
				/* Rotate left and put y on llist. */
				root->right = y->left;
				y->left = root;
				vm_map_entry_set_max_free(root);
				root = y->right;
				y->right = llist;
				llist = y;
			} else {
				/* Put root on llist. */
				root->right = llist;
				llist = root;
				root = y;
			}
		} else
			break;
	}

	/*
	 * Pass Two: Walk back up the two spines, flip the pointers
	 * and set max_free.  The subtrees of the root go at the
	 * bottom of llist and rlist.
	 */
	ltree = root->left;
	while (llist != NULL) {
		y = llist->right;
		llist->right = ltree;
		vm_map_entry_set_max_free(llist);
		ltree = llist;
		llist = y;
	}
	rtree = root->right;
	while (rlist != NULL) {
		y = rlist->left;
		rlist->left = rtree;
		vm_map_entry_set_max_free(rlist);
		rtree = rlist;
		rlist = y;
	}

	/*
	 * Final assembly: add ltree and rtree as subtrees of root.
	 */
	root->left = ltree;
	root->right = rtree;
	vm_map_entry_set_max_free(root);

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
	VM_MAP_ASSERT_LOCKED(map);
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
		after_where->adj_free = entry->start - after_where->end;
		vm_map_entry_set_max_free(after_where);
	} else {
		entry->right = map->root;
		entry->left = NULL;
	}
	entry->adj_free = (entry->next == &map->header ? map->max_offset :
	    entry->next->start) - entry->end;
	vm_map_entry_set_max_free(entry);
	map->root = entry;
}

static void
vm_map_entry_unlink(vm_map_t map,
		    vm_map_entry_t entry)
{
	vm_map_entry_t next, prev, root;

	VM_MAP_ASSERT_LOCKED(map);
	if (entry != map->root)
		vm_map_entry_splay(entry->start, map->root);
	if (entry->left == NULL)
		root = entry->right;
	else {
		root = vm_map_entry_splay(entry->start, entry->left);
		root->right = entry->right;
		root->adj_free = (entry->next == &map->header ? map->max_offset :
		    entry->next->start) - root->end;
		vm_map_entry_set_max_free(root);
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
 *	vm_map_entry_resize_free:
 *
 *	Recompute the amount of free space following a vm_map_entry
 *	and propagate that value up the tree.  Call this function after
 *	resizing a map entry in-place, that is, without a call to
 *	vm_map_entry_link() or _unlink().
 *
 *	The map must be locked, and leaves it so.
 */
static void
vm_map_entry_resize_free(vm_map_t map, vm_map_entry_t entry)
{

	/*
	 * Using splay trees without parent pointers, propagating
	 * max_free up the tree is done by moving the entry to the
	 * root and making the change there.
	 */
	if (entry != map->root)
		map->root = vm_map_entry_splay(entry->start, map->root);

	entry->adj_free = (entry->next == &map->header ? map->max_offset :
	    entry->next->start) - entry->end;
	vm_map_entry_set_max_free(entry);
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
	boolean_t locked;

	/*
	 * If the map is empty, then the map entry immediately preceding
	 * "address" is the map's header.
	 */
	cur = map->root;
	if (cur == NULL)
		*entry = &map->header;
	else if (address >= cur->start && cur->end > address) {
		*entry = cur;
		return (TRUE);
	} else if ((locked = vm_map_locked(map)) ||
	    sx_try_upgrade(&map->lock)) {
		/*
		 * Splay requires a write lock on the map.  However, it only
		 * restructures the binary search tree; it does not otherwise
		 * change the map.  Thus, the map's timestamp need not change
		 * on a temporary upgrade.
		 */
		map->root = cur = vm_map_entry_splay(address, cur);
		if (!locked)
			sx_downgrade(&map->lock);

		/*
		 * If "address" is contained within a map entry, the new root
		 * is that map entry.  Otherwise, the new root is a map entry
		 * immediately before or after "address".
		 */
		if (address >= cur->start) {
			*entry = cur;
			if (cur->end > address)
				return (TRUE);
		} else
			*entry = cur->prev;
	} else
		/*
		 * Since the map is only locked for read access, perform a
		 * standard binary search tree lookup for "address".
		 */
		for (;;) {
			if (address < cur->start) {
				if (cur->left == NULL) {
					*entry = cur->prev;
					break;
				}
				cur = cur->left;
			} else if (cur->end > address) {
				*entry = cur;
				return (TRUE);
			} else {
				if (cur->right == NULL) {
					*entry = cur;
					break;
				}
				cur = cur->right;
			}
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
	struct ucred *cred;
	vm_inherit_t inheritance;
	boolean_t charge_prev_obj;

	VM_MAP_ASSERT_LOCKED(map);

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
	charge_prev_obj = FALSE;

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
	if (cow & MAP_VN_WRITECOUNT)
		protoeflags |= MAP_ENTRY_VN_WRITECNT;
	if (cow & MAP_INHERIT_SHARE)
		inheritance = VM_INHERIT_SHARE;
	else
		inheritance = VM_INHERIT_DEFAULT;

	cred = NULL;
	KASSERT((object != kmem_object && object != kernel_object) ||
	    ((object == kmem_object || object == kernel_object) &&
		!(protoeflags & MAP_ENTRY_NEEDS_COPY)),
	    ("kmem or kernel object and cow"));
	if (cow & (MAP_ACC_NO_CHARGE | MAP_NOFAULT))
		goto charged;
	if ((cow & MAP_ACC_CHARGED) || ((prot & VM_PROT_WRITE) &&
	    ((protoeflags & MAP_ENTRY_NEEDS_COPY) || object == NULL))) {
		if (!(cow & MAP_ACC_CHARGED) && !swap_reserve(end - start))
			return (KERN_RESOURCE_SHORTAGE);
		KASSERT(object == NULL || (protoeflags & MAP_ENTRY_NEEDS_COPY) ||
		    object->cred == NULL,
		    ("OVERCOMMIT: vm_map_insert o %p", object));
		cred = curthread->td_ucred;
		crhold(cred);
		if (object == NULL && !(protoeflags & MAP_ENTRY_NEEDS_COPY))
			charge_prev_obj = TRUE;
	}

charged:
	/* Expand the kernel pmap, if necessary. */
	if (map == kernel_map && end > kernel_vm_end)
		pmap_growkernel(end);
	if (object != NULL) {
		/*
		 * OBJ_ONEMAPPING must be cleared unless this mapping
		 * is trivially proven to be the only mapping for any
		 * of the object's pages.  (Object granularity
		 * reference counting is insufficient to recognize
		 * aliases with precision.)
		 */
		VM_OBJECT_WLOCK(object);
		if (object->ref_count > 1 || object->shadow_count != 0)
			vm_object_clear_flag(object, OBJ_ONEMAPPING);
		VM_OBJECT_WUNLOCK(object);
	}
	else if ((prev_entry != &map->header) &&
		 (prev_entry->eflags == protoeflags) &&
		 (prev_entry->end == start) &&
		 (prev_entry->wired_count == 0) &&
		 (prev_entry->cred == cred ||
		  (prev_entry->object.vm_object != NULL &&
		   (prev_entry->object.vm_object->cred == cred))) &&
		   vm_object_coalesce(prev_entry->object.vm_object,
		       prev_entry->offset,
		       (vm_size_t)(prev_entry->end - prev_entry->start),
		       (vm_size_t)(end - prev_entry->end), charge_prev_obj)) {
		/*
		 * We were able to extend the object.  Determine if we
		 * can extend the previous map entry to include the
		 * new range as well.
		 */
		if ((prev_entry->inheritance == inheritance) &&
		    (prev_entry->protection == prot) &&
		    (prev_entry->max_protection == max)) {
			map->size += (end - prev_entry->end);
			prev_entry->end = end;
			vm_map_entry_resize_free(map, prev_entry);
			vm_map_simplify_entry(map, prev_entry);
			if (cred != NULL)
				crfree(cred);
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
		if (cred != NULL && object != NULL && object->cred != NULL &&
		    !(prev_entry->eflags & MAP_ENTRY_NEEDS_COPY)) {
			/* Object already accounts for this uid. */
			crfree(cred);
			cred = NULL;
		}
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
	new_entry->cred = NULL;

	new_entry->eflags = protoeflags;
	new_entry->object.vm_object = object;
	new_entry->offset = offset;
	new_entry->avail_ssize = 0;

	new_entry->inheritance = inheritance;
	new_entry->protection = prot;
	new_entry->max_protection = max;
	new_entry->wired_count = 0;
	new_entry->read_ahead = VM_FAULT_READ_AHEAD_INIT;
	new_entry->next_read = OFF_TO_IDX(offset);

	KASSERT(cred == NULL || !ENTRY_CHARGED(new_entry),
	    ("OVERCOMMIT: vm_map_insert leaks vm_map %p", new_entry));
	new_entry->cred = cred;

	/*
	 * Insert the new entry into the list
	 */
	vm_map_entry_link(map, prev_entry, new_entry);
	map->size += new_entry->end - new_entry->start;

	/*
	 * It may be possible to merge the new entry with the next and/or
	 * previous entries.  However, due to MAP_STACK_* being a hack, a
	 * panic can result from merging such entries.
	 */
	if ((cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) == 0)
		vm_map_simplify_entry(map, new_entry);

	if (cow & (MAP_PREFAULT|MAP_PREFAULT_PARTIAL)) {
		vm_map_pmap_enter(map, start, prot,
				    object, OFF_TO_IDX(offset), end - start,
				    cow & MAP_PREFAULT_PARTIAL);
	}

	return (KERN_SUCCESS);
}

/*
 *	vm_map_findspace:
 *
 *	Find the first fit (lowest VM address) for "length" free bytes
 *	beginning at address >= start in the given map.
 *
 *	In a vm_map_entry, "adj_free" is the amount of free space
 *	adjacent (higher address) to this entry, and "max_free" is the
 *	maximum amount of contiguous free space in its subtree.  This
 *	allows finding a free region in one path down the tree, so
 *	O(log n) amortized with splay trees.
 *
 *	The map must be locked, and leaves it so.
 *
 *	Returns: 0 on success, and starting address in *addr,
 *		 1 if insufficient space.
 */
int
vm_map_findspace(vm_map_t map, vm_offset_t start, vm_size_t length,
    vm_offset_t *addr)	/* OUT */
{
	vm_map_entry_t entry;
	vm_offset_t st;

	/*
	 * Request must fit within min/max VM address and must avoid
	 * address wrap.
	 */
	if (start < map->min_offset)
		start = map->min_offset;
	if (start + length > map->max_offset || start + length < start)
		return (1);

	/* Empty tree means wide open address space. */
	if (map->root == NULL) {
		*addr = start;
		return (0);
	}

	/*
	 * After splay, if start comes before root node, then there
	 * must be a gap from start to the root.
	 */
	map->root = vm_map_entry_splay(start, map->root);
	if (start + length <= map->root->start) {
		*addr = start;
		return (0);
	}

	/*
	 * Root is the last node that might begin its gap before
	 * start, and this is the last comparison where address
	 * wrap might be a problem.
	 */
	st = (start > map->root->end) ? start : map->root->end;
	if (length <= map->root->end + map->root->adj_free - st) {
		*addr = st;
		return (0);
	}

	/* With max_free, can immediately tell if no solution. */
	entry = map->root->right;
	if (entry == NULL || length > entry->max_free)
		return (1);

	/*
	 * Search the right subtree in the order: left subtree, root,
	 * right subtree (first fit).  The previous splay implies that
	 * all regions in the right subtree have addresses > start.
	 */
	while (entry != NULL) {
		if (entry->left != NULL && entry->left->max_free >= length)
			entry = entry->left;
		else if (entry->adj_free >= length) {
			*addr = entry->end;
			return (0);
		} else
			entry = entry->right;
	}

	/* Can't get here, so panic if we do. */
	panic("vm_map_findspace: max_free corrupt");
}

int
vm_map_fixed(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t start, vm_size_t length, vm_prot_t prot,
    vm_prot_t max, int cow)
{
	vm_offset_t end;
	int result;

	end = start + length;
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	(void) vm_map_delete(map, start, end);
	result = vm_map_insert(map, object, offset, start, end, prot,
	    max, cow);
	vm_map_unlock(map);
	return (result);
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
	    vm_size_t length, vm_offset_t max_addr, int find_space,
	    vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_offset_t alignment, initial_addr, start;
	int result;

	if (find_space == VMFS_OPTIMAL_SPACE && (object == NULL ||
	    (object->flags & OBJ_COLORED) == 0))
		find_space = VMFS_ANY_SPACE;
	if (find_space >> 8 != 0) {
		KASSERT((find_space & 0xff) == 0, ("bad VMFS flags"));
		alignment = (vm_offset_t)1 << (find_space >> 8);
	} else
		alignment = 0;
	initial_addr = *addr;
again:
	start = initial_addr;
	vm_map_lock(map);
	do {
		if (find_space != VMFS_NO_SPACE) {
			if (vm_map_findspace(map, start, length, addr) ||
			    (max_addr != 0 && *addr + length > max_addr)) {
				vm_map_unlock(map);
				if (find_space == VMFS_OPTIMAL_SPACE) {
					find_space = VMFS_ANY_SPACE;
					goto again;
				}
				return (KERN_NO_SPACE);
			}
			switch (find_space) {
			case VMFS_SUPER_SPACE:
			case VMFS_OPTIMAL_SPACE:
				pmap_align_superpage(object, offset, addr,
				    length);
				break;
			case VMFS_ANY_SPACE:
				break;
			default:
				if ((*addr & (alignment - 1)) != 0) {
					*addr &= ~(alignment - 1);
					*addr += alignment;
				}
				break;
			}

			start = *addr;
		}
		result = vm_map_insert(map, object, offset, start, start +
		    length, prot, max, cow);
	} while (result == KERN_NO_SPACE && find_space != VMFS_NO_SPACE &&
	    find_space != VMFS_ANY_SPACE);
	vm_map_unlock(map);
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
		     (prev->wired_count == entry->wired_count) &&
		     (prev->cred == entry->cred)) {
			vm_map_entry_unlink(map, prev);
			entry->start = prev->start;
			entry->offset = prev->offset;
			if (entry->prev != &map->header)
				vm_map_entry_resize_free(map, entry->prev);

			/*
			 * If the backing object is a vnode object,
			 * vm_object_deallocate() calls vrele().
			 * However, vrele() does not lock the vnode
			 * because the vnode has additional
			 * references.  Thus, the map lock can be kept
			 * without causing a lock-order reversal with
			 * the vnode lock.
			 *
			 * Since we count the number of virtual page
			 * mappings in object->un_pager.vnp.writemappings,
			 * the writemappings value should not be adjusted
			 * when the entry is disposed of.
			 */
			if (prev->object.vm_object)
				vm_object_deallocate(prev->object.vm_object);
			if (prev->cred != NULL)
				crfree(prev->cred);
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
		    (next->wired_count == entry->wired_count) &&
		    (next->cred == entry->cred)) {
			vm_map_entry_unlink(map, next);
			entry->end = next->end;
			vm_map_entry_resize_free(map, entry);

			/*
			 * See comment above.
			 */
			if (next->object.vm_object)
				vm_object_deallocate(next->object.vm_object);
			if (next->cred != NULL)
				crfree(next->cred);
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

	VM_MAP_ASSERT_LOCKED(map);

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
		if (entry->cred != NULL) {
			object->cred = entry->cred;
			object->charge = entry->end - entry->start;
			entry->cred = NULL;
		}
	} else if (entry->object.vm_object != NULL &&
		   ((entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) &&
		   entry->cred != NULL) {
		VM_OBJECT_WLOCK(entry->object.vm_object);
		KASSERT(entry->object.vm_object->cred == NULL,
		    ("OVERCOMMIT: vm_entry_clip_start: both cred e %p", entry));
		entry->object.vm_object->cred = entry->cred;
		entry->object.vm_object->charge = entry->end - entry->start;
		VM_OBJECT_WUNLOCK(entry->object.vm_object);
		entry->cred = NULL;
	}

	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->end = start;
	entry->offset += (start - entry->start);
	entry->start = start;
	if (new_entry->cred != NULL)
		crhold(entry->cred);

	vm_map_entry_link(map, entry->prev, new_entry);

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_reference(new_entry->object.vm_object);
		/*
		 * The object->un_pager.vnp.writemappings for the
		 * object of MAP_ENTRY_VN_WRITECNT type entry shall be
		 * kept as is here.  The virtual pages are
		 * re-distributed among the clipped entries, so the sum is
		 * left the same.
		 */
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

	VM_MAP_ASSERT_LOCKED(map);

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
		if (entry->cred != NULL) {
			object->cred = entry->cred;
			object->charge = entry->end - entry->start;
			entry->cred = NULL;
		}
	} else if (entry->object.vm_object != NULL &&
		   ((entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) &&
		   entry->cred != NULL) {
		VM_OBJECT_WLOCK(entry->object.vm_object);
		KASSERT(entry->object.vm_object->cred == NULL,
		    ("OVERCOMMIT: vm_entry_clip_end: both cred e %p", entry));
		entry->object.vm_object->cred = entry->cred;
		entry->object.vm_object->charge = entry->end - entry->start;
		VM_OBJECT_WUNLOCK(entry->object.vm_object);
		entry->cred = NULL;
	}

	/*
	 * Create a new entry and insert it AFTER the specified entry
	 */
	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;

	new_entry->start = entry->end = end;
	new_entry->offset += (end - entry->start);
	if (new_entry->cred != NULL)
		crhold(entry->cred);

	vm_map_entry_link(map, entry, new_entry);

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_reference(new_entry->object.vm_object);
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
 * The maximum number of pages to map
 */
#define	MAX_INIT_PT	96

/*
 *	vm_map_pmap_enter:
 *
 *	Preload read-only mappings for the specified object's resident pages
 *	into the target map.  If "flags" is MAP_PREFAULT_PARTIAL, then only
 *	the resident pages within the address range [addr, addr + ulmin(size,
 *	ptoa(MAX_INIT_PT))) are mapped.  Otherwise, all resident pages within
 *	the specified address range are mapped.  This eliminates many soft
 *	faults on process startup and immediately after an mmap(2).  Because
 *	these are speculative mappings, cached pages are not reactivated and
 *	mapped.
 */
void
vm_map_pmap_enter(vm_map_t map, vm_offset_t addr, vm_prot_t prot,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size, int flags)
{
	vm_offset_t start;
	vm_page_t p, p_start;
	vm_pindex_t psize, tmpidx;

	if ((prot & (VM_PROT_READ | VM_PROT_EXECUTE)) == 0 || object == NULL)
		return;
	VM_OBJECT_RLOCK(object);
	if (object->type == OBJT_DEVICE || object->type == OBJT_SG) {
		VM_OBJECT_RUNLOCK(object);
		VM_OBJECT_WLOCK(object);
		if (object->type == OBJT_DEVICE || object->type == OBJT_SG) {
			pmap_object_init_pt(map->pmap, addr, object, pindex,
			    size);
			VM_OBJECT_WUNLOCK(object);
			return;
		}
		VM_OBJECT_LOCK_DOWNGRADE(object);
	}

	psize = atop(size);
	if (psize > MAX_INIT_PT && (flags & MAP_PREFAULT_PARTIAL) != 0)
		psize = MAX_INIT_PT;
	if (psize + pindex > object->size) {
		if (object->size < pindex) {
			VM_OBJECT_RUNLOCK(object);
			return;
		}
		psize = object->size - pindex;
	}

	start = 0;
	p_start = NULL;

	p = vm_page_find_least(object, pindex);
	/*
	 * Assert: the variable p is either (1) the page with the
	 * least pindex greater than or equal to the parameter pindex
	 * or (2) NULL.
	 */
	for (;
	     p != NULL && (tmpidx = p->pindex - pindex) < psize;
	     p = TAILQ_NEXT(p, listq)) {
		/*
		 * don't allow an madvise to blow away our really
		 * free pages allocating pv entries.
		 */
		if ((flags & MAP_PREFAULT_MADVISE) &&
		    cnt.v_free_count < cnt.v_free_reserved) {
			psize = tmpidx;
			break;
		}
		if (p->valid == VM_PAGE_BITS_ALL) {
			if (p_start == NULL) {
				start = addr + ptoa(tmpidx);
				p_start = p;
			}
		} else if (p_start != NULL) {
			pmap_enter_object(map->pmap, start, addr +
			    ptoa(tmpidx), p_start, prot);
			p_start = NULL;
		}
	}
	if (p_start != NULL)
		pmap_enter_object(map->pmap, start, addr + ptoa(psize),
		    p_start, prot);
	VM_OBJECT_RUNLOCK(object);
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
	vm_map_entry_t current, entry;
	vm_object_t obj;
	struct ucred *cred;
	vm_prot_t old_prot;

	if (start == end)
		return (KERN_SUCCESS);

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
	 * Do an accounting pass for private read-only mappings that
	 * now will do cow due to allowed write (e.g. debugger sets
	 * breakpoint on text segment)
	 */
	for (current = entry; (current != &map->header) &&
	     (current->start < end); current = current->next) {

		vm_map_clip_end(map, current, end);

		if (set_max ||
		    ((new_prot & ~(current->protection)) & VM_PROT_WRITE) == 0 ||
		    ENTRY_CHARGED(current)) {
			continue;
		}

		cred = curthread->td_ucred;
		obj = current->object.vm_object;

		if (obj == NULL || (current->eflags & MAP_ENTRY_NEEDS_COPY)) {
			if (!swap_reserve(current->end - current->start)) {
				vm_map_unlock(map);
				return (KERN_RESOURCE_SHORTAGE);
			}
			crhold(cred);
			current->cred = cred;
			continue;
		}

		VM_OBJECT_WLOCK(obj);
		if (obj->type != OBJT_DEFAULT && obj->type != OBJT_SWAP) {
			VM_OBJECT_WUNLOCK(obj);
			continue;
		}

		/*
		 * Charge for the whole object allocation now, since
		 * we cannot distinguish between non-charged and
		 * charged clipped mapping of the same object later.
		 */
		KASSERT(obj->charge == 0,
		    ("vm_map_protect: object %p overcharged\n", obj));
		if (!swap_reserve(ptoa(obj->size))) {
			VM_OBJECT_WUNLOCK(obj);
			vm_map_unlock(map);
			return (KERN_RESOURCE_SHORTAGE);
		}

		crhold(cred);
		obj->cred = cred;
		obj->charge = ptoa(obj->size);
		VM_OBJECT_WUNLOCK(obj);
	}

	/*
	 * Go back and fix up protections. [Note that clipping is not
	 * necessary the second time.]
	 */
	current = entry;
	while ((current != &map->header) && (current->start < end)) {
		old_prot = current->protection;

		if (set_max)
			current->protection =
			    (current->max_protection = new_prot) &
			    old_prot;
		else
			current->protection = new_prot;

		if ((current->eflags & (MAP_ENTRY_COW | MAP_ENTRY_USER_WIRED))
		     == (MAP_ENTRY_COW | MAP_ENTRY_USER_WIRED) &&
		    (current->protection & VM_PROT_WRITE) != 0 &&
		    (old_prot & VM_PROT_WRITE) == 0) {
			vm_fault_copy_entry(map, map, current, current, NULL);
		}

		/*
		 * When restricting access, update the physical map.  Worry
		 * about copy-on-write here.
		 */
		if ((old_prot & ~current->protection) != 0) {
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
 *	This routine traverses a processes map handling the madvise
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
		if (start == end)
			return (KERN_SUCCESS);
		modify_map = 1;
		vm_map_lock(map);
		break;
	case MADV_WILLNEED:
	case MADV_DONTNEED:
	case MADV_FREE:
		if (start == end)
			return (KERN_SUCCESS);
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
		vm_pindex_t pstart, pend;

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
			vm_offset_t useEnd, useStart;

			if (current->eflags & MAP_ENTRY_IS_SUB_MAP)
				continue;

			pstart = OFF_TO_IDX(current->offset);
			pend = pstart + atop(current->end - current->start);
			useStart = current->start;
			useEnd = current->end;

			if (current->start < start) {
				pstart += atop(start - current->start);
				useStart = start;
			}
			if (current->end > end) {
				pend -= atop(current->end - end);
				useEnd = end;
			}

			if (pstart >= pend)
				continue;

			/*
			 * Perform the pmap_advise() before clearing
			 * PGA_REFERENCED in vm_page_advise().  Otherwise, a
			 * concurrent pmap operation, such as pmap_remove(),
			 * could clear a reference in the pmap and set
			 * PGA_REFERENCED on the page before the pmap_advise()
			 * had completed.  Consequently, the page would appear
			 * referenced based upon an old reference that
			 * occurred before this pmap_advise() ran.
			 */
			if (behav == MADV_DONTNEED || behav == MADV_FREE)
				pmap_advise(map->pmap, useStart, useEnd,
				    behav);

			vm_object_madvise(current->object.vm_object, pstart,
			    pend, behav);
			if (behav == MADV_WILLNEED) {
				vm_map_pmap_enter(map,
				    useStart,
				    current->protection,
				    current->object.vm_object,
				    pstart,
				    ptoa(pend - pstart),
				    MAP_PREFAULT_MADVISE
				);
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
 *	child maps at the time of vmspace_fork.
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
	if (start == end)
		return (KERN_SUCCESS);
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
    int flags)
{
	vm_map_entry_t entry, first_entry, tmp_entry;
	vm_offset_t saved_start;
	unsigned int last_timestamp;
	int rv;
	boolean_t need_wakeup, result, user_unwire;

	if (start == end)
		return (KERN_SUCCESS);
	user_unwire = (flags & VM_MAP_WIRE_USER) ? TRUE : FALSE;
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		if (flags & VM_MAP_WIRE_HOLESOK)
			first_entry = first_entry->next;
		else {
			vm_map_unlock(map);
			return (KERN_INVALID_ADDRESS);
		}
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
			if (vm_map_unlock_and_wait(map, 0)) {
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
					if (flags & VM_MAP_WIRE_HOLESOK)
						tmp_entry = tmp_entry->next;
					else {
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
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 &&
		    entry->wiring_thread == NULL,
		    ("owned map entry %p", entry));
		entry->eflags |= MAP_ENTRY_IN_TRANSITION;
		entry->wiring_thread = curthread;
		/*
		 * Check the map for holes in the specified region.
		 * If VM_MAP_WIRE_HOLESOK was specified, skip this check.
		 */
		if (((flags & VM_MAP_WIRE_HOLESOK) == 0) &&
		    (entry->end < end && (entry->next == &map->header ||
		    entry->next->start > entry->end))) {
			end = entry->end;
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}
		/*
		 * If system unwiring, require that the entry is system wired.
		 */
		if (!user_unwire &&
		    vm_map_entry_system_wired_count(entry) == 0) {
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
		if (!result && (flags & VM_MAP_WIRE_HOLESOK))
			first_entry = first_entry->next;
		else
			KASSERT(result, ("vm_map_unwire: lookup failed"));
	}
	for (entry = first_entry; entry != &map->header && entry->start < end;
	    entry = entry->next) {
		/*
		 * If VM_MAP_WIRE_HOLESOK was specified, an empty
		 * space in the unwired region could have been mapped
		 * while the map lock was dropped for draining
		 * MAP_ENTRY_IN_TRANSITION.  Moreover, another thread
		 * could be simultaneously wiring this new mapping
		 * entry.  Detect these cases and skip any entries
		 * marked as in transition by us.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 ||
		    entry->wiring_thread != curthread) {
			KASSERT((flags & VM_MAP_WIRE_HOLESOK) != 0,
			    ("vm_map_unwire: !HOLESOK and new/changed entry"));
			continue;
		}

		if (rv == KERN_SUCCESS && (!user_unwire ||
		    (entry->eflags & MAP_ENTRY_USER_WIRED))) {
			if (user_unwire)
				entry->eflags &= ~MAP_ENTRY_USER_WIRED;
			entry->wired_count--;
			if (entry->wired_count == 0) {
				/*
				 * Retain the map lock.
				 */
				vm_fault_unwire(map, entry->start, entry->end,
				    entry->object.vm_object != NULL &&
				    (entry->object.vm_object->flags &
				    OBJ_FICTITIOUS) != 0);
			}
		}
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0,
		    ("vm_map_unwire: in-transition flag missing %p", entry));
		KASSERT(entry->wiring_thread == curthread,
		    ("vm_map_unwire: alien wire %p", entry));
		entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
		entry->wiring_thread = NULL;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = TRUE;
		}
		vm_map_simplify_entry(map, entry);
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
    int flags)
{
	vm_map_entry_t entry, first_entry, tmp_entry;
	vm_offset_t saved_end, saved_start;
	unsigned int last_timestamp;
	int rv;
	boolean_t fictitious, need_wakeup, result, user_wire;
	vm_prot_t prot;

	if (start == end)
		return (KERN_SUCCESS);
	prot = 0;
	if (flags & VM_MAP_WIRE_WRITE)
		prot |= VM_PROT_WRITE;
	user_wire = (flags & VM_MAP_WIRE_USER) ? TRUE : FALSE;
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		if (flags & VM_MAP_WIRE_HOLESOK)
			first_entry = first_entry->next;
		else {
			vm_map_unlock(map);
			return (KERN_INVALID_ADDRESS);
		}
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
			if (vm_map_unlock_and_wait(map, 0)) {
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
					if (flags & VM_MAP_WIRE_HOLESOK)
						tmp_entry = tmp_entry->next;
					else {
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
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 &&
		    entry->wiring_thread == NULL,
		    ("owned map entry %p", entry));
		entry->eflags |= MAP_ENTRY_IN_TRANSITION;
		entry->wiring_thread = curthread;
		if ((entry->protection & (VM_PROT_READ | VM_PROT_EXECUTE)) == 0
		    || (entry->protection & prot) != prot) {
			entry->eflags |= MAP_ENTRY_WIRE_SKIPPED;
			if ((flags & VM_MAP_WIRE_HOLESOK) == 0) {
				end = entry->end;
				rv = KERN_INVALID_ADDRESS;
				goto done;
			}
			goto next_entry;
		}
		if (entry->wired_count == 0) {
			entry->wired_count++;
			saved_start = entry->start;
			saved_end = entry->end;
			fictitious = entry->object.vm_object != NULL &&
			    (entry->object.vm_object->flags &
			    OBJ_FICTITIOUS) != 0;
			/*
			 * Release the map lock, relying on the in-transition
			 * mark.  Mark the map busy for fork.
			 */
			vm_map_busy(map);
			vm_map_unlock(map);
			rv = vm_fault_wire(map, saved_start, saved_end,
			    fictitious);
			vm_map_lock(map);
			vm_map_unbusy(map);
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
		 * If VM_MAP_WIRE_HOLESOK was specified, skip this check.
		 */
	next_entry:
		if (((flags & VM_MAP_WIRE_HOLESOK) == 0) &&
		    (entry->end < end && (entry->next == &map->header ||
		    entry->next->start > entry->end))) {
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
		if (!result && (flags & VM_MAP_WIRE_HOLESOK))
			first_entry = first_entry->next;
		else
			KASSERT(result, ("vm_map_wire: lookup failed"));
	}
	for (entry = first_entry; entry != &map->header && entry->start < end;
	    entry = entry->next) {
		if ((entry->eflags & MAP_ENTRY_WIRE_SKIPPED) != 0)
			goto next_entry_done;

		/*
		 * If VM_MAP_WIRE_HOLESOK was specified, an empty
		 * space in the unwired region could have been mapped
		 * while the map lock was dropped for faulting in the
		 * pages or draining MAP_ENTRY_IN_TRANSITION.
		 * Moreover, another thread could be simultaneously
		 * wiring this new mapping entry.  Detect these cases
		 * and skip any entries marked as in transition by us.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 ||
		    entry->wiring_thread != curthread) {
			KASSERT((flags & VM_MAP_WIRE_HOLESOK) != 0,
			    ("vm_map_wire: !HOLESOK and new/changed entry"));
			continue;
		}

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
				vm_fault_unwire(map, entry->start, entry->end,
				    entry->object.vm_object != NULL &&
				    (entry->object.vm_object->flags &
				    OBJ_FICTITIOUS) != 0);
			}
		}
	next_entry_done:
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0,
		    ("vm_map_wire: in-transition flag missing %p", entry));
		KASSERT(entry->wiring_thread == curthread,
		    ("vm_map_wire: alien wire %p", entry));
		entry->eflags &= ~(MAP_ENTRY_IN_TRANSITION |
		    MAP_ENTRY_WIRE_SKIPPED);
		entry->wiring_thread = NULL;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = TRUE;
		}
		vm_map_simplify_entry(map, entry);
	}
	vm_map_unlock(map);
	if (need_wakeup)
		vm_map_wakeup(map);
	return (rv);
}

/*
 * vm_map_sync
 *
 * Push any dirty cached pages in the address range to their pager.
 * If syncio is TRUE, dirty pages are written synchronously.
 * If invalidate is TRUE, any cached pages are freed as well.
 *
 * If the size of the region from start to end is zero, we are
 * supposed to flush all modified pages within the region containing
 * start.  Unfortunately, a region can be split or coalesced with
 * neighboring regions, making it difficult to determine what the
 * original region was.  Therefore, we approximate this requirement by
 * flushing the current region containing start.
 *
 * Returns an error if any part of the specified range is not mapped.
 */
int
vm_map_sync(
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
	unsigned int last_timestamp;
	boolean_t failed;

	vm_map_lock_read(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &entry)) {
		vm_map_unlock_read(map);
		return (KERN_INVALID_ADDRESS);
	} else if (start == end) {
		start = entry->start;
		end = entry->end;
	}
	/*
	 * Make a first pass to check for user-wired memory and holes.
	 */
	for (current = entry; current != &map->header && current->start < end;
	    current = current->next) {
		if (invalidate && (current->eflags & MAP_ENTRY_USER_WIRED)) {
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
		pmap_remove(map->pmap, start, end);
	failed = FALSE;

	/*
	 * Make a second pass, cleaning/uncaching pages from the indicated
	 * objects as we go.
	 */
	for (current = entry; current != &map->header && current->start < end;) {
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
		vm_object_reference(object);
		last_timestamp = map->timestamp;
		vm_map_unlock_read(map);
		if (!vm_object_sync(object, offset, size, syncio, invalidate))
			failed = TRUE;
		start += size;
		vm_object_deallocate(object);
		vm_map_lock_read(map);
		if (last_timestamp == map->timestamp ||
		    !vm_map_lookup_entry(map, start, &current))
			current = current->next;
	}

	vm_map_unlock_read(map);
	return (failed ? KERN_FAILURE : KERN_SUCCESS);
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
	vm_fault_unwire(map, entry->start, entry->end,
	    entry->object.vm_object != NULL &&
	    (entry->object.vm_object->flags & OBJ_FICTITIOUS) != 0);
	entry->wired_count = 0;
}

static void
vm_map_entry_deallocate(vm_map_entry_t entry, boolean_t system_map)
{

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0)
		vm_object_deallocate(entry->object.vm_object);
	uma_zfree(system_map ? kmapentzone : mapentzone, entry);
}

/*
 *	vm_map_entry_delete:	[ internal use only ]
 *
 *	Deallocate the given entry from the target map.
 */
static void
vm_map_entry_delete(vm_map_t map, vm_map_entry_t entry)
{
	vm_object_t object;
	vm_pindex_t offidxstart, offidxend, count, size1;
	vm_ooffset_t size;

	vm_map_entry_unlink(map, entry);
	object = entry->object.vm_object;
	size = entry->end - entry->start;
	map->size -= size;

	if (entry->cred != NULL) {
		swap_release_by_cred(size, entry->cred);
		crfree(entry->cred);
	}

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0 &&
	    (object != NULL)) {
		KASSERT(entry->cred == NULL || object->cred == NULL ||
		    (entry->eflags & MAP_ENTRY_NEEDS_COPY),
		    ("OVERCOMMIT vm_map_entry_delete: both cred %p", entry));
		count = OFF_TO_IDX(size);
		offidxstart = OFF_TO_IDX(entry->offset);
		offidxend = offidxstart + count;
		VM_OBJECT_WLOCK(object);
		if (object->ref_count != 1 &&
		    ((object->flags & (OBJ_NOSPLIT|OBJ_ONEMAPPING)) == OBJ_ONEMAPPING ||
		    object == kernel_object || object == kmem_object)) {
			vm_object_collapse(object);

			/*
			 * The option OBJPR_NOTMAPPED can be passed here
			 * because vm_map_delete() already performed
			 * pmap_remove() on the only mapping to this range
			 * of pages. 
			 */
			vm_object_page_remove(object, offidxstart, offidxend,
			    OBJPR_NOTMAPPED);
			if (object->type == OBJT_SWAP)
				swap_pager_freespace(object, offidxstart, count);
			if (offidxend >= object->size &&
			    offidxstart < object->size) {
				size1 = object->size;
				object->size = offidxstart;
				if (object->cred != NULL) {
					size1 -= object->size;
					KASSERT(object->charge >= ptoa(size1),
					    ("vm_map_entry_delete: object->charge < 0"));
					swap_release_by_cred(ptoa(size1), object->cred);
					object->charge -= ptoa(size1);
				}
			}
		}
		VM_OBJECT_WUNLOCK(object);
	} else
		entry->object.vm_object = NULL;
	if (map->system_map)
		vm_map_entry_deallocate(entry, TRUE);
	else {
		entry->next = curthread->td_map_def_user;
		curthread->td_map_def_user = entry;
	}
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
	vm_map_entry_t entry;
	vm_map_entry_t first_entry;

	VM_MAP_ASSERT_LOCKED(map);
	if (start == end)
		return (KERN_SUCCESS);

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
	 * Step through all entries in this region
	 */
	while ((entry != &map->header) && (entry->start < end)) {
		vm_map_entry_t next;

		/*
		 * Wait for wiring or unwiring of an entry to complete.
		 * Also wait for any system wirings to disappear on
		 * user maps.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0 ||
		    (vm_map_pmap(map) != kernel_pmap &&
		    vm_map_entry_system_wired_count(entry) != 0)) {
			unsigned int last_timestamp;
			vm_offset_t saved_start;
			vm_map_entry_t tmp_entry;

			saved_start = entry->start;
			entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
			last_timestamp = map->timestamp;
			(void) vm_map_unlock_and_wait(map, 0);
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

		next = entry->next;

		/*
		 * Unwire before removing addresses from the pmap; otherwise,
		 * unwiring will put the entries back in the pmap.
		 */
		if (entry->wired_count != 0) {
			vm_map_entry_unwire(map, entry);
		}

		pmap_remove(map->pmap, entry->start, entry->end);

		/*
		 * Delete the entry only after removing all pmap
		 * entries pointing to its pages.  (Otherwise, its
		 * page frames may be reallocated, and any modify bits
		 * will be set in the wrong object!)
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
	int result;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	result = vm_map_delete(map, start, end);
	vm_map_unlock(map);
	return (result);
}

/*
 *	vm_map_check_protection:
 *
 *	Assert that the target map allows the specified privilege on the
 *	entire address region given.  The entire region must be allocated.
 *
 *	WARNING!  This code does not and should not check whether the
 *	contents of the region is accessible.  For example a smaller file
 *	might be mapped into a larger address space.
 *
 *	NOTE!  This code is also called by munmap().
 *
 *	The map must be locked.  A read lock is sufficient.
 */
boolean_t
vm_map_check_protection(vm_map_t map, vm_offset_t start, vm_offset_t end,
			vm_prot_t protection)
{
	vm_map_entry_t entry;
	vm_map_entry_t tmp_entry;

	if (!vm_map_lookup_entry(map, start, &tmp_entry))
		return (FALSE);
	entry = tmp_entry;

	while (start < end) {
		if (entry == &map->header)
			return (FALSE);
		/*
		 * No holes allowed!
		 */
		if (start < entry->start)
			return (FALSE);
		/*
		 * Check protection associated with entry.
		 */
		if ((entry->protection & protection) != protection)
			return (FALSE);
		/* go to next entry */
		start = entry->end;
		entry = entry->next;
	}
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
	vm_map_entry_t dst_entry,
	vm_ooffset_t *fork_charge)
{
	vm_object_t src_object;
	vm_map_entry_t fake_entry;
	vm_offset_t size;
	struct ucred *cred;
	int charged;

	VM_MAP_ASSERT_LOCKED(dst_map);

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
		size = src_entry->end - src_entry->start;
		if ((src_object = src_entry->object.vm_object) != NULL) {
			VM_OBJECT_WLOCK(src_object);
			charged = ENTRY_CHARGED(src_entry);
			if ((src_object->handle == NULL) &&
				(src_object->type == OBJT_DEFAULT ||
				 src_object->type == OBJT_SWAP)) {
				vm_object_collapse(src_object);
				if ((src_object->flags & (OBJ_NOSPLIT|OBJ_ONEMAPPING)) == OBJ_ONEMAPPING) {
					vm_object_split(src_entry);
					src_object = src_entry->object.vm_object;
				}
			}
			vm_object_reference_locked(src_object);
			vm_object_clear_flag(src_object, OBJ_ONEMAPPING);
			if (src_entry->cred != NULL &&
			    !(src_entry->eflags & MAP_ENTRY_NEEDS_COPY)) {
				KASSERT(src_object->cred == NULL,
				    ("OVERCOMMIT: vm_map_copy_entry: cred %p",
				     src_object));
				src_object->cred = src_entry->cred;
				src_object->charge = size;
			}
			VM_OBJECT_WUNLOCK(src_object);
			dst_entry->object.vm_object = src_object;
			if (charged) {
				cred = curthread->td_ucred;
				crhold(cred);
				dst_entry->cred = cred;
				*fork_charge += size;
				if (!(src_entry->eflags &
				      MAP_ENTRY_NEEDS_COPY)) {
					crhold(cred);
					src_entry->cred = cred;
					*fork_charge += size;
				}
			}
			src_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->eflags |= (MAP_ENTRY_COW|MAP_ENTRY_NEEDS_COPY);
			dst_entry->offset = src_entry->offset;
			if (src_entry->eflags & MAP_ENTRY_VN_WRITECNT) {
				/*
				 * MAP_ENTRY_VN_WRITECNT cannot
				 * indicate write reference from
				 * src_entry, since the entry is
				 * marked as needs copy.  Allocate a
				 * fake entry that is used to
				 * decrement object->un_pager.vnp.writecount
				 * at the appropriate time.  Attach
				 * fake_entry to the deferred list.
				 */
				fake_entry = vm_map_entry_create(dst_map);
				fake_entry->eflags = MAP_ENTRY_VN_WRITECNT;
				src_entry->eflags &= ~MAP_ENTRY_VN_WRITECNT;
				vm_object_reference(src_object);
				fake_entry->object.vm_object = src_object;
				fake_entry->start = src_entry->start;
				fake_entry->end = src_entry->end;
				fake_entry->next = curthread->td_map_def_user;
				curthread->td_map_def_user = fake_entry;
			}
		} else {
			dst_entry->object.vm_object = NULL;
			dst_entry->offset = 0;
			if (src_entry->cred != NULL) {
				dst_entry->cred = curthread->td_ucred;
				crhold(dst_entry->cred);
				*fork_charge += size;
			}
		}

		pmap_copy(dst_map->pmap, src_map->pmap, dst_entry->start,
		    dst_entry->end - dst_entry->start, src_entry->start);
	} else {
		/*
		 * Of course, wired down pages can't be set copy-on-write.
		 * Cause wired pages to be copied into the new map by
		 * simulating faults (the new pages are pageable)
		 */
		vm_fault_copy_entry(dst_map, src_map, dst_entry, src_entry,
		    fork_charge);
	}
}

/*
 * vmspace_map_entry_forked:
 * Update the newly-forked vmspace each time a map entry is inherited
 * or copied.  The values for vm_dsize and vm_tsize are approximate
 * (and mostly-obsolete ideas in the face of mmap(2) et al.)
 */
static void
vmspace_map_entry_forked(const struct vmspace *vm1, struct vmspace *vm2,
    vm_map_entry_t entry)
{
	vm_size_t entrysize;
	vm_offset_t newend;

	entrysize = entry->end - entry->start;
	vm2->vm_map.size += entrysize;
	if (entry->eflags & (MAP_ENTRY_GROWS_DOWN | MAP_ENTRY_GROWS_UP)) {
		vm2->vm_ssize += btoc(entrysize);
	} else if (entry->start >= (vm_offset_t)vm1->vm_daddr &&
	    entry->start < (vm_offset_t)vm1->vm_daddr + ctob(vm1->vm_dsize)) {
		newend = MIN(entry->end,
		    (vm_offset_t)vm1->vm_daddr + ctob(vm1->vm_dsize));
		vm2->vm_dsize += btoc(newend - entry->start);
	} else if (entry->start >= (vm_offset_t)vm1->vm_taddr &&
	    entry->start < (vm_offset_t)vm1->vm_taddr + ctob(vm1->vm_tsize)) {
		newend = MIN(entry->end,
		    (vm_offset_t)vm1->vm_taddr + ctob(vm1->vm_tsize));
		vm2->vm_tsize += btoc(newend - entry->start);
	}
}

/*
 * vmspace_fork:
 * Create a new process vmspace structure and vm_map
 * based on those of an existing process.  The new map
 * is based on the old map, according to the inheritance
 * values on the regions in that map.
 *
 * XXX It might be worth coalescing the entries added to the new vmspace.
 *
 * The source map must not be locked.
 */
struct vmspace *
vmspace_fork(struct vmspace *vm1, vm_ooffset_t *fork_charge)
{
	struct vmspace *vm2;
	vm_map_t new_map, old_map;
	vm_map_entry_t new_entry, old_entry;
	vm_object_t object;
	int locked;

	old_map = &vm1->vm_map;
	/* Copy immutable fields of vm1 to vm2. */
	vm2 = vmspace_alloc(old_map->min_offset, old_map->max_offset, NULL);
	if (vm2 == NULL)
		return (NULL);
	vm2->vm_taddr = vm1->vm_taddr;
	vm2->vm_daddr = vm1->vm_daddr;
	vm2->vm_maxsaddr = vm1->vm_maxsaddr;
	vm_map_lock(old_map);
	if (old_map->busy)
		vm_map_wait_busy(old_map);
	new_map = &vm2->vm_map;
	locked = vm_map_trylock(new_map); /* trylock to silence WITNESS */
	KASSERT(locked, ("vmspace_fork: lock failed"));

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
				old_entry->offset = 0;
				if (old_entry->cred != NULL) {
					object->cred = old_entry->cred;
					object->charge = old_entry->end -
					    old_entry->start;
					old_entry->cred = NULL;
				}
			}

			/*
			 * Add the reference before calling vm_object_shadow
			 * to insure that a shadow object is created.
			 */
			vm_object_reference(object);
			if (old_entry->eflags & MAP_ENTRY_NEEDS_COPY) {
				vm_object_shadow(&old_entry->object.vm_object,
				    &old_entry->offset,
				    old_entry->end - old_entry->start);
				old_entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
				/* Transfer the second reference too. */
				vm_object_reference(
				    old_entry->object.vm_object);

				/*
				 * As in vm_map_simplify_entry(), the
				 * vnode lock will not be acquired in
				 * this call to vm_object_deallocate().
				 */
				vm_object_deallocate(object);
				object = old_entry->object.vm_object;
			}
			VM_OBJECT_WLOCK(object);
			vm_object_clear_flag(object, OBJ_ONEMAPPING);
			if (old_entry->cred != NULL) {
				KASSERT(object->cred == NULL, ("vmspace_fork both cred"));
				object->cred = old_entry->cred;
				object->charge = old_entry->end - old_entry->start;
				old_entry->cred = NULL;
			}

			/*
			 * Assert the correct state of the vnode
			 * v_writecount while the object is locked, to
			 * not relock it later for the assertion
			 * correctness.
			 */
			if (old_entry->eflags & MAP_ENTRY_VN_WRITECNT &&
			    object->type == OBJT_VNODE) {
				KASSERT(((struct vnode *)object->handle)->
				    v_writecount > 0,
				    ("vmspace_fork: v_writecount %p", object));
				KASSERT(object->un_pager.vnp.writemappings > 0,
				    ("vmspace_fork: vnp.writecount %p",
				    object));
			}
			VM_OBJECT_WUNLOCK(object);

			/*
			 * Clone the entry, referencing the shared object.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->eflags &= ~(MAP_ENTRY_USER_WIRED |
			    MAP_ENTRY_IN_TRANSITION);
			new_entry->wiring_thread = NULL;
			new_entry->wired_count = 0;
			if (new_entry->eflags & MAP_ENTRY_VN_WRITECNT) {
				vnode_pager_update_writecount(object,
				    new_entry->start, new_entry->end);
			}

			/*
			 * Insert the entry into the new map -- we know we're
			 * inserting at the end of the new map.
			 */
			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);
			vmspace_map_entry_forked(vm1, vm2, new_entry);

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
			/*
			 * Copied entry is COW over the old object.
			 */
			new_entry->eflags &= ~(MAP_ENTRY_USER_WIRED |
			    MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_VN_WRITECNT);
			new_entry->wiring_thread = NULL;
			new_entry->wired_count = 0;
			new_entry->object.vm_object = NULL;
			new_entry->cred = NULL;
			vm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);
			vmspace_map_entry_forked(vm1, vm2, new_entry);
			vm_map_copy_entry(old_map, new_map, old_entry,
			    new_entry, fork_charge);
			break;
		}
		old_entry = old_entry->next;
	}
	/*
	 * Use inlined vm_map_unlock() to postpone handling the deferred
	 * map entries, which cannot be done until both old_map and
	 * new_map locks are released.
	 */
	sx_xunlock(&old_map->lock);
	sx_xunlock(&new_map->lock);
	vm_map_process_deferred();

	return (vm2);
}

int
vm_map_stack(vm_map_t map, vm_offset_t addrbos, vm_size_t max_ssize,
    vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_map_entry_t new_entry, prev_entry;
	vm_offset_t bot, top;
	vm_size_t growsize, init_ssize;
	int orient, rv;
	rlim_t lmemlim, vmemlim;

	/*
	 * The stack orientation is piggybacked with the cow argument.
	 * Extract it into orient and mask the cow argument so that we
	 * don't pass it around further.
	 * NOTE: We explicitly allow bi-directional stacks.
	 */
	orient = cow & (MAP_STACK_GROWS_DOWN|MAP_STACK_GROWS_UP);
	cow &= ~orient;
	KASSERT(orient != 0, ("No stack grow direction"));

	if (addrbos < vm_map_min(map) ||
	    addrbos > vm_map_max(map) ||
	    addrbos + max_ssize < addrbos)
		return (KERN_NO_SPACE);

	growsize = sgrowsiz;
	init_ssize = (max_ssize < growsize) ? max_ssize : growsize;

	PROC_LOCK(curproc);
	lmemlim = lim_cur(curproc, RLIMIT_MEMLOCK);
	vmemlim = lim_cur(curproc, RLIMIT_VMEM);
	PROC_UNLOCK(curproc);

	vm_map_lock(map);

	/* If addr is already mapped, no go */
	if (vm_map_lookup_entry(map, addrbos, &prev_entry)) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	if (!old_mlock && map->flags & MAP_WIREFUTURE) {
		if (ptoa(pmap_wired_count(map->pmap)) + init_ssize > lmemlim) {
			vm_map_unlock(map);
			return (KERN_NO_SPACE);
		}
	}

	/* If we would blow our VMEM resource limit, no go */
	if (map->size + init_ssize > vmemlim) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	/*
	 * If we can't accomodate max_ssize in the current mapping, no go.
	 * However, we need to be aware that subsequent user mappings might
	 * map into the space we have reserved for stack, and currently this
	 * space is not protected.
	 *
	 * Hopefully we will at least detect this condition when we try to
	 * grow the stack.
	 */
	if ((prev_entry->next != &map->header) &&
	    (prev_entry->next->start < addrbos + max_ssize)) {
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	/*
	 * We initially map a stack of only init_ssize.  We will grow as
	 * needed later.  Depending on the orientation of the stack (i.e.
	 * the grow direction) we either map at the top of the range, the
	 * bottom of the range or in the middle.
	 *
	 * Note: we would normally expect prot and max to be VM_PROT_ALL,
	 * and cow to be 0.  Possibly we should eliminate these as input
	 * parameters, and just pass these values here in the insert call.
	 */
	if (orient == MAP_STACK_GROWS_DOWN)
		bot = addrbos + max_ssize - init_ssize;
	else if (orient == MAP_STACK_GROWS_UP)
		bot = addrbos;
	else
		bot = round_page(addrbos + max_ssize/2 - init_ssize/2);
	top = bot + init_ssize;
	rv = vm_map_insert(map, NULL, 0, bot, top, prot, max, cow);

	/* Now set the avail_ssize amount. */
	if (rv == KERN_SUCCESS) {
		if (prev_entry != &map->header)
			vm_map_clip_end(map, prev_entry, bot);
		new_entry = prev_entry->next;
		if (new_entry->end != top || new_entry->start != bot)
			panic("Bad entry start/end for new stack entry");

		new_entry->avail_ssize = max_ssize - init_ssize;
		if (orient & MAP_STACK_GROWS_DOWN)
			new_entry->eflags |= MAP_ENTRY_GROWS_DOWN;
		if (orient & MAP_STACK_GROWS_UP)
			new_entry->eflags |= MAP_ENTRY_GROWS_UP;
	}

	vm_map_unlock(map);
	return (rv);
}

static int stack_guard_page = 0;
TUNABLE_INT("security.bsd.stack_guard_page", &stack_guard_page);
SYSCTL_INT(_security_bsd, OID_AUTO, stack_guard_page, CTLFLAG_RW,
    &stack_guard_page, 0,
    "Insert stack guard page ahead of the growable segments.");

/* Attempts to grow a vm stack entry.  Returns KERN_SUCCESS if the
 * desired address is already mapped, or if we successfully grow
 * the stack.  Also returns KERN_SUCCESS if addr is outside the
 * stack range (this is strange, but preserves compatibility with
 * the grow function in vm_machdep.c).
 */
int
vm_map_growstack(struct proc *p, vm_offset_t addr)
{
	vm_map_entry_t next_entry, prev_entry;
	vm_map_entry_t new_entry, stack_entry;
	struct vmspace *vm = p->p_vmspace;
	vm_map_t map = &vm->vm_map;
	vm_offset_t end;
	vm_size_t growsize;
	size_t grow_amount, max_grow;
	rlim_t lmemlim, stacklim, vmemlim;
	int is_procstack, rv;
	struct ucred *cred;
#ifdef notyet
	uint64_t limit;
#endif
#ifdef RACCT
	int error;
#endif

Retry:
	PROC_LOCK(p);
	lmemlim = lim_cur(p, RLIMIT_MEMLOCK);
	stacklim = lim_cur(p, RLIMIT_STACK);
	vmemlim = lim_cur(p, RLIMIT_VMEM);
	PROC_UNLOCK(p);

	vm_map_lock_read(map);

	/* If addr is already in the entry range, no need to grow.*/
	if (vm_map_lookup_entry(map, addr, &prev_entry)) {
		vm_map_unlock_read(map);
		return (KERN_SUCCESS);
	}

	next_entry = prev_entry->next;
	if (!(prev_entry->eflags & MAP_ENTRY_GROWS_UP)) {
		/*
		 * This entry does not grow upwards. Since the address lies
		 * beyond this entry, the next entry (if one exists) has to
		 * be a downward growable entry. The entry list header is
		 * never a growable entry, so it suffices to check the flags.
		 */
		if (!(next_entry->eflags & MAP_ENTRY_GROWS_DOWN)) {
			vm_map_unlock_read(map);
			return (KERN_SUCCESS);
		}
		stack_entry = next_entry;
	} else {
		/*
		 * This entry grows upward. If the next entry does not at
		 * least grow downwards, this is the entry we need to grow.
		 * otherwise we have two possible choices and we have to
		 * select one.
		 */
		if (next_entry->eflags & MAP_ENTRY_GROWS_DOWN) {
			/*
			 * We have two choices; grow the entry closest to
			 * the address to minimize the amount of growth.
			 */
			if (addr - prev_entry->end <= next_entry->start - addr)
				stack_entry = prev_entry;
			else
				stack_entry = next_entry;
		} else
			stack_entry = prev_entry;
	}

	if (stack_entry == next_entry) {
		KASSERT(stack_entry->eflags & MAP_ENTRY_GROWS_DOWN, ("foo"));
		KASSERT(addr < stack_entry->start, ("foo"));
		end = (prev_entry != &map->header) ? prev_entry->end :
		    stack_entry->start - stack_entry->avail_ssize;
		grow_amount = roundup(stack_entry->start - addr, PAGE_SIZE);
		max_grow = stack_entry->start - end;
	} else {
		KASSERT(stack_entry->eflags & MAP_ENTRY_GROWS_UP, ("foo"));
		KASSERT(addr >= stack_entry->end, ("foo"));
		end = (next_entry != &map->header) ? next_entry->start :
		    stack_entry->end + stack_entry->avail_ssize;
		grow_amount = roundup(addr + 1 - stack_entry->end, PAGE_SIZE);
		max_grow = end - stack_entry->end;
	}

	if (grow_amount > stack_entry->avail_ssize) {
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
	}

	/*
	 * If there is no longer enough space between the entries nogo, and
	 * adjust the available space.  Note: this  should only happen if the
	 * user has mapped into the stack area after the stack was created,
	 * and is probably an error.
	 *
	 * This also effectively destroys any guard page the user might have
	 * intended by limiting the stack size.
	 */
	if (grow_amount + (stack_guard_page ? PAGE_SIZE : 0) > max_grow) {
		if (vm_map_lock_upgrade(map))
			goto Retry;

		stack_entry->avail_ssize = max_grow;

		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

	is_procstack = (addr >= (vm_offset_t)vm->vm_maxsaddr) ? 1 : 0;

	/*
	 * If this is the main process stack, see if we're over the stack
	 * limit.
	 */
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount > stacklim)) {
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
	}
#ifdef RACCT
	PROC_LOCK(p);
	if (is_procstack &&
	    racct_set(p, RACCT_STACK, ctob(vm->vm_ssize) + grow_amount)) {
		PROC_UNLOCK(p);
		vm_map_unlock_read(map);
		return (KERN_NO_SPACE);
	}
	PROC_UNLOCK(p);
#endif

	/* Round up the grow amount modulo sgrowsiz */
	growsize = sgrowsiz;
	grow_amount = roundup(grow_amount, growsize);
	if (grow_amount > stack_entry->avail_ssize)
		grow_amount = stack_entry->avail_ssize;
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount > stacklim)) {
		grow_amount = trunc_page((vm_size_t)stacklim) -
		    ctob(vm->vm_ssize);
	}
#ifdef notyet
	PROC_LOCK(p);
	limit = racct_get_available(p, RACCT_STACK);
	PROC_UNLOCK(p);
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount > limit))
		grow_amount = limit - ctob(vm->vm_ssize);
#endif
	if (!old_mlock && map->flags & MAP_WIREFUTURE) {
		if (ptoa(pmap_wired_count(map->pmap)) + grow_amount > lmemlim) {
			vm_map_unlock_read(map);
			rv = KERN_NO_SPACE;
			goto out;
		}
#ifdef RACCT
		PROC_LOCK(p);
		if (racct_set(p, RACCT_MEMLOCK,
		    ptoa(pmap_wired_count(map->pmap)) + grow_amount)) {
			PROC_UNLOCK(p);
			vm_map_unlock_read(map);
			rv = KERN_NO_SPACE;
			goto out;
		}
		PROC_UNLOCK(p);
#endif
	}
	/* If we would blow our VMEM resource limit, no go */
	if (map->size + grow_amount > vmemlim) {
		vm_map_unlock_read(map);
		rv = KERN_NO_SPACE;
		goto out;
	}
#ifdef RACCT
	PROC_LOCK(p);
	if (racct_set(p, RACCT_VMEM, map->size + grow_amount)) {
		PROC_UNLOCK(p);
		vm_map_unlock_read(map);
		rv = KERN_NO_SPACE;
		goto out;
	}
	PROC_UNLOCK(p);
#endif

	if (vm_map_lock_upgrade(map))
		goto Retry;

	if (stack_entry == next_entry) {
		/*
		 * Growing downward.
		 */
		/* Get the preliminary new entry start value */
		addr = stack_entry->start - grow_amount;

		/*
		 * If this puts us into the previous entry, cut back our
		 * growth to the available space. Also, see the note above.
		 */
		if (addr < end) {
			stack_entry->avail_ssize = max_grow;
			addr = end;
			if (stack_guard_page)
				addr += PAGE_SIZE;
		}

		rv = vm_map_insert(map, NULL, 0, addr, stack_entry->start,
		    next_entry->protection, next_entry->max_protection, 0);

		/* Adjust the available stack space by the amount we grew. */
		if (rv == KERN_SUCCESS) {
			if (prev_entry != &map->header)
				vm_map_clip_end(map, prev_entry, addr);
			new_entry = prev_entry->next;
			KASSERT(new_entry == stack_entry->prev, ("foo"));
			KASSERT(new_entry->end == stack_entry->start, ("foo"));
			KASSERT(new_entry->start == addr, ("foo"));
			grow_amount = new_entry->end - new_entry->start;
			new_entry->avail_ssize = stack_entry->avail_ssize -
			    grow_amount;
			stack_entry->eflags &= ~MAP_ENTRY_GROWS_DOWN;
			new_entry->eflags |= MAP_ENTRY_GROWS_DOWN;
		}
	} else {
		/*
		 * Growing upward.
		 */
		addr = stack_entry->end + grow_amount;

		/*
		 * If this puts us into the next entry, cut back our growth
		 * to the available space. Also, see the note above.
		 */
		if (addr > end) {
			stack_entry->avail_ssize = end - stack_entry->end;
			addr = end;
			if (stack_guard_page)
				addr -= PAGE_SIZE;
		}

		grow_amount = addr - stack_entry->end;
		cred = stack_entry->cred;
		if (cred == NULL && stack_entry->object.vm_object != NULL)
			cred = stack_entry->object.vm_object->cred;
		if (cred != NULL && !swap_reserve_by_cred(grow_amount, cred))
			rv = KERN_NO_SPACE;
		/* Grow the underlying object if applicable. */
		else if (stack_entry->object.vm_object == NULL ||
			 vm_object_coalesce(stack_entry->object.vm_object,
			 stack_entry->offset,
			 (vm_size_t)(stack_entry->end - stack_entry->start),
			 (vm_size_t)grow_amount, cred != NULL)) {
			map->size += (addr - stack_entry->end);
			/* Update the current entry. */
			stack_entry->end = addr;
			stack_entry->avail_ssize -= grow_amount;
			vm_map_entry_resize_free(map, stack_entry);
			rv = KERN_SUCCESS;

			if (next_entry != &map->header)
				vm_map_clip_start(map, next_entry, addr);
		} else
			rv = KERN_FAILURE;
	}

	if (rv == KERN_SUCCESS && is_procstack)
		vm->vm_ssize += btoc(grow_amount);

	vm_map_unlock(map);

	/*
	 * Heed the MAP_WIREFUTURE flag if it was set for this process.
	 */
	if (rv == KERN_SUCCESS && (map->flags & MAP_WIREFUTURE)) {
		vm_map_wire(map,
		    (stack_entry == next_entry) ? addr : addr - grow_amount,
		    (stack_entry == next_entry) ? stack_entry->start : addr,
		    (p->p_flag & P_SYSTEM)
		    ? VM_MAP_WIRE_SYSTEM|VM_MAP_WIRE_NOHOLES
		    : VM_MAP_WIRE_USER|VM_MAP_WIRE_NOHOLES);
	}

out:
#ifdef RACCT
	if (rv != KERN_SUCCESS) {
		PROC_LOCK(p);
		error = racct_set(p, RACCT_VMEM, map->size);
		KASSERT(error == 0, ("decreasing RACCT_VMEM failed"));
		if (!old_mlock) {
			error = racct_set(p, RACCT_MEMLOCK,
			    ptoa(pmap_wired_count(map->pmap)));
			KASSERT(error == 0, ("decreasing RACCT_MEMLOCK failed"));
		}
	    	error = racct_set(p, RACCT_STACK, ctob(vm->vm_ssize));
		KASSERT(error == 0, ("decreasing RACCT_STACK failed"));
		PROC_UNLOCK(p);
	}
#endif

	return (rv);
}

/*
 * Unshare the specified VM space for exec.  If other processes are
 * mapped to it, then create a new one.  The new vmspace is null.
 */
int
vmspace_exec(struct proc *p, vm_offset_t minuser, vm_offset_t maxuser)
{
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;

	newvmspace = vmspace_alloc(minuser, maxuser, NULL);
	if (newvmspace == NULL)
		return (ENOMEM);
	newvmspace->vm_swrss = oldvmspace->vm_swrss;
	/*
	 * This code is written like this for prototype purposes.  The
	 * goal is to avoid running down the vmspace here, but let the
	 * other process's that are still using the vmspace to finally
	 * run it down.  Even though there is little or no chance of blocking
	 * here, it is a good idea to keep this form for future mods.
	 */
	PROC_VMSPACE_LOCK(p);
	p->p_vmspace = newvmspace;
	PROC_VMSPACE_UNLOCK(p);
	if (p == curthread->td_proc)
		pmap_activate(curthread);
	vmspace_free(oldvmspace);
	return (0);
}

/*
 * Unshare the specified VM space for forcing COW.  This
 * is called by rfork, for the (RFMEM|RFPROC) == 0 case.
 */
int
vmspace_unshare(struct proc *p)
{
	struct vmspace *oldvmspace = p->p_vmspace;
	struct vmspace *newvmspace;
	vm_ooffset_t fork_charge;

	if (oldvmspace->vm_refcnt == 1)
		return (0);
	fork_charge = 0;
	newvmspace = vmspace_fork(oldvmspace, &fork_charge);
	if (newvmspace == NULL)
		return (ENOMEM);
	if (!swap_reserve_by_cred(fork_charge, p->p_ucred)) {
		vmspace_free(newvmspace);
		return (ENOMEM);
	}
	PROC_VMSPACE_LOCK(p);
	p->p_vmspace = newvmspace;
	PROC_VMSPACE_UNLOCK(p);
	if (p == curthread->td_proc)
		pmap_activate(curthread);
	vmspace_free(oldvmspace);
	return (0);
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
	vm_object_t eobject;
	vm_size_t size;
	struct ucred *cred;

RetryLookup:;

	vm_map_lock_read(map);

	/*
	 * Lookup the faulting address.
	 */
	if (!vm_map_lookup_entry(map, vaddr, out_entry)) {
		vm_map_unlock_read(map);
		return (KERN_INVALID_ADDRESS);
	}

	entry = *out_entry;

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
	 */
	prot = entry->protection;
	fault_type &= (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	if ((fault_type & prot) != fault_type || prot == VM_PROT_NONE) {
		vm_map_unlock_read(map);
		return (KERN_PROTECTION_FAILURE);
	}
	if ((entry->eflags & MAP_ENTRY_USER_WIRED) &&
	    (entry->eflags & MAP_ENTRY_COW) &&
	    (fault_type & VM_PROT_WRITE)) {
		vm_map_unlock_read(map);
		return (KERN_PROTECTION_FAILURE);
	}
	if ((fault_typea & VM_PROT_COPY) != 0 &&
	    (entry->max_protection & VM_PROT_WRITE) == 0 &&
	    (entry->eflags & MAP_ENTRY_COW) == 0) {
		vm_map_unlock_read(map);
		return (KERN_PROTECTION_FAILURE);
	}

	/*
	 * If this page is not pageable, we have to get it for all possible
	 * accesses.
	 */
	*wired = (entry->wired_count != 0);
	if (*wired)
		fault_type = entry->protection;
	size = entry->end - entry->start;
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
		if ((fault_type & VM_PROT_WRITE) != 0 ||
		    (fault_typea & VM_PROT_COPY) != 0) {
			/*
			 * Make a new object, and place it in the object
			 * chain.  Note that no new references have appeared
			 * -- one just moved from the map to the new
			 * object.
			 */
			if (vm_map_lock_upgrade(map))
				goto RetryLookup;

			if (entry->cred == NULL) {
				/*
				 * The debugger owner is charged for
				 * the memory.
				 */
				cred = curthread->td_ucred;
				crhold(cred);
				if (!swap_reserve_by_cred(size, cred)) {
					crfree(cred);
					vm_map_unlock(map);
					return (KERN_RESOURCE_SHORTAGE);
				}
				entry->cred = cred;
			}
			vm_object_shadow(&entry->object.vm_object,
			    &entry->offset, size);
			entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
			eobject = entry->object.vm_object;
			if (eobject->cred != NULL) {
				/*
				 * The object was not shadowed.
				 */
				swap_release_by_cred(size, entry->cred);
				crfree(entry->cred);
				entry->cred = NULL;
			} else if (entry->cred != NULL) {
				VM_OBJECT_WLOCK(eobject);
				eobject->cred = entry->cred;
				eobject->charge = size;
				VM_OBJECT_WUNLOCK(eobject);
				entry->cred = NULL;
			}

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
		    atop(size));
		entry->offset = 0;
		if (entry->cred != NULL) {
			VM_OBJECT_WLOCK(entry->object.vm_object);
			entry->object.vm_object->cred = entry->cred;
			entry->object.vm_object->charge = size;
			VM_OBJECT_WUNLOCK(entry->object.vm_object);
			entry->cred = NULL;
		}
		vm_map_lock_downgrade(map);
	}

	/*
	 * Return the object/offset from this entry.  If the entry was
	 * copy-on-write or empty, it has been fixed up.
	 */
	*pindex = OFF_TO_IDX((vaddr - entry->start) + entry->offset);
	*object = entry->object.vm_object;

	*out_prot = prot;
	return (KERN_SUCCESS);
}

/*
 *	vm_map_lookup_locked:
 *
 *	Lookup the faulting address.  A version of vm_map_lookup that returns 
 *      KERN_FAILURE instead of blocking on map lock or memory allocation.
 */
int
vm_map_lookup_locked(vm_map_t *var_map,		/* IN/OUT */
		     vm_offset_t vaddr,
		     vm_prot_t fault_typea,
		     vm_map_entry_t *out_entry,	/* OUT */
		     vm_object_t *object,	/* OUT */
		     vm_pindex_t *pindex,	/* OUT */
		     vm_prot_t *out_prot,	/* OUT */
		     boolean_t *wired)		/* OUT */
{
	vm_map_entry_t entry;
	vm_map_t map = *var_map;
	vm_prot_t prot;
	vm_prot_t fault_type = fault_typea;

	/*
	 * Lookup the faulting address.
	 */
	if (!vm_map_lookup_entry(map, vaddr, out_entry))
		return (KERN_INVALID_ADDRESS);

	entry = *out_entry;

	/*
	 * Fail if the entry refers to a submap.
	 */
	if (entry->eflags & MAP_ENTRY_IS_SUB_MAP)
		return (KERN_FAILURE);

	/*
	 * Check whether this task is allowed to have this page.
	 */
	prot = entry->protection;
	fault_type &= VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	if ((fault_type & prot) != fault_type)
		return (KERN_PROTECTION_FAILURE);
	if ((entry->eflags & MAP_ENTRY_USER_WIRED) &&
	    (entry->eflags & MAP_ENTRY_COW) &&
	    (fault_type & VM_PROT_WRITE))
		return (KERN_PROTECTION_FAILURE);

	/*
	 * If this page is not pageable, we have to get it for all possible
	 * accesses.
	 */
	*wired = (entry->wired_count != 0);
	if (*wired)
		fault_type = entry->protection;

	if (entry->eflags & MAP_ENTRY_NEEDS_COPY) {
		/*
		 * Fail if the entry was copy-on-write for a write fault.
		 */
		if (fault_type & VM_PROT_WRITE)
			return (KERN_FAILURE);
		/*
		 * We're attempting to read a copy-on-write page --
		 * don't allow writes.
		 */
		prot &= ~VM_PROT_WRITE;
	}

	/*
	 * Fail if an object should be created.
	 */
	if (entry->object.vm_object == NULL && !map->system_map)
		return (KERN_FAILURE);

	/*
	 * Return the object/offset from this entry.  If the entry was
	 * copy-on-write or empty, it has been fixed up.
	 */
	*pindex = OFF_TO_IDX((vaddr - entry->start) + entry->offset);
	*object = entry->object.vm_object;

	*out_prot = prot;
	return (KERN_SUCCESS);
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

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

static void
vm_map_print(vm_map_t map)
{
	vm_map_entry_t entry;

	db_iprintf("Task map %p: pmap=%p, nentries=%d, version=%u\n",
	    (void *)map,
	    (void *)map->pmap, map->nentries, map->timestamp);

	db_indent += 2;
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		db_iprintf("map entry %p: start=%p, end=%p\n",
		    (void *)entry, (void *)entry->start, (void *)entry->end);
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
			if ((entry->prev == &map->header) ||
			    (entry->prev->object.sub_map !=
				entry->object.sub_map)) {
				db_indent += 2;
				vm_map_print((vm_map_t)entry->object.sub_map);
				db_indent -= 2;
			}
		} else {
			if (entry->cred != NULL)
				db_printf(", ruid %d", entry->cred->cr_ruid);
			db_printf(", object=%p, offset=0x%jx",
			    (void *)entry->object.vm_object,
			    (uintmax_t)entry->offset);
			if (entry->object.vm_object && entry->object.vm_object->cred)
				db_printf(", obj ruid %d charge %jx",
				    entry->object.vm_object->cred->cr_ruid,
				    (uintmax_t)entry->object.vm_object->charge);
			if (entry->eflags & MAP_ENTRY_COW)
				db_printf(", copy (%s)",
				    (entry->eflags & MAP_ENTRY_NEEDS_COPY) ? "needed" : "done");
			db_printf("\n");

			if ((entry->prev == &map->header) ||
			    (entry->prev->object.vm_object !=
				entry->object.vm_object)) {
				db_indent += 2;
				vm_object_print((db_expr_t)(intptr_t)
						entry->object.vm_object,
						1, 0, (char *)0);
				db_indent -= 2;
			}
		}
	}
	db_indent -= 2;
}

DB_SHOW_COMMAND(map, map)
{

	if (!have_addr) {
		db_printf("usage: show map <addr>\n");
		return;
	}
	vm_map_print((vm_map_t)addr);
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

	vm_map_print((vm_map_t)&p->p_vmspace->vm_map);
}

#endif /* DDB */
