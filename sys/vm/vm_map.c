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
#include <sys/elf.h>
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
#include <vm/vm_pageout.h>
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
static uma_zone_t vmspace_zone;
static int vmspace_zinit(void *mem, int size, int flags);
static void _vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min,
    vm_offset_t max);
static void vm_map_entry_deallocate(vm_map_entry_t entry, boolean_t system_map);
static void vm_map_entry_dispose(vm_map_t map, vm_map_entry_t entry);
static void vm_map_entry_unwire(vm_map_t map, vm_map_entry_t entry);
static int vm_map_growstack(vm_map_t map, vm_offset_t addr,
    vm_map_entry_t gap_entry);
static void vm_map_pmap_enter(vm_map_t map, vm_offset_t addr, vm_prot_t prot,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size, int flags);
#ifdef INVARIANTS
static void vmspace_zdtor(void *mem, int size, void *arg);
#endif
static int vm_map_stack_locked(vm_map_t map, vm_offset_t addrbos,
    vm_size_t max_ssize, vm_size_t growsize, vm_prot_t prot, vm_prot_t max,
    int cow);
static void vm_map_wire_entry_failure(vm_map_t map, vm_map_entry_t entry,
    vm_offset_t failed_addr);

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

#ifndef UMA_MD_SMALL_ALLOC

/*
 * Allocate a new slab for kernel map entries.  The kernel map may be locked or
 * unlocked, depending on whether the request is coming from the kernel map or a
 * submap.  This function allocates a virtual address range directly from the
 * kernel map instead of the kmem_* layer to avoid recursion on the kernel map
 * lock and also to avoid triggering allocator recursion in the vmem boundary
 * tag allocator.
 */
static void *
kmapent_alloc(uma_zone_t zone, vm_size_t bytes, int domain, uint8_t *pflag,
    int wait)
{
	vm_offset_t addr;
	int error, locked;

	*pflag = UMA_SLAB_PRIV;

	if (!(locked = vm_map_locked(kernel_map)))
		vm_map_lock(kernel_map);
	addr = vm_map_findspace(kernel_map, vm_map_min(kernel_map), bytes);
	if (addr + bytes < addr || addr + bytes > vm_map_max(kernel_map))
		panic("%s: kernel map is exhausted", __func__);
	error = vm_map_insert(kernel_map, NULL, 0, addr, addr + bytes,
	    VM_PROT_RW, VM_PROT_RW, MAP_NOFAULT);
	if (error != KERN_SUCCESS)
		panic("%s: vm_map_insert() failed: %d", __func__, error);
	if (!locked)
		vm_map_unlock(kernel_map);
	error = kmem_back_domain(domain, kernel_object, addr, bytes, M_NOWAIT |
	    M_USE_RESERVE | (wait & M_ZERO));
	if (error == KERN_SUCCESS) {
		return ((void *)addr);
	} else {
		if (!locked)
			vm_map_lock(kernel_map);
		vm_map_delete(kernel_map, addr, bytes);
		if (!locked)
			vm_map_unlock(kernel_map);
		return (NULL);
	}
}

static void
kmapent_free(void *item, vm_size_t size, uint8_t pflag)
{
	vm_offset_t addr;
	int error;

	if ((pflag & UMA_SLAB_PRIV) == 0)
		/* XXX leaked */
		return;

	addr = (vm_offset_t)item;
	kmem_unback(kernel_object, addr, size);
	error = vm_map_remove(kernel_map, addr, addr + size);
	KASSERT(error == KERN_SUCCESS,
	    ("%s: vm_map_remove failed: %d", __func__, error));
}

/*
 * The worst-case upper bound on the number of kernel map entries that may be
 * created before the zone must be replenished in _vm_map_unlock().
 */
#define	KMAPENT_RESERVE		1

#endif /* !UMD_MD_SMALL_ALLOC */

/*
 *	vm_map_startup:
 *
 *	Initialize the vm_map module.  Must be called before any other vm_map
 *	routines.
 *
 *	User map and entry structures are allocated from the general purpose
 *	memory pool.  Kernel maps are statically defined.  Kernel map entries
 *	require special handling to avoid recursion; see the comments above
 *	kmapent_alloc() and in vm_map_entry_create().
 */
void
vm_map_startup(void)
{
	mtx_init(&map_sleep_mtx, "vm map sleep mutex", NULL, MTX_DEF);

	/*
	 * Disable the use of per-CPU buckets: map entry allocation is
	 * serialized by the kernel map lock.
	 */
	kmapentzone = uma_zcreate("KMAP ENTRY", sizeof(struct vm_map_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_VM | UMA_ZONE_NOBUCKET);
#ifndef UMA_MD_SMALL_ALLOC
	/* Reserve an extra map entry for use when replenishing the reserve. */
	uma_zone_reserve(kmapentzone, KMAPENT_RESERVE + 1);
	uma_prealloc(kmapentzone, KMAPENT_RESERVE + 1);
	uma_zone_set_allocf(kmapentzone, kmapent_alloc);
	uma_zone_set_freef(kmapentzone, kmapent_free);
#endif

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
	vm_map_t map;

	vm = (struct vmspace *)mem;
	map = &vm->vm_map;

	memset(map, 0, sizeof(*map));
	mtx_init(&map->system_mtx, "vm map (system)", NULL,
	    MTX_DEF | MTX_DUPOK);
	sx_init(&map->lock, "vm map (user)");
	PMAP_LOCK_INIT(vmspace_pmap(vm));
	return (0);
}

#ifdef INVARIANTS
static void
vmspace_zdtor(void *mem, int size, void *arg)
{
	struct vmspace *vm;

	vm = (struct vmspace *)mem;
	KASSERT(vm->vm_map.nentries == 0,
	    ("vmspace %p nentries == %d on free", vm, vm->vm_map.nentries));
	KASSERT(vm->vm_map.size == 0,
	    ("vmspace %p size == %ju on free", vm, (uintmax_t)vm->vm_map.size));
}
#endif	/* INVARIANTS */

/*
 * Allocate a vmspace structure, including a vm_map and pmap,
 * and initialize those structures.  The refcnt is set to 1.
 */
struct vmspace *
vmspace_alloc(vm_offset_t min, vm_offset_t max, pmap_pinit_t pinit)
{
	struct vmspace *vm;

	vm = uma_zalloc(vmspace_zone, M_WAITOK);
	KASSERT(vm->vm_map.pmap == NULL, ("vm_map.pmap must be NULL"));
	if (!pinit(vmspace_pmap(vm))) {
		uma_zfree(vmspace_zone, vm);
		return (NULL);
	}
	CTR1(KTR_VM, "vmspace_alloc: %p", vm);
	_vm_map_init(&vm->vm_map, vmspace_pmap(vm), min, max);
	refcount_init(&vm->vm_refcnt, 1);
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

#ifdef RACCT
static void
vmspace_container_reset(struct proc *p)
{

	PROC_LOCK(p);
	racct_set(p, RACCT_DATA, 0);
	racct_set(p, RACCT_STACK, 0);
	racct_set(p, RACCT_RSS, 0);
	racct_set(p, RACCT_MEMLOCK, 0);
	racct_set(p, RACCT_VMEM, 0);
	PROC_UNLOCK(p);
}
#endif

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
	(void)vm_map_remove(&vm->vm_map, vm_map_min(&vm->vm_map),
	    vm_map_max(&vm->vm_map));

	pmap_release(vmspace_pmap(vm));
	vm->vm_map.pmap = NULL;
	uma_zfree(vmspace_zone, vm);
}

void
vmspace_free(struct vmspace *vm)
{

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "vmspace_free() called");

	if (refcount_release(&vm->vm_refcnt))
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
	struct vmspace *vm;
	struct proc *p;
	bool released;

	p = td->td_proc;
	vm = p->p_vmspace;

	/*
	 * Prepare to release the vmspace reference.  The thread that releases
	 * the last reference is responsible for tearing down the vmspace.
	 * However, threads not releasing the final reference must switch to the
	 * kernel's vmspace0 before the decrement so that the subsequent pmap
	 * deactivation does not modify a freed vmspace.
	 */
	refcount_acquire(&vmspace0.vm_refcnt);
	if (!(released = refcount_release_if_last(&vm->vm_refcnt))) {
		if (p->p_vmspace != &vmspace0) {
			PROC_VMSPACE_LOCK(p);
			p->p_vmspace = &vmspace0;
			PROC_VMSPACE_UNLOCK(p);
			pmap_activate(td);
		}
		released = refcount_release(&vm->vm_refcnt);
	}
	if (released) {
		/*
		 * pmap_remove_pages() expects the pmap to be active, so switch
		 * back first if necessary.
		 */
		if (p->p_vmspace != vm) {
			PROC_VMSPACE_LOCK(p);
			p->p_vmspace = vm;
			PROC_VMSPACE_UNLOCK(p);
			pmap_activate(td);
		}
		pmap_remove_pages(vmspace_pmap(vm));
		PROC_VMSPACE_LOCK(p);
		p->p_vmspace = &vmspace0;
		PROC_VMSPACE_UNLOCK(p);
		pmap_activate(td);
		vmspace_dofree(vm);
	}
#ifdef RACCT
	if (racct_enable)
		vmspace_container_reset(p);
#endif
}

/* Acquire reference to vmspace owned by another process. */

struct vmspace *
vmspace_acquire_ref(struct proc *p)
{
	struct vmspace *vm;

	PROC_VMSPACE_LOCK(p);
	vm = p->p_vmspace;
	if (vm == NULL || !refcount_acquire_if_not_zero(&vm->vm_refcnt)) {
		PROC_VMSPACE_UNLOCK(p);
		return (NULL);
	}
	if (vm != p->p_vmspace) {
		PROC_VMSPACE_UNLOCK(p);
		vmspace_free(vm);
		return (NULL);
	}
	PROC_VMSPACE_UNLOCK(p);
	return (vm);
}

/*
 * Switch between vmspaces in an AIO kernel process.
 *
 * The new vmspace is either the vmspace of a user process obtained
 * from an active AIO request or the initial vmspace of the AIO kernel
 * process (when it is idling).  Because user processes will block to
 * drain any active AIO requests before proceeding in exit() or
 * execve(), the reference count for vmspaces from AIO requests can
 * never be 0.  Similarly, AIO kernel processes hold an extra
 * reference on their initial vmspace for the life of the process.  As
 * a result, the 'newvm' vmspace always has a non-zero reference
 * count.  This permits an additional reference on 'newvm' to be
 * acquired via a simple atomic increment rather than the loop in
 * vmspace_acquire_ref() above.
 */
void
vmspace_switch_aio(struct vmspace *newvm)
{
	struct vmspace *oldvm;

	/* XXX: Need some way to assert that this is an aio daemon. */

	KASSERT(refcount_load(&newvm->vm_refcnt) > 0,
	    ("vmspace_switch_aio: newvm unreferenced"));

	oldvm = curproc->p_vmspace;
	if (oldvm == newvm)
		return;

	/*
	 * Point to the new address space and refer to it.
	 */
	curproc->p_vmspace = newvm;
	refcount_acquire(&newvm->vm_refcnt);

	/* Activate the new mapping. */
	pmap_activate(curthread);

	vmspace_free(oldvm);
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

void
vm_map_entry_set_vnode_text(vm_map_entry_t entry, bool add)
{
	vm_object_t object;
	struct vnode *vp;
	bool vp_held;

	if ((entry->eflags & MAP_ENTRY_VN_EXEC) == 0)
		return;
	KASSERT((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0,
	    ("Submap with execs"));
	object = entry->object.vm_object;
	KASSERT(object != NULL, ("No object for text, entry %p", entry));
	if ((object->flags & OBJ_ANON) != 0)
		object = object->handle;
	else
		KASSERT(object->backing_object == NULL,
		    ("non-anon object %p shadows", object));
	KASSERT(object != NULL, ("No content object for text, entry %p obj %p",
	    entry, entry->object.vm_object));

	/*
	 * Mostly, we do not lock the backing object.  It is
	 * referenced by the entry we are processing, so it cannot go
	 * away.
	 */
	vm_pager_getvp(object, &vp, &vp_held);
	if (vp != NULL) {
		if (add) {
			VOP_SET_TEXT_CHECKED(vp);
		} else {
			vn_lock(vp, LK_SHARED | LK_RETRY);
			VOP_UNSET_TEXT_CHECKED(vp);
			VOP_UNLOCK(vp);
		}
		if (vp_held)
			vdrop(vp);
	}
}

/*
 * Use a different name for this vm_map_entry field when it's use
 * is not consistent with its use as part of an ordered search tree.
 */
#define defer_next right

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
		next = entry->defer_next;
		MPASS((entry->eflags & (MAP_ENTRY_WRITECNT |
		    MAP_ENTRY_VN_EXEC)) != (MAP_ENTRY_WRITECNT |
		    MAP_ENTRY_VN_EXEC));
		if ((entry->eflags & MAP_ENTRY_WRITECNT) != 0) {
			/*
			 * Decrement the object's writemappings and
			 * possibly the vnode's v_writecount.
			 */
			KASSERT((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0,
			    ("Submap with writecount"));
			object = entry->object.vm_object;
			KASSERT(object != NULL, ("No object for writecount"));
			vm_pager_release_writecount(object, entry->start,
			    entry->end);
		}
		vm_map_entry_set_vnode_text(entry, false);
		vm_map_entry_deallocate(entry, FALSE);
		entry = next;
	}
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

enum { VMMAP_CHECK_NONE, VMMAP_CHECK_UNLOCK, VMMAP_CHECK_ALL };
#ifdef DIAGNOSTIC
static int enable_vmmap_check = VMMAP_CHECK_UNLOCK;
#else
static int enable_vmmap_check = VMMAP_CHECK_NONE;
#endif
SYSCTL_INT(_debug, OID_AUTO, vmmap_check, CTLFLAG_RWTUN,
    &enable_vmmap_check, 0, "Enable vm map consistency checking");

static void _vm_map_assert_consistent(vm_map_t map, int check);

#define VM_MAP_ASSERT_CONSISTENT(map) \
    _vm_map_assert_consistent(map, VMMAP_CHECK_ALL)
#ifdef DIAGNOSTIC
#define VM_MAP_UNLOCK_CONSISTENT(map) do {				\
	if (map->nupdates > map->nentries) {				\
		_vm_map_assert_consistent(map, VMMAP_CHECK_UNLOCK);	\
		map->nupdates = 0;					\
	}								\
} while (0)
#else
#define VM_MAP_UNLOCK_CONSISTENT(map)
#endif
#else
#define	VM_MAP_ASSERT_LOCKED(map)
#define VM_MAP_ASSERT_CONSISTENT(map)
#define VM_MAP_UNLOCK_CONSISTENT(map)
#endif /* INVARIANTS */

void
_vm_map_unlock(vm_map_t map, const char *file, int line)
{

	VM_MAP_UNLOCK_CONSISTENT(map);
	if (map->system_map) {
#ifndef UMA_MD_SMALL_ALLOC
		if (map == kernel_map && (map->flags & MAP_REPLENISH) != 0) {
			uma_prealloc(kmapentzone, 1);
			map->flags &= ~MAP_REPLENISH;
		}
#endif
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	} else {
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

	if (map->system_map) {
		KASSERT((map->flags & MAP_REPLENISH) == 0,
		    ("%s: MAP_REPLENISH leaked", __func__));
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	} else {
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
		KASSERT((map->flags & MAP_REPLENISH) == 0,
		    ("%s: MAP_REPLENISH leaked", __func__));
		mtx_assert_(&map->system_mtx, MA_OWNED, file, line);
	} else {
		VM_MAP_UNLOCK_CONSISTENT(map);
		sx_downgrade_(&map->lock, file, line);
	}
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

	VM_MAP_UNLOCK_CONSISTENT(map);
	mtx_lock(&map_sleep_mtx);
	if (map->system_map) {
		KASSERT((map->flags & MAP_REPLENISH) == 0,
		    ("%s: MAP_REPLENISH leaked", __func__));
		mtx_unlock_flags_(&map->system_mtx, 0, file, line);
	} else {
		sx_xunlock_(&map->lock, file, line);
	}
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
 * Initialize an existing vm_map structure
 * such as that in the vmspace structure.
 */
static void
_vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min, vm_offset_t max)
{

	map->header.eflags = MAP_ENTRY_HEADER;
	map->needs_wakeup = FALSE;
	map->system_map = 0;
	map->pmap = pmap;
	map->header.end = min;
	map->header.start = max;
	map->flags = 0;
	map->header.left = map->header.right = &map->header;
	map->root = NULL;
	map->timestamp = 0;
	map->busy = 0;
	map->anon_loc = 0;
#ifdef DIAGNOSTIC
	map->nupdates = 0;
#endif
}

void
vm_map_init(vm_map_t map, pmap_t pmap, vm_offset_t min, vm_offset_t max)
{

	_vm_map_init(map, pmap, min, max);
	mtx_init(&map->system_mtx, "vm map (system)", NULL,
	    MTX_DEF | MTX_DUPOK);
	sx_init(&map->lock, "vm map (user)");
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

#ifndef UMA_MD_SMALL_ALLOC
	if (map == kernel_map) {
		VM_MAP_ASSERT_LOCKED(map);

		/*
		 * A new slab of kernel map entries cannot be allocated at this
		 * point because the kernel map has not yet been updated to
		 * reflect the caller's request.  Therefore, we allocate a new
		 * map entry, dipping into the reserve if necessary, and set a
		 * flag indicating that the reserve must be replenished before
		 * the map is unlocked.
		 */
		new_entry = uma_zalloc(kmapentzone, M_NOWAIT | M_NOVM);
		if (new_entry == NULL) {
			new_entry = uma_zalloc(kmapentzone,
			    M_NOWAIT | M_NOVM | M_USE_RESERVE);
			kernel_map->flags |= MAP_REPLENISH;
		}
	} else
#endif
	if (map->system_map) {
		new_entry = uma_zalloc(kmapentzone, M_NOWAIT);
	} else {
		new_entry = uma_zalloc(mapentzone, M_WAITOK);
	}
	KASSERT(new_entry != NULL,
	    ("vm_map_entry_create: kernel resources exhausted"));
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
 *	vm_map_entry_max_free_{left,right}:
 *
 *	Compute the size of the largest free gap between two entries,
 *	one the root of a tree and the other the ancestor of that root
 *	that is the least or greatest ancestor found on the search path.
 */
static inline vm_size_t
vm_map_entry_max_free_left(vm_map_entry_t root, vm_map_entry_t left_ancestor)
{

	return (root->left != left_ancestor ?
	    root->left->max_free : root->start - left_ancestor->end);
}

static inline vm_size_t
vm_map_entry_max_free_right(vm_map_entry_t root, vm_map_entry_t right_ancestor)
{

	return (root->right != right_ancestor ?
	    root->right->max_free : right_ancestor->start - root->end);
}

/*
 *	vm_map_entry_{pred,succ}:
 *
 *	Find the {predecessor, successor} of the entry by taking one step
 *	in the appropriate direction and backtracking as much as necessary.
 *	vm_map_entry_succ is defined in vm_map.h.
 */
static inline vm_map_entry_t
vm_map_entry_pred(vm_map_entry_t entry)
{
	vm_map_entry_t prior;

	prior = entry->left;
	if (prior->right->start < entry->start) {
		do
			prior = prior->right;
		while (prior->right != entry);
	}
	return (prior);
}

static inline vm_size_t
vm_size_max(vm_size_t a, vm_size_t b)
{

	return (a > b ? a : b);
}

#define SPLAY_LEFT_STEP(root, y, llist, rlist, test) do {		\
	vm_map_entry_t z;						\
	vm_size_t max_free;						\
									\
	/*								\
	 * Infer root->right->max_free == root->max_free when		\
	 * y->max_free < root->max_free || root->max_free == 0.		\
	 * Otherwise, look right to find it.				\
	 */								\
	y = root->left;							\
	max_free = root->max_free;					\
	KASSERT(max_free == vm_size_max(				\
	    vm_map_entry_max_free_left(root, llist),			\
	    vm_map_entry_max_free_right(root, rlist)),			\
	    ("%s: max_free invariant fails", __func__));		\
	if (max_free - 1 < vm_map_entry_max_free_left(root, llist))	\
		max_free = vm_map_entry_max_free_right(root, rlist);	\
	if (y != llist && (test)) {					\
		/* Rotate right and make y root. */			\
		z = y->right;						\
		if (z != root) {					\
			root->left = z;					\
			y->right = root;				\
			if (max_free < y->max_free)			\
			    root->max_free = max_free =			\
			    vm_size_max(max_free, z->max_free);		\
		} else if (max_free < y->max_free)			\
			root->max_free = max_free =			\
			    vm_size_max(max_free, root->start - y->end);\
		root = y;						\
		y = root->left;						\
	}								\
	/* Copy right->max_free.  Put root on rlist. */			\
	root->max_free = max_free;					\
	KASSERT(max_free == vm_map_entry_max_free_right(root, rlist),	\
	    ("%s: max_free not copied from right", __func__));		\
	root->left = rlist;						\
	rlist = root;							\
	root = y != llist ? y : NULL;					\
} while (0)

#define SPLAY_RIGHT_STEP(root, y, llist, rlist, test) do {		\
	vm_map_entry_t z;						\
	vm_size_t max_free;						\
									\
	/*								\
	 * Infer root->left->max_free == root->max_free when		\
	 * y->max_free < root->max_free || root->max_free == 0.		\
	 * Otherwise, look left to find it.				\
	 */								\
	y = root->right;						\
	max_free = root->max_free;					\
	KASSERT(max_free == vm_size_max(				\
	    vm_map_entry_max_free_left(root, llist),			\
	    vm_map_entry_max_free_right(root, rlist)),			\
	    ("%s: max_free invariant fails", __func__));		\
	if (max_free - 1 < vm_map_entry_max_free_right(root, rlist))	\
		max_free = vm_map_entry_max_free_left(root, llist);	\
	if (y != rlist && (test)) {					\
		/* Rotate left and make y root. */			\
		z = y->left;						\
		if (z != root) {					\
			root->right = z;				\
			y->left = root;					\
			if (max_free < y->max_free)			\
			    root->max_free = max_free =			\
			    vm_size_max(max_free, z->max_free);		\
		} else if (max_free < y->max_free)			\
			root->max_free = max_free =			\
			    vm_size_max(max_free, y->start - root->end);\
		root = y;						\
		y = root->right;					\
	}								\
	/* Copy left->max_free.  Put root on llist. */			\
	root->max_free = max_free;					\
	KASSERT(max_free == vm_map_entry_max_free_left(root, llist),	\
	    ("%s: max_free not copied from left", __func__));		\
	root->right = llist;						\
	llist = root;							\
	root = y != rlist ? y : NULL;					\
} while (0)

/*
 * Walk down the tree until we find addr or a gap where addr would go, breaking
 * off left and right subtrees of nodes less than, or greater than addr.  Treat
 * subtrees with root->max_free < length as empty trees.  llist and rlist are
 * the two sides in reverse order (bottom-up), with llist linked by the right
 * pointer and rlist linked by the left pointer in the vm_map_entry, and both
 * lists terminated by &map->header.  This function, and the subsequent call to
 * vm_map_splay_merge_{left,right,pred,succ}, rely on the start and end address
 * values in &map->header.
 */
static __always_inline vm_map_entry_t
vm_map_splay_split(vm_map_t map, vm_offset_t addr, vm_size_t length,
    vm_map_entry_t *llist, vm_map_entry_t *rlist)
{
	vm_map_entry_t left, right, root, y;

	left = right = &map->header;
	root = map->root;
	while (root != NULL && root->max_free >= length) {
		KASSERT(left->end <= root->start &&
		    root->end <= right->start,
		    ("%s: root not within tree bounds", __func__));
		if (addr < root->start) {
			SPLAY_LEFT_STEP(root, y, left, right,
			    y->max_free >= length && addr < y->start);
		} else if (addr >= root->end) {
			SPLAY_RIGHT_STEP(root, y, left, right,
			    y->max_free >= length && addr >= y->end);
		} else
			break;
	}
	*llist = left;
	*rlist = right;
	return (root);
}

static __always_inline void
vm_map_splay_findnext(vm_map_entry_t root, vm_map_entry_t *rlist)
{
	vm_map_entry_t hi, right, y;

	right = *rlist;
	hi = root->right == right ? NULL : root->right;
	if (hi == NULL)
		return;
	do
		SPLAY_LEFT_STEP(hi, y, root, right, true);
	while (hi != NULL);
	*rlist = right;
}

static __always_inline void
vm_map_splay_findprev(vm_map_entry_t root, vm_map_entry_t *llist)
{
	vm_map_entry_t left, lo, y;

	left = *llist;
	lo = root->left == left ? NULL : root->left;
	if (lo == NULL)
		return;
	do
		SPLAY_RIGHT_STEP(lo, y, left, root, true);
	while (lo != NULL);
	*llist = left;
}

static inline void
vm_map_entry_swap(vm_map_entry_t *a, vm_map_entry_t *b)
{
	vm_map_entry_t tmp;

	tmp = *b;
	*b = *a;
	*a = tmp;
}

/*
 * Walk back up the two spines, flip the pointers and set max_free.  The
 * subtrees of the root go at the bottom of llist and rlist.
 */
static vm_size_t
vm_map_splay_merge_left_walk(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t tail, vm_size_t max_free, vm_map_entry_t llist)
{
	do {
		/*
		 * The max_free values of the children of llist are in
		 * llist->max_free and max_free.  Update with the
		 * max value.
		 */
		llist->max_free = max_free =
		    vm_size_max(llist->max_free, max_free);
		vm_map_entry_swap(&llist->right, &tail);
		vm_map_entry_swap(&tail, &llist);
	} while (llist != header);
	root->left = tail;
	return (max_free);
}

/*
 * When llist is known to be the predecessor of root.
 */
static inline vm_size_t
vm_map_splay_merge_pred(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t llist)
{
	vm_size_t max_free;

	max_free = root->start - llist->end;
	if (llist != header) {
		max_free = vm_map_splay_merge_left_walk(header, root,
		    root, max_free, llist);
	} else {
		root->left = header;
		header->right = root;
	}
	return (max_free);
}

/*
 * When llist may or may not be the predecessor of root.
 */
static inline vm_size_t
vm_map_splay_merge_left(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t llist)
{
	vm_size_t max_free;

	max_free = vm_map_entry_max_free_left(root, llist);
	if (llist != header) {
		max_free = vm_map_splay_merge_left_walk(header, root,
		    root->left == llist ? root : root->left,
		    max_free, llist);
	}
	return (max_free);
}

static vm_size_t
vm_map_splay_merge_right_walk(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t tail, vm_size_t max_free, vm_map_entry_t rlist)
{
	do {
		/*
		 * The max_free values of the children of rlist are in
		 * rlist->max_free and max_free.  Update with the
		 * max value.
		 */
		rlist->max_free = max_free =
		    vm_size_max(rlist->max_free, max_free);
		vm_map_entry_swap(&rlist->left, &tail);
		vm_map_entry_swap(&tail, &rlist);
	} while (rlist != header);
	root->right = tail;
	return (max_free);
}

/*
 * When rlist is known to be the succecessor of root.
 */
static inline vm_size_t
vm_map_splay_merge_succ(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t rlist)
{
	vm_size_t max_free;

	max_free = rlist->start - root->end;
	if (rlist != header) {
		max_free = vm_map_splay_merge_right_walk(header, root,
		    root, max_free, rlist);
	} else {
		root->right = header;
		header->left = root;
	}
	return (max_free);
}

/*
 * When rlist may or may not be the succecessor of root.
 */
static inline vm_size_t
vm_map_splay_merge_right(vm_map_entry_t header, vm_map_entry_t root,
    vm_map_entry_t rlist)
{
	vm_size_t max_free;

	max_free = vm_map_entry_max_free_right(root, rlist);
	if (rlist != header) {
		max_free = vm_map_splay_merge_right_walk(header, root,
		    root->right == rlist ? root : root->right,
		    max_free, rlist);
	}
	return (max_free);
}

/*
 *	vm_map_splay:
 *
 *	The Sleator and Tarjan top-down splay algorithm with the
 *	following variation.  Max_free must be computed bottom-up, so
 *	on the downward pass, maintain the left and right spines in
 *	reverse order.  Then, make a second pass up each side to fix
 *	the pointers and compute max_free.  The time bound is O(log n)
 *	amortized.
 *
 *	The tree is threaded, which means that there are no null pointers.
 *	When a node has no left child, its left pointer points to its
 *	predecessor, which the last ancestor on the search path from the root
 *	where the search branched right.  Likewise, when a node has no right
 *	child, its right pointer points to its successor.  The map header node
 *	is the predecessor of the first map entry, and the successor of the
 *	last.
 *
 *	The new root is the vm_map_entry containing "addr", or else an
 *	adjacent entry (lower if possible) if addr is not in the tree.
 *
 *	The map must be locked, and leaves it so.
 *
 *	Returns: the new root.
 */
static vm_map_entry_t
vm_map_splay(vm_map_t map, vm_offset_t addr)
{
	vm_map_entry_t header, llist, rlist, root;
	vm_size_t max_free_left, max_free_right;

	header = &map->header;
	root = vm_map_splay_split(map, addr, 0, &llist, &rlist);
	if (root != NULL) {
		max_free_left = vm_map_splay_merge_left(header, root, llist);
		max_free_right = vm_map_splay_merge_right(header, root, rlist);
	} else if (llist != header) {
		/*
		 * Recover the greatest node in the left
		 * subtree and make it the root.
		 */
		root = llist;
		llist = root->right;
		max_free_left = vm_map_splay_merge_left(header, root, llist);
		max_free_right = vm_map_splay_merge_succ(header, root, rlist);
	} else if (rlist != header) {
		/*
		 * Recover the least node in the right
		 * subtree and make it the root.
		 */
		root = rlist;
		rlist = root->left;
		max_free_left = vm_map_splay_merge_pred(header, root, llist);
		max_free_right = vm_map_splay_merge_right(header, root, rlist);
	} else {
		/* There is no root. */
		return (NULL);
	}
	root->max_free = vm_size_max(max_free_left, max_free_right);
	map->root = root;
	VM_MAP_ASSERT_CONSISTENT(map);
	return (root);
}

/*
 *	vm_map_entry_{un,}link:
 *
 *	Insert/remove entries from maps.  On linking, if new entry clips
 *	existing entry, trim existing entry to avoid overlap, and manage
 *	offsets.  On unlinking, merge disappearing entry with neighbor, if
 *	called for, and manage offsets.  Callers should not modify fields in
 *	entries already mapped.
 */
static void
vm_map_entry_link(vm_map_t map, vm_map_entry_t entry)
{
	vm_map_entry_t header, llist, rlist, root;
	vm_size_t max_free_left, max_free_right;

	CTR3(KTR_VM,
	    "vm_map_entry_link: map %p, nentries %d, entry %p", map,
	    map->nentries, entry);
	VM_MAP_ASSERT_LOCKED(map);
	map->nentries++;
	header = &map->header;
	root = vm_map_splay_split(map, entry->start, 0, &llist, &rlist);
	if (root == NULL) {
		/*
		 * The new entry does not overlap any existing entry in the
		 * map, so it becomes the new root of the map tree.
		 */
		max_free_left = vm_map_splay_merge_pred(header, entry, llist);
		max_free_right = vm_map_splay_merge_succ(header, entry, rlist);
	} else if (entry->start == root->start) {
		/*
		 * The new entry is a clone of root, with only the end field
		 * changed.  The root entry will be shrunk to abut the new
		 * entry, and will be the right child of the new root entry in
		 * the modified map.
		 */
		KASSERT(entry->end < root->end,
		    ("%s: clip_start not within entry", __func__));
		vm_map_splay_findprev(root, &llist);
		root->offset += entry->end - root->start;
		root->start = entry->end;
		max_free_left = vm_map_splay_merge_pred(header, entry, llist);
		max_free_right = root->max_free = vm_size_max(
		    vm_map_splay_merge_pred(entry, root, entry),
		    vm_map_splay_merge_right(header, root, rlist));
	} else {
		/*
		 * The new entry is a clone of root, with only the start field
		 * changed.  The root entry will be shrunk to abut the new
		 * entry, and will be the left child of the new root entry in
		 * the modified map.
		 */
		KASSERT(entry->end == root->end,
		    ("%s: clip_start not within entry", __func__));
		vm_map_splay_findnext(root, &rlist);
		entry->offset += entry->start - root->start;
		root->end = entry->start;
		max_free_left = root->max_free = vm_size_max(
		    vm_map_splay_merge_left(header, root, llist),
		    vm_map_splay_merge_succ(entry, root, entry));
		max_free_right = vm_map_splay_merge_succ(header, entry, rlist);
	}
	entry->max_free = vm_size_max(max_free_left, max_free_right);
	map->root = entry;
	VM_MAP_ASSERT_CONSISTENT(map);
}

enum unlink_merge_type {
	UNLINK_MERGE_NONE,
	UNLINK_MERGE_NEXT
};

static void
vm_map_entry_unlink(vm_map_t map, vm_map_entry_t entry,
    enum unlink_merge_type op)
{
	vm_map_entry_t header, llist, rlist, root;
	vm_size_t max_free_left, max_free_right;

	VM_MAP_ASSERT_LOCKED(map);
	header = &map->header;
	root = vm_map_splay_split(map, entry->start, 0, &llist, &rlist);
	KASSERT(root != NULL,
	    ("vm_map_entry_unlink: unlink object not mapped"));

	vm_map_splay_findprev(root, &llist);
	vm_map_splay_findnext(root, &rlist);
	if (op == UNLINK_MERGE_NEXT) {
		rlist->start = root->start;
		rlist->offset = root->offset;
	}
	if (llist != header) {
		root = llist;
		llist = root->right;
		max_free_left = vm_map_splay_merge_left(header, root, llist);
		max_free_right = vm_map_splay_merge_succ(header, root, rlist);
	} else if (rlist != header) {
		root = rlist;
		rlist = root->left;
		max_free_left = vm_map_splay_merge_pred(header, root, llist);
		max_free_right = vm_map_splay_merge_right(header, root, rlist);
	} else {
		header->left = header->right = header;
		root = NULL;
	}
	if (root != NULL)
		root->max_free = vm_size_max(max_free_left, max_free_right);
	map->root = root;
	VM_MAP_ASSERT_CONSISTENT(map);
	map->nentries--;
	CTR3(KTR_VM, "vm_map_entry_unlink: map %p, nentries %d, entry %p", map,
	    map->nentries, entry);
}

/*
 *	vm_map_entry_resize:
 *
 *	Resize a vm_map_entry, recompute the amount of free space that
 *	follows it and propagate that value up the tree.
 *
 *	The map must be locked, and leaves it so.
 */
static void
vm_map_entry_resize(vm_map_t map, vm_map_entry_t entry, vm_size_t grow_amount)
{
	vm_map_entry_t header, llist, rlist, root;

	VM_MAP_ASSERT_LOCKED(map);
	header = &map->header;
	root = vm_map_splay_split(map, entry->start, 0, &llist, &rlist);
	KASSERT(root != NULL, ("%s: resize object not mapped", __func__));
	vm_map_splay_findnext(root, &rlist);
	entry->end += grow_amount;
	root->max_free = vm_size_max(
	    vm_map_splay_merge_left(header, root, llist),
	    vm_map_splay_merge_succ(header, root, rlist));
	map->root = root;
	VM_MAP_ASSERT_CONSISTENT(map);
	CTR4(KTR_VM, "%s: map %p, nentries %d, entry %p",
	    __func__, map, map->nentries, entry);
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
	vm_map_entry_t cur, header, lbound, ubound;
	boolean_t locked;

	/*
	 * If the map is empty, then the map entry immediately preceding
	 * "address" is the map's header.
	 */
	header = &map->header;
	cur = map->root;
	if (cur == NULL) {
		*entry = header;
		return (FALSE);
	}
	if (address >= cur->start && cur->end > address) {
		*entry = cur;
		return (TRUE);
	}
	if ((locked = vm_map_locked(map)) ||
	    sx_try_upgrade(&map->lock)) {
		/*
		 * Splay requires a write lock on the map.  However, it only
		 * restructures the binary search tree; it does not otherwise
		 * change the map.  Thus, the map's timestamp need not change
		 * on a temporary upgrade.
		 */
		cur = vm_map_splay(map, address);
		if (!locked) {
			VM_MAP_UNLOCK_CONSISTENT(map);
			sx_downgrade(&map->lock);
		}

		/*
		 * If "address" is contained within a map entry, the new root
		 * is that map entry.  Otherwise, the new root is a map entry
		 * immediately before or after "address".
		 */
		if (address < cur->start) {
			*entry = header;
			return (FALSE);
		}
		*entry = cur;
		return (address < cur->end);
	}
	/*
	 * Since the map is only locked for read access, perform a
	 * standard binary search tree lookup for "address".
	 */
	lbound = ubound = header;
	for (;;) {
		if (address < cur->start) {
			ubound = cur;
			cur = cur->left;
			if (cur == lbound)
				break;
		} else if (cur->end <= address) {
			lbound = cur;
			cur = cur->right;
			if (cur == ubound)
				break;
		} else {
			*entry = cur;
			return (TRUE);
		}
	}
	*entry = lbound;
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
    vm_offset_t start, vm_offset_t end, vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_map_entry_t new_entry, next_entry, prev_entry;
	struct ucred *cred;
	vm_eflags_t protoeflags;
	vm_inherit_t inheritance;
	u_long bdry;
	u_int bidx;

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT(object != kernel_object ||
	    (cow & MAP_COPY_ON_WRITE) == 0,
	    ("vm_map_insert: kernel object and COW"));
	KASSERT(object == NULL || (cow & MAP_NOFAULT) == 0 ||
	    (cow & MAP_SPLIT_BOUNDARY_MASK) != 0,
	    ("vm_map_insert: paradoxical MAP_NOFAULT request, obj %p cow %#x",
	    object, cow));
	KASSERT((prot & ~max) == 0,
	    ("prot %#x is not subset of max_prot %#x", prot, max));

	/*
	 * Check that the start and end points are not bogus.
	 */
	if (start == end || !vm_map_range_valid(map, start, end))
		return (KERN_INVALID_ADDRESS);

	if ((map->flags & MAP_WXORX) != 0 && (prot & (VM_PROT_WRITE |
	    VM_PROT_EXECUTE)) == (VM_PROT_WRITE | VM_PROT_EXECUTE))
		return (KERN_PROTECTION_FAILURE);

	/*
	 * Find the entry prior to the proposed starting address; if it's part
	 * of an existing entry, this range is bogus.
	 */
	if (vm_map_lookup_entry(map, start, &prev_entry))
		return (KERN_NO_SPACE);

	/*
	 * Assert that the next entry doesn't overlap the end point.
	 */
	next_entry = vm_map_entry_succ(prev_entry);
	if (next_entry->start < end)
		return (KERN_NO_SPACE);

	if ((cow & MAP_CREATE_GUARD) != 0 && (object != NULL ||
	    max != VM_PROT_NONE))
		return (KERN_INVALID_ARGUMENT);

	protoeflags = 0;
	if (cow & MAP_COPY_ON_WRITE)
		protoeflags |= MAP_ENTRY_COW | MAP_ENTRY_NEEDS_COPY;
	if (cow & MAP_NOFAULT)
		protoeflags |= MAP_ENTRY_NOFAULT;
	if (cow & MAP_DISABLE_SYNCER)
		protoeflags |= MAP_ENTRY_NOSYNC;
	if (cow & MAP_DISABLE_COREDUMP)
		protoeflags |= MAP_ENTRY_NOCOREDUMP;
	if (cow & MAP_STACK_GROWS_DOWN)
		protoeflags |= MAP_ENTRY_GROWS_DOWN;
	if (cow & MAP_STACK_GROWS_UP)
		protoeflags |= MAP_ENTRY_GROWS_UP;
	if (cow & MAP_WRITECOUNT)
		protoeflags |= MAP_ENTRY_WRITECNT;
	if (cow & MAP_VN_EXEC)
		protoeflags |= MAP_ENTRY_VN_EXEC;
	if ((cow & MAP_CREATE_GUARD) != 0)
		protoeflags |= MAP_ENTRY_GUARD;
	if ((cow & MAP_CREATE_STACK_GAP_DN) != 0)
		protoeflags |= MAP_ENTRY_STACK_GAP_DN;
	if ((cow & MAP_CREATE_STACK_GAP_UP) != 0)
		protoeflags |= MAP_ENTRY_STACK_GAP_UP;
	if (cow & MAP_INHERIT_SHARE)
		inheritance = VM_INHERIT_SHARE;
	else
		inheritance = VM_INHERIT_DEFAULT;
	if ((cow & MAP_SPLIT_BOUNDARY_MASK) != 0) {
		/* This magically ignores index 0, for usual page size. */
		bidx = (cow & MAP_SPLIT_BOUNDARY_MASK) >>
		    MAP_SPLIT_BOUNDARY_SHIFT;
		if (bidx >= MAXPAGESIZES)
			return (KERN_INVALID_ARGUMENT);
		bdry = pagesizes[bidx] - 1;
		if ((start & bdry) != 0 || (end & bdry) != 0)
			return (KERN_INVALID_ARGUMENT);
		protoeflags |= bidx << MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
	}

	cred = NULL;
	if ((cow & (MAP_ACC_NO_CHARGE | MAP_NOFAULT | MAP_CREATE_GUARD)) != 0)
		goto charged;
	if ((cow & MAP_ACC_CHARGED) || ((prot & VM_PROT_WRITE) &&
	    ((protoeflags & MAP_ENTRY_NEEDS_COPY) || object == NULL))) {
		if (!(cow & MAP_ACC_CHARGED) && !swap_reserve(end - start))
			return (KERN_RESOURCE_SHORTAGE);
		KASSERT(object == NULL ||
		    (protoeflags & MAP_ENTRY_NEEDS_COPY) != 0 ||
		    object->cred == NULL,
		    ("overcommit: vm_map_insert o %p", object));
		cred = curthread->td_ucred;
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
		if ((object->flags & OBJ_ANON) != 0) {
			VM_OBJECT_WLOCK(object);
			if (object->ref_count > 1 || object->shadow_count != 0)
				vm_object_clear_flag(object, OBJ_ONEMAPPING);
			VM_OBJECT_WUNLOCK(object);
		}
	} else if ((prev_entry->eflags & ~MAP_ENTRY_USER_WIRED) ==
	    protoeflags &&
	    (cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP |
	    MAP_VN_EXEC)) == 0 &&
	    prev_entry->end == start && (prev_entry->cred == cred ||
	    (prev_entry->object.vm_object != NULL &&
	    prev_entry->object.vm_object->cred == cred)) &&
	    vm_object_coalesce(prev_entry->object.vm_object,
	    prev_entry->offset,
	    (vm_size_t)(prev_entry->end - prev_entry->start),
	    (vm_size_t)(end - prev_entry->end), cred != NULL &&
	    (protoeflags & MAP_ENTRY_NEEDS_COPY) == 0)) {
		/*
		 * We were able to extend the object.  Determine if we
		 * can extend the previous map entry to include the
		 * new range as well.
		 */
		if (prev_entry->inheritance == inheritance &&
		    prev_entry->protection == prot &&
		    prev_entry->max_protection == max &&
		    prev_entry->wired_count == 0) {
			KASSERT((prev_entry->eflags & MAP_ENTRY_USER_WIRED) ==
			    0, ("prev_entry %p has incoherent wiring",
			    prev_entry));
			if ((prev_entry->eflags & MAP_ENTRY_GUARD) == 0)
				map->size += end - prev_entry->end;
			vm_map_entry_resize(map, prev_entry,
			    end - prev_entry->end);
			vm_map_try_merge_entries(map, prev_entry, next_entry);
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
			cred = NULL;
		}
	}
	if (cred != NULL)
		crhold(cred);

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

	new_entry->inheritance = inheritance;
	new_entry->protection = prot;
	new_entry->max_protection = max;
	new_entry->wired_count = 0;
	new_entry->wiring_thread = NULL;
	new_entry->read_ahead = VM_FAULT_READ_AHEAD_INIT;
	new_entry->next_read = start;

	KASSERT(cred == NULL || !ENTRY_CHARGED(new_entry),
	    ("overcommit: vm_map_insert leaks vm_map %p", new_entry));
	new_entry->cred = cred;

	/*
	 * Insert the new entry into the list
	 */
	vm_map_entry_link(map, new_entry);
	if ((new_entry->eflags & MAP_ENTRY_GUARD) == 0)
		map->size += new_entry->end - new_entry->start;

	/*
	 * Try to coalesce the new entry with both the previous and next
	 * entries in the list.  Previously, we only attempted to coalesce
	 * with the previous entry when object is NULL.  Here, we handle the
	 * other cases, which are less common.
	 */
	vm_map_try_merge_entries(map, prev_entry, new_entry);
	vm_map_try_merge_entries(map, new_entry, next_entry);

	if ((cow & (MAP_PREFAULT | MAP_PREFAULT_PARTIAL)) != 0) {
		vm_map_pmap_enter(map, start, prot, object, OFF_TO_IDX(offset),
		    end - start, cow & MAP_PREFAULT_PARTIAL);
	}

	return (KERN_SUCCESS);
}

/*
 *	vm_map_findspace:
 *
 *	Find the first fit (lowest VM address) for "length" free bytes
 *	beginning at address >= start in the given map.
 *
 *	In a vm_map_entry, "max_free" is the maximum amount of
 *	contiguous free space between an entry in its subtree and a
 *	neighbor of that entry.  This allows finding a free region in
 *	one path down the tree, so O(log n) amortized with splay
 *	trees.
 *
 *	The map must be locked, and leaves it so.
 *
 *	Returns: starting address if sufficient space,
 *		 vm_map_max(map)-length+1 if insufficient space.
 */
vm_offset_t
vm_map_findspace(vm_map_t map, vm_offset_t start, vm_size_t length)
{
	vm_map_entry_t header, llist, rlist, root, y;
	vm_size_t left_length, max_free_left, max_free_right;
	vm_offset_t gap_end;

	VM_MAP_ASSERT_LOCKED(map);

	/*
	 * Request must fit within min/max VM address and must avoid
	 * address wrap.
	 */
	start = MAX(start, vm_map_min(map));
	if (start >= vm_map_max(map) || length > vm_map_max(map) - start)
		return (vm_map_max(map) - length + 1);

	/* Empty tree means wide open address space. */
	if (map->root == NULL)
		return (start);

	/*
	 * After splay_split, if start is within an entry, push it to the start
	 * of the following gap.  If rlist is at the end of the gap containing
	 * start, save the end of that gap in gap_end to see if the gap is big
	 * enough; otherwise set gap_end to start skip gap-checking and move
	 * directly to a search of the right subtree.
	 */
	header = &map->header;
	root = vm_map_splay_split(map, start, length, &llist, &rlist);
	gap_end = rlist->start;
	if (root != NULL) {
		start = root->end;
		if (root->right != rlist)
			gap_end = start;
		max_free_left = vm_map_splay_merge_left(header, root, llist);
		max_free_right = vm_map_splay_merge_right(header, root, rlist);
	} else if (rlist != header) {
		root = rlist;
		rlist = root->left;
		max_free_left = vm_map_splay_merge_pred(header, root, llist);
		max_free_right = vm_map_splay_merge_right(header, root, rlist);
	} else {
		root = llist;
		llist = root->right;
		max_free_left = vm_map_splay_merge_left(header, root, llist);
		max_free_right = vm_map_splay_merge_succ(header, root, rlist);
	}
	root->max_free = vm_size_max(max_free_left, max_free_right);
	map->root = root;
	VM_MAP_ASSERT_CONSISTENT(map);
	if (length <= gap_end - start)
		return (start);

	/* With max_free, can immediately tell if no solution. */
	if (root->right == header || length > root->right->max_free)
		return (vm_map_max(map) - length + 1);

	/*
	 * Splay for the least large-enough gap in the right subtree.
	 */
	llist = rlist = header;
	for (left_length = 0;;
	    left_length = vm_map_entry_max_free_left(root, llist)) {
		if (length <= left_length)
			SPLAY_LEFT_STEP(root, y, llist, rlist,
			    length <= vm_map_entry_max_free_left(y, llist));
		else
			SPLAY_RIGHT_STEP(root, y, llist, rlist,
			    length > vm_map_entry_max_free_left(y, root));
		if (root == NULL)
			break;
	}
	root = llist;
	llist = root->right;
	max_free_left = vm_map_splay_merge_left(header, root, llist);
	if (rlist == header) {
		root->max_free = vm_size_max(max_free_left,
		    vm_map_splay_merge_succ(header, root, rlist));
	} else {
		y = rlist;
		rlist = y->left;
		y->max_free = vm_size_max(
		    vm_map_splay_merge_pred(root, y, root),
		    vm_map_splay_merge_right(header, y, rlist));
		root->max_free = vm_size_max(max_free_left, y->max_free);
	}
	map->root = root;
	VM_MAP_ASSERT_CONSISTENT(map);
	return (root->end);
}

int
vm_map_fixed(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t start, vm_size_t length, vm_prot_t prot,
    vm_prot_t max, int cow)
{
	vm_offset_t end;
	int result;

	end = start + length;
	KASSERT((cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) == 0 ||
	    object == NULL,
	    ("vm_map_fixed: non-NULL backing object for stack"));
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if ((cow & MAP_CHECK_EXCL) == 0) {
		result = vm_map_delete(map, start, end);
		if (result != KERN_SUCCESS)
			goto out;
	}
	if ((cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) != 0) {
		result = vm_map_stack_locked(map, start, length, sgrowsiz,
		    prot, max, cow);
	} else {
		result = vm_map_insert(map, object, offset, start, end,
		    prot, max, cow);
	}
out:
	vm_map_unlock(map);
	return (result);
}

static const int aslr_pages_rnd_64[2] = {0x1000, 0x10};
static const int aslr_pages_rnd_32[2] = {0x100, 0x4};

static int cluster_anon = 1;
SYSCTL_INT(_vm, OID_AUTO, cluster_anon, CTLFLAG_RW,
    &cluster_anon, 0,
    "Cluster anonymous mappings: 0 = no, 1 = yes if no hint, 2 = always");

static bool
clustering_anon_allowed(vm_offset_t addr)
{

	switch (cluster_anon) {
	case 0:
		return (false);
	case 1:
		return (addr == 0);
	case 2:
	default:
		return (true);
	}
}

static long aslr_restarts;
SYSCTL_LONG(_vm, OID_AUTO, aslr_restarts, CTLFLAG_RD,
    &aslr_restarts, 0,
    "Number of aslr failures");

/*
 * Searches for the specified amount of free space in the given map with the
 * specified alignment.  Performs an address-ordered, first-fit search from
 * the given address "*addr", with an optional upper bound "max_addr".  If the
 * parameter "alignment" is zero, then the alignment is computed from the
 * given (object, offset) pair so as to enable the greatest possible use of
 * superpage mappings.  Returns KERN_SUCCESS and the address of the free space
 * in "*addr" if successful.  Otherwise, returns KERN_NO_SPACE.
 *
 * The map must be locked.  Initially, there must be at least "length" bytes
 * of free space at the given address.
 */
static int
vm_map_alignspace(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t length, vm_offset_t max_addr,
    vm_offset_t alignment)
{
	vm_offset_t aligned_addr, free_addr;

	VM_MAP_ASSERT_LOCKED(map);
	free_addr = *addr;
	KASSERT(free_addr == vm_map_findspace(map, free_addr, length),
	    ("caller failed to provide space %#jx at address %p",
	     (uintmax_t)length, (void *)free_addr));
	for (;;) {
		/*
		 * At the start of every iteration, the free space at address
		 * "*addr" is at least "length" bytes.
		 */
		if (alignment == 0)
			pmap_align_superpage(object, offset, addr, length);
		else
			*addr = roundup2(*addr, alignment);
		aligned_addr = *addr;
		if (aligned_addr == free_addr) {
			/*
			 * Alignment did not change "*addr", so "*addr" must
			 * still provide sufficient free space.
			 */
			return (KERN_SUCCESS);
		}

		/*
		 * Test for address wrap on "*addr".  A wrapped "*addr" could
		 * be a valid address, in which case vm_map_findspace() cannot
		 * be relied upon to fail.
		 */
		if (aligned_addr < free_addr)
			return (KERN_NO_SPACE);
		*addr = vm_map_findspace(map, aligned_addr, length);
		if (*addr + length > vm_map_max(map) ||
		    (max_addr != 0 && *addr + length > max_addr))
			return (KERN_NO_SPACE);
		free_addr = *addr;
		if (free_addr == aligned_addr) {
			/*
			 * If a successful call to vm_map_findspace() did not
			 * change "*addr", then "*addr" must still be aligned
			 * and provide sufficient free space.
			 */
			return (KERN_SUCCESS);
		}
	}
}

int
vm_map_find_aligned(vm_map_t map, vm_offset_t *addr, vm_size_t length,
    vm_offset_t max_addr, vm_offset_t alignment)
{
	/* XXXKIB ASLR eh ? */
	*addr = vm_map_findspace(map, *addr, length);
	if (*addr + length > vm_map_max(map) ||
	    (max_addr != 0 && *addr + length > max_addr))
		return (KERN_NO_SPACE);
	return (vm_map_alignspace(map, NULL, 0, addr, length, max_addr,
	    alignment));
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
	vm_offset_t alignment, curr_min_addr, min_addr;
	int gap, pidx, rv, try;
	bool cluster, en_aslr, update_anon;

	KASSERT((cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) == 0 ||
	    object == NULL,
	    ("vm_map_find: non-NULL backing object for stack"));
	MPASS((cow & MAP_REMAP) == 0 || (find_space == VMFS_NO_SPACE &&
	    (cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) == 0));
	if (find_space == VMFS_OPTIMAL_SPACE && (object == NULL ||
	    (object->flags & OBJ_COLORED) == 0))
		find_space = VMFS_ANY_SPACE;
	if (find_space >> 8 != 0) {
		KASSERT((find_space & 0xff) == 0, ("bad VMFS flags"));
		alignment = (vm_offset_t)1 << (find_space >> 8);
	} else
		alignment = 0;
	en_aslr = (map->flags & MAP_ASLR) != 0;
	update_anon = cluster = clustering_anon_allowed(*addr) &&
	    (map->flags & MAP_IS_SUB_MAP) == 0 && max_addr == 0 &&
	    find_space != VMFS_NO_SPACE && object == NULL &&
	    (cow & (MAP_INHERIT_SHARE | MAP_STACK_GROWS_UP |
	    MAP_STACK_GROWS_DOWN)) == 0 && prot != PROT_NONE;
	curr_min_addr = min_addr = *addr;
	if (en_aslr && min_addr == 0 && !cluster &&
	    find_space != VMFS_NO_SPACE &&
	    (map->flags & MAP_ASLR_IGNSTART) != 0)
		curr_min_addr = min_addr = vm_map_min(map);
	try = 0;
	vm_map_lock(map);
	if (cluster) {
		curr_min_addr = map->anon_loc;
		if (curr_min_addr == 0)
			cluster = false;
	}
	if (find_space != VMFS_NO_SPACE) {
		KASSERT(find_space == VMFS_ANY_SPACE ||
		    find_space == VMFS_OPTIMAL_SPACE ||
		    find_space == VMFS_SUPER_SPACE ||
		    alignment != 0, ("unexpected VMFS flag"));
again:
		/*
		 * When creating an anonymous mapping, try clustering
		 * with an existing anonymous mapping first.
		 *
		 * We make up to two attempts to find address space
		 * for a given find_space value. The first attempt may
		 * apply randomization or may cluster with an existing
		 * anonymous mapping. If this first attempt fails,
		 * perform a first-fit search of the available address
		 * space.
		 *
		 * If all tries failed, and find_space is
		 * VMFS_OPTIMAL_SPACE, fallback to VMFS_ANY_SPACE.
		 * Again enable clustering and randomization.
		 */
		try++;
		MPASS(try <= 2);

		if (try == 2) {
			/*
			 * Second try: we failed either to find a
			 * suitable region for randomizing the
			 * allocation, or to cluster with an existing
			 * mapping.  Retry with free run.
			 */
			curr_min_addr = (map->flags & MAP_ASLR_IGNSTART) != 0 ?
			    vm_map_min(map) : min_addr;
			atomic_add_long(&aslr_restarts, 1);
		}

		if (try == 1 && en_aslr && !cluster) {
			/*
			 * Find space for allocation, including
			 * gap needed for later randomization.
			 */
			pidx = MAXPAGESIZES > 1 && pagesizes[1] != 0 &&
			    (find_space == VMFS_SUPER_SPACE || find_space ==
			    VMFS_OPTIMAL_SPACE) ? 1 : 0;
			gap = vm_map_max(map) > MAP_32BIT_MAX_ADDR &&
			    (max_addr == 0 || max_addr > MAP_32BIT_MAX_ADDR) ?
			    aslr_pages_rnd_64[pidx] : aslr_pages_rnd_32[pidx];
			*addr = vm_map_findspace(map, curr_min_addr,
			    length + gap * pagesizes[pidx]);
			if (*addr + length + gap * pagesizes[pidx] >
			    vm_map_max(map))
				goto again;
			/* And randomize the start address. */
			*addr += (arc4random() % gap) * pagesizes[pidx];
			if (max_addr != 0 && *addr + length > max_addr)
				goto again;
		} else {
			*addr = vm_map_findspace(map, curr_min_addr, length);
			if (*addr + length > vm_map_max(map) ||
			    (max_addr != 0 && *addr + length > max_addr)) {
				if (cluster) {
					cluster = false;
					MPASS(try == 1);
					goto again;
				}
				rv = KERN_NO_SPACE;
				goto done;
			}
		}

		if (find_space != VMFS_ANY_SPACE &&
		    (rv = vm_map_alignspace(map, object, offset, addr, length,
		    max_addr, alignment)) != KERN_SUCCESS) {
			if (find_space == VMFS_OPTIMAL_SPACE) {
				find_space = VMFS_ANY_SPACE;
				curr_min_addr = min_addr;
				cluster = update_anon;
				try = 0;
				goto again;
			}
			goto done;
		}
	} else if ((cow & MAP_REMAP) != 0) {
		if (!vm_map_range_valid(map, *addr, *addr + length)) {
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}
		rv = vm_map_delete(map, *addr, *addr + length);
		if (rv != KERN_SUCCESS)
			goto done;
	}
	if ((cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP)) != 0) {
		rv = vm_map_stack_locked(map, *addr, length, sgrowsiz, prot,
		    max, cow);
	} else {
		rv = vm_map_insert(map, object, offset, *addr, *addr + length,
		    prot, max, cow);
	}
	if (rv == KERN_SUCCESS && update_anon)
		map->anon_loc = *addr + length;
done:
	vm_map_unlock(map);
	return (rv);
}

/*
 *	vm_map_find_min() is a variant of vm_map_find() that takes an
 *	additional parameter (min_addr) and treats the given address
 *	(*addr) differently.  Specifically, it treats *addr as a hint
 *	and not as the minimum address where the mapping is created.
 *
 *	This function works in two phases.  First, it tries to
 *	allocate above the hint.  If that fails and the hint is
 *	greater than min_addr, it performs a second pass, replacing
 *	the hint with min_addr as the minimum address for the
 *	allocation.
 */
int
vm_map_find_min(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t length, vm_offset_t min_addr,
    vm_offset_t max_addr, int find_space, vm_prot_t prot, vm_prot_t max,
    int cow)
{
	vm_offset_t hint;
	int rv;

	hint = *addr;
	for (;;) {
		rv = vm_map_find(map, object, offset, addr, length, max_addr,
		    find_space, prot, max, cow);
		if (rv == KERN_SUCCESS || min_addr >= hint)
			return (rv);
		*addr = hint = min_addr;
	}
}

/*
 * A map entry with any of the following flags set must not be merged with
 * another entry.
 */
#define	MAP_ENTRY_NOMERGE_MASK	(MAP_ENTRY_GROWS_DOWN | MAP_ENTRY_GROWS_UP | \
	    MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_IS_SUB_MAP | MAP_ENTRY_VN_EXEC)

static bool
vm_map_mergeable_neighbors(vm_map_entry_t prev, vm_map_entry_t entry)
{

	KASSERT((prev->eflags & MAP_ENTRY_NOMERGE_MASK) == 0 ||
	    (entry->eflags & MAP_ENTRY_NOMERGE_MASK) == 0,
	    ("vm_map_mergeable_neighbors: neither %p nor %p are mergeable",
	    prev, entry));
	return (prev->end == entry->start &&
	    prev->object.vm_object == entry->object.vm_object &&
	    (prev->object.vm_object == NULL ||
	    prev->offset + (prev->end - prev->start) == entry->offset) &&
	    prev->eflags == entry->eflags &&
	    prev->protection == entry->protection &&
	    prev->max_protection == entry->max_protection &&
	    prev->inheritance == entry->inheritance &&
	    prev->wired_count == entry->wired_count &&
	    prev->cred == entry->cred);
}

static void
vm_map_merged_neighbor_dispose(vm_map_t map, vm_map_entry_t entry)
{

	/*
	 * If the backing object is a vnode object, vm_object_deallocate()
	 * calls vrele().  However, vrele() does not lock the vnode because
	 * the vnode has additional references.  Thus, the map lock can be
	 * kept without causing a lock-order reversal with the vnode lock.
	 *
	 * Since we count the number of virtual page mappings in
	 * object->un_pager.vnp.writemappings, the writemappings value
	 * should not be adjusted when the entry is disposed of.
	 */
	if (entry->object.vm_object != NULL)
		vm_object_deallocate(entry->object.vm_object);
	if (entry->cred != NULL)
		crfree(entry->cred);
	vm_map_entry_dispose(map, entry);
}

/*
 *	vm_map_try_merge_entries:
 *
 *	Compare the given map entry to its predecessor, and merge its precessor
 *	into it if possible.  The entry remains valid, and may be extended.
 *	The predecessor may be deleted.
 *
 *	The map must be locked.
 */
void
vm_map_try_merge_entries(vm_map_t map, vm_map_entry_t prev_entry,
    vm_map_entry_t entry)
{

	VM_MAP_ASSERT_LOCKED(map);
	if ((entry->eflags & MAP_ENTRY_NOMERGE_MASK) == 0 &&
	    vm_map_mergeable_neighbors(prev_entry, entry)) {
		vm_map_entry_unlink(map, prev_entry, UNLINK_MERGE_NEXT);
		vm_map_merged_neighbor_dispose(map, prev_entry);
	}
}

/*
 *	vm_map_entry_back:
 *
 *	Allocate an object to back a map entry.
 */
static inline void
vm_map_entry_back(vm_map_entry_t entry)
{
	vm_object_t object;

	KASSERT(entry->object.vm_object == NULL,
	    ("map entry %p has backing object", entry));
	KASSERT((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0,
	    ("map entry %p is a submap", entry));
	object = vm_object_allocate_anon(atop(entry->end - entry->start), NULL,
	    entry->cred, entry->end - entry->start);
	entry->object.vm_object = object;
	entry->offset = 0;
	entry->cred = NULL;
}

/*
 *	vm_map_entry_charge_object
 *
 *	If there is no object backing this entry, create one.  Otherwise, if
 *	the entry has cred, give it to the backing object.
 */
static inline void
vm_map_entry_charge_object(vm_map_t map, vm_map_entry_t entry)
{

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0,
	    ("map entry %p is a submap", entry));
	if (entry->object.vm_object == NULL && !map->system_map &&
	    (entry->eflags & MAP_ENTRY_GUARD) == 0)
		vm_map_entry_back(entry);
	else if (entry->object.vm_object != NULL &&
	    ((entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0) &&
	    entry->cred != NULL) {
		VM_OBJECT_WLOCK(entry->object.vm_object);
		KASSERT(entry->object.vm_object->cred == NULL,
		    ("OVERCOMMIT: %s: both cred e %p", __func__, entry));
		entry->object.vm_object->cred = entry->cred;
		entry->object.vm_object->charge = entry->end - entry->start;
		VM_OBJECT_WUNLOCK(entry->object.vm_object);
		entry->cred = NULL;
	}
}

/*
 *	vm_map_entry_clone
 *
 *	Create a duplicate map entry for clipping.
 */
static vm_map_entry_t
vm_map_entry_clone(vm_map_t map, vm_map_entry_t entry)
{
	vm_map_entry_t new_entry;

	VM_MAP_ASSERT_LOCKED(map);

	/*
	 * Create a backing object now, if none exists, so that more individual
	 * objects won't be created after the map entry is split.
	 */
	vm_map_entry_charge_object(map, entry);

	/* Clone the entry. */
	new_entry = vm_map_entry_create(map);
	*new_entry = *entry;
	if (new_entry->cred != NULL)
		crhold(entry->cred);
	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
		vm_object_reference(new_entry->object.vm_object);
		vm_map_entry_set_vnode_text(new_entry, true);
		/*
		 * The object->un_pager.vnp.writemappings for the object of
		 * MAP_ENTRY_WRITECNT type entry shall be kept as is here.  The
		 * virtual pages are re-distributed among the clipped entries,
		 * so the sum is left the same.
		 */
	}
	return (new_entry);
}

/*
 *	vm_map_clip_start:	[ internal use only ]
 *
 *	Asserts that the given entry begins at or after
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
static int
vm_map_clip_start(vm_map_t map, vm_map_entry_t entry, vm_offset_t startaddr)
{
	vm_map_entry_t new_entry;
	int bdry_idx;

	if (!map->system_map)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "%s: map %p entry %p start 0x%jx", __func__, map, entry,
		    (uintmax_t)startaddr);

	if (startaddr <= entry->start)
		return (KERN_SUCCESS);

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT(entry->end > startaddr && entry->start < startaddr,
	    ("%s: invalid clip of entry %p", __func__, entry));

	bdry_idx = (entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK) >>
	    MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
	if (bdry_idx != 0) {
		if ((startaddr & (pagesizes[bdry_idx] - 1)) != 0)
			return (KERN_INVALID_ARGUMENT);
	}

	new_entry = vm_map_entry_clone(map, entry);

	/*
	 * Split off the front portion.  Insert the new entry BEFORE this one,
	 * so that this entry has the specified starting address.
	 */
	new_entry->end = startaddr;
	vm_map_entry_link(map, new_entry);
	return (KERN_SUCCESS);
}

/*
 *	vm_map_lookup_clip_start:
 *
 *	Find the entry at or just after 'start', and clip it if 'start' is in
 *	the interior of the entry.  Return entry after 'start', and in
 *	prev_entry set the entry before 'start'.
 */
static int
vm_map_lookup_clip_start(vm_map_t map, vm_offset_t start,
    vm_map_entry_t *res_entry, vm_map_entry_t *prev_entry)
{
	vm_map_entry_t entry;
	int rv;

	if (!map->system_map)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "%s: map %p start 0x%jx prev %p", __func__, map,
		    (uintmax_t)start, prev_entry);

	if (vm_map_lookup_entry(map, start, prev_entry)) {
		entry = *prev_entry;
		rv = vm_map_clip_start(map, entry, start);
		if (rv != KERN_SUCCESS)
			return (rv);
		*prev_entry = vm_map_entry_pred(entry);
	} else
		entry = vm_map_entry_succ(*prev_entry);
	*res_entry = entry;
	return (KERN_SUCCESS);
}

/*
 *	vm_map_clip_end:	[ internal use only ]
 *
 *	Asserts that the given entry ends at or before
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
static int
vm_map_clip_end(vm_map_t map, vm_map_entry_t entry, vm_offset_t endaddr)
{
	vm_map_entry_t new_entry;
	int bdry_idx;

	if (!map->system_map)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "%s: map %p entry %p end 0x%jx", __func__, map, entry,
		    (uintmax_t)endaddr);

	if (endaddr >= entry->end)
		return (KERN_SUCCESS);

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT(entry->start < endaddr && entry->end > endaddr,
	    ("%s: invalid clip of entry %p", __func__, entry));

	bdry_idx = (entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK) >>
	    MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
	if (bdry_idx != 0) {
		if ((endaddr & (pagesizes[bdry_idx] - 1)) != 0)
			return (KERN_INVALID_ARGUMENT);
	}

	new_entry = vm_map_entry_clone(map, entry);

	/*
	 * Split off the back portion.  Insert the new entry AFTER this one,
	 * so that this entry has the specified ending address.
	 */
	new_entry->start = endaddr;
	vm_map_entry_link(map, new_entry);

	return (KERN_SUCCESS);
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
	int result;

	result = KERN_INVALID_ARGUMENT;

	vm_map_lock(submap);
	submap->flags |= MAP_IS_SUB_MAP;
	vm_map_unlock(submap);

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (vm_map_lookup_entry(map, start, &entry) && entry->end >= end &&
	    (entry->eflags & MAP_ENTRY_COW) == 0 &&
	    entry->object.vm_object == NULL) {
		result = vm_map_clip_start(map, entry, start);
		if (result != KERN_SUCCESS)
			goto unlock;
		result = vm_map_clip_end(map, entry, end);
		if (result != KERN_SUCCESS)
			goto unlock;
		entry->object.sub_map = submap;
		entry->eflags |= MAP_ENTRY_IS_SUB_MAP;
		result = KERN_SUCCESS;
	}
unlock:
	vm_map_unlock(map);

	if (result != KERN_SUCCESS) {
		vm_map_lock(submap);
		submap->flags &= ~MAP_IS_SUB_MAP;
		vm_map_unlock(submap);
	}
	return (result);
}

/*
 * The maximum number of pages to map if MAP_PREFAULT_PARTIAL is specified
 */
#define	MAX_INIT_PT	96

/*
 *	vm_map_pmap_enter:
 *
 *	Preload the specified map's pmap with mappings to the specified
 *	object's memory-resident pages.  No further physical pages are
 *	allocated, and no further virtual pages are retrieved from secondary
 *	storage.  If the specified flags include MAP_PREFAULT_PARTIAL, then a
 *	limited number of page mappings are created at the low-end of the
 *	specified address range.  (For this purpose, a superpage mapping
 *	counts as one page mapping.)  Otherwise, all resident pages within
 *	the specified address range are mapped.
 */
static void
vm_map_pmap_enter(vm_map_t map, vm_offset_t addr, vm_prot_t prot,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size, int flags)
{
	vm_offset_t start;
	vm_page_t p, p_start;
	vm_pindex_t mask, psize, threshold, tmpidx;

	if ((prot & (VM_PROT_READ | VM_PROT_EXECUTE)) == 0 || object == NULL)
		return;
	if (object->type == OBJT_DEVICE || object->type == OBJT_SG) {
		VM_OBJECT_WLOCK(object);
		if (object->type == OBJT_DEVICE || object->type == OBJT_SG) {
			pmap_object_init_pt(map->pmap, addr, object, pindex,
			    size);
			VM_OBJECT_WUNLOCK(object);
			return;
		}
		VM_OBJECT_LOCK_DOWNGRADE(object);
	} else
		VM_OBJECT_RLOCK(object);

	psize = atop(size);
	if (psize + pindex > object->size) {
		if (pindex >= object->size) {
			VM_OBJECT_RUNLOCK(object);
			return;
		}
		psize = object->size - pindex;
	}

	start = 0;
	p_start = NULL;
	threshold = MAX_INIT_PT;

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
		if (((flags & MAP_PREFAULT_MADVISE) != 0 &&
		    vm_page_count_severe()) ||
		    ((flags & MAP_PREFAULT_PARTIAL) != 0 &&
		    tmpidx >= threshold)) {
			psize = tmpidx;
			break;
		}
		if (vm_page_all_valid(p)) {
			if (p_start == NULL) {
				start = addr + ptoa(tmpidx);
				p_start = p;
			}
			/* Jump ahead if a superpage mapping is possible. */
			if (p->psind > 0 && ((addr + ptoa(tmpidx)) &
			    (pagesizes[p->psind] - 1)) == 0) {
				mask = atop(pagesizes[p->psind]) - 1;
				if (tmpidx + mask < psize &&
				    vm_page_ps_test(p, PS_ALL_VALID, NULL)) {
					p += mask;
					threshold += mask;
				}
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
 *	Sets the protection and/or the maximum protection of the
 *	specified address region in the target map.
 */
int
vm_map_protect(vm_map_t map, vm_offset_t start, vm_offset_t end,
    vm_prot_t new_prot, vm_prot_t new_maxprot, int flags)
{
	vm_map_entry_t entry, first_entry, in_tran, prev_entry;
	vm_object_t obj;
	struct ucred *cred;
	vm_prot_t old_prot;
	int rv;

	if (start == end)
		return (KERN_SUCCESS);

	if ((flags & (VM_MAP_PROTECT_SET_PROT | VM_MAP_PROTECT_SET_MAXPROT)) ==
	    (VM_MAP_PROTECT_SET_PROT | VM_MAP_PROTECT_SET_MAXPROT) &&
	    (new_prot & new_maxprot) != new_prot)
		return (KERN_OUT_OF_BOUNDS);

again:
	in_tran = NULL;
	vm_map_lock(map);

	if ((map->flags & MAP_WXORX) != 0 &&
	    (flags & VM_MAP_PROTECT_SET_PROT) != 0 &&
	    (new_prot & (VM_PROT_WRITE | VM_PROT_EXECUTE)) == (VM_PROT_WRITE |
	    VM_PROT_EXECUTE)) {
		vm_map_unlock(map);
		return (KERN_PROTECTION_FAILURE);
	}

	/*
	 * Ensure that we are not concurrently wiring pages.  vm_map_wire() may
	 * need to fault pages into the map and will drop the map lock while
	 * doing so, and the VM object may end up in an inconsistent state if we
	 * update the protection on the map entry in between faults.
	 */
	vm_map_wait_busy(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (!vm_map_lookup_entry(map, start, &first_entry))
		first_entry = vm_map_entry_succ(first_entry);

	/*
	 * Make a first pass to check for protection violations.
	 */
	for (entry = first_entry; entry->start < end;
	    entry = vm_map_entry_succ(entry)) {
		if ((entry->eflags & MAP_ENTRY_GUARD) != 0)
			continue;
		if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0) {
			vm_map_unlock(map);
			return (KERN_INVALID_ARGUMENT);
		}
		if ((flags & VM_MAP_PROTECT_SET_PROT) == 0)
			new_prot = entry->protection;
		if ((flags & VM_MAP_PROTECT_SET_MAXPROT) == 0)
			new_maxprot = entry->max_protection;
		if ((new_prot & entry->max_protection) != new_prot ||
		    (new_maxprot & entry->max_protection) != new_maxprot) {
			vm_map_unlock(map);
			return (KERN_PROTECTION_FAILURE);
		}
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0)
			in_tran = entry;
	}

	/*
	 * Postpone the operation until all in-transition map entries have
	 * stabilized.  An in-transition entry might already have its pages
	 * wired and wired_count incremented, but not yet have its
	 * MAP_ENTRY_USER_WIRED flag set.  In which case, we would fail to call
	 * vm_fault_copy_entry() in the final loop below.
	 */
	if (in_tran != NULL) {
		in_tran->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
		vm_map_unlock_and_wait(map, 0);
		goto again;
	}

	/*
	 * Before changing the protections, try to reserve swap space for any
	 * private (i.e., copy-on-write) mappings that are transitioning from
	 * read-only to read/write access.  If a reservation fails, break out
	 * of this loop early and let the next loop simplify the entries, since
	 * some may now be mergeable.
	 */
	rv = vm_map_clip_start(map, first_entry, start);
	if (rv != KERN_SUCCESS) {
		vm_map_unlock(map);
		return (rv);
	}
	for (entry = first_entry; entry->start < end;
	    entry = vm_map_entry_succ(entry)) {
		rv = vm_map_clip_end(map, entry, end);
		if (rv != KERN_SUCCESS) {
			vm_map_unlock(map);
			return (rv);
		}

		if ((flags & VM_MAP_PROTECT_SET_PROT) == 0 ||
		    ((new_prot & ~entry->protection) & VM_PROT_WRITE) == 0 ||
		    ENTRY_CHARGED(entry) ||
		    (entry->eflags & MAP_ENTRY_GUARD) != 0)
			continue;

		cred = curthread->td_ucred;
		obj = entry->object.vm_object;

		if (obj == NULL ||
		    (entry->eflags & MAP_ENTRY_NEEDS_COPY) != 0) {
			if (!swap_reserve(entry->end - entry->start)) {
				rv = KERN_RESOURCE_SHORTAGE;
				end = entry->end;
				break;
			}
			crhold(cred);
			entry->cred = cred;
			continue;
		}

		if (obj->type != OBJT_DEFAULT &&
		    (obj->flags & OBJ_SWAP) == 0)
			continue;
		VM_OBJECT_WLOCK(obj);
		if (obj->type != OBJT_DEFAULT &&
		    (obj->flags & OBJ_SWAP) == 0) {
			VM_OBJECT_WUNLOCK(obj);
			continue;
		}

		/*
		 * Charge for the whole object allocation now, since
		 * we cannot distinguish between non-charged and
		 * charged clipped mapping of the same object later.
		 */
		KASSERT(obj->charge == 0,
		    ("vm_map_protect: object %p overcharged (entry %p)",
		    obj, entry));
		if (!swap_reserve(ptoa(obj->size))) {
			VM_OBJECT_WUNLOCK(obj);
			rv = KERN_RESOURCE_SHORTAGE;
			end = entry->end;
			break;
		}

		crhold(cred);
		obj->cred = cred;
		obj->charge = ptoa(obj->size);
		VM_OBJECT_WUNLOCK(obj);
	}

	/*
	 * If enough swap space was available, go back and fix up protections.
	 * Otherwise, just simplify entries, since some may have been modified.
	 * [Note that clipping is not necessary the second time.]
	 */
	for (prev_entry = vm_map_entry_pred(first_entry), entry = first_entry;
	    entry->start < end;
	    vm_map_try_merge_entries(map, prev_entry, entry),
	    prev_entry = entry, entry = vm_map_entry_succ(entry)) {
		if (rv != KERN_SUCCESS ||
		    (entry->eflags & MAP_ENTRY_GUARD) != 0)
			continue;

		old_prot = entry->protection;

		if ((flags & VM_MAP_PROTECT_SET_MAXPROT) != 0) {
			entry->max_protection = new_maxprot;
			entry->protection = new_maxprot & old_prot;
		}
		if ((flags & VM_MAP_PROTECT_SET_PROT) != 0)
			entry->protection = new_prot;

		/*
		 * For user wired map entries, the normal lazy evaluation of
		 * write access upgrades through soft page faults is
		 * undesirable.  Instead, immediately copy any pages that are
		 * copy-on-write and enable write access in the physical map.
		 */
		if ((entry->eflags & MAP_ENTRY_USER_WIRED) != 0 &&
		    (entry->protection & VM_PROT_WRITE) != 0 &&
		    (old_prot & VM_PROT_WRITE) == 0)
			vm_fault_copy_entry(map, map, entry, entry, NULL);

		/*
		 * When restricting access, update the physical map.  Worry
		 * about copy-on-write here.
		 */
		if ((old_prot & ~entry->protection) != 0) {
#define MASK(entry)	(((entry)->eflags & MAP_ENTRY_COW) ? ~VM_PROT_WRITE : \
							VM_PROT_ALL)
			pmap_protect(map->pmap, entry->start,
			    entry->end,
			    entry->protection & MASK(entry));
#undef	MASK
		}
	}
	vm_map_try_merge_entries(map, prev_entry, entry);
	vm_map_unlock(map);
	return (rv);
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
	vm_map_entry_t entry, prev_entry;
	int rv;
	bool modify_map;

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
			return (0);
		modify_map = true;
		vm_map_lock(map);
		break;
	case MADV_WILLNEED:
	case MADV_DONTNEED:
	case MADV_FREE:
		if (start == end)
			return (0);
		modify_map = false;
		vm_map_lock_read(map);
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Locate starting entry and clip if necessary.
	 */
	VM_MAP_RANGE_CHECK(map, start, end);

	if (modify_map) {
		/*
		 * madvise behaviors that are implemented in the vm_map_entry.
		 *
		 * We clip the vm_map_entry so that behavioral changes are
		 * limited to the specified address range.
		 */
		rv = vm_map_lookup_clip_start(map, start, &entry, &prev_entry);
		if (rv != KERN_SUCCESS) {
			vm_map_unlock(map);
			return (vm_mmap_to_errno(rv));
		}

		for (; entry->start < end; prev_entry = entry,
		    entry = vm_map_entry_succ(entry)) {
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0)
				continue;

			rv = vm_map_clip_end(map, entry, end);
			if (rv != KERN_SUCCESS) {
				vm_map_unlock(map);
				return (vm_mmap_to_errno(rv));
			}

			switch (behav) {
			case MADV_NORMAL:
				vm_map_entry_set_behavior(entry,
				    MAP_ENTRY_BEHAV_NORMAL);
				break;
			case MADV_SEQUENTIAL:
				vm_map_entry_set_behavior(entry,
				    MAP_ENTRY_BEHAV_SEQUENTIAL);
				break;
			case MADV_RANDOM:
				vm_map_entry_set_behavior(entry,
				    MAP_ENTRY_BEHAV_RANDOM);
				break;
			case MADV_NOSYNC:
				entry->eflags |= MAP_ENTRY_NOSYNC;
				break;
			case MADV_AUTOSYNC:
				entry->eflags &= ~MAP_ENTRY_NOSYNC;
				break;
			case MADV_NOCORE:
				entry->eflags |= MAP_ENTRY_NOCOREDUMP;
				break;
			case MADV_CORE:
				entry->eflags &= ~MAP_ENTRY_NOCOREDUMP;
				break;
			default:
				break;
			}
			vm_map_try_merge_entries(map, prev_entry, entry);
		}
		vm_map_try_merge_entries(map, prev_entry, entry);
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
		if (!vm_map_lookup_entry(map, start, &entry))
			entry = vm_map_entry_succ(entry);
		for (; entry->start < end;
		    entry = vm_map_entry_succ(entry)) {
			vm_offset_t useEnd, useStart;

			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0)
				continue;

			/*
			 * MADV_FREE would otherwise rewind time to
			 * the creation of the shadow object.  Because
			 * we hold the VM map read-locked, neither the
			 * entry's object nor the presence of a
			 * backing object can change.
			 */
			if (behav == MADV_FREE &&
			    entry->object.vm_object != NULL &&
			    entry->object.vm_object->backing_object != NULL)
				continue;

			pstart = OFF_TO_IDX(entry->offset);
			pend = pstart + atop(entry->end - entry->start);
			useStart = entry->start;
			useEnd = entry->end;

			if (entry->start < start) {
				pstart += atop(start - entry->start);
				useStart = start;
			}
			if (entry->end > end) {
				pend -= atop(entry->end - end);
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

			vm_object_madvise(entry->object.vm_object, pstart,
			    pend, behav);

			/*
			 * Pre-populate paging structures in the
			 * WILLNEED case.  For wired entries, the
			 * paging structures are already populated.
			 */
			if (behav == MADV_WILLNEED &&
			    entry->wired_count == 0) {
				vm_map_pmap_enter(map,
				    useStart,
				    entry->protection,
				    entry->object.vm_object,
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
	vm_map_entry_t entry, lentry, prev_entry, start_entry;
	int rv;

	switch (new_inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
	case VM_INHERIT_ZERO:
		break;
	default:
		return (KERN_INVALID_ARGUMENT);
	}
	if (start == end)
		return (KERN_SUCCESS);
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	rv = vm_map_lookup_clip_start(map, start, &start_entry, &prev_entry);
	if (rv != KERN_SUCCESS)
		goto unlock;
	if (vm_map_lookup_entry(map, end - 1, &lentry)) {
		rv = vm_map_clip_end(map, lentry, end);
		if (rv != KERN_SUCCESS)
			goto unlock;
	}
	if (new_inheritance == VM_INHERIT_COPY) {
		for (entry = start_entry; entry->start < end;
		    prev_entry = entry, entry = vm_map_entry_succ(entry)) {
			if ((entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK)
			    != 0) {
				rv = KERN_INVALID_ARGUMENT;
				goto unlock;
			}
		}
	}
	for (entry = start_entry; entry->start < end; prev_entry = entry,
	    entry = vm_map_entry_succ(entry)) {
		KASSERT(entry->end <= end, ("non-clipped entry %p end %jx %jx",
		    entry, (uintmax_t)entry->end, (uintmax_t)end));
		if ((entry->eflags & MAP_ENTRY_GUARD) == 0 ||
		    new_inheritance != VM_INHERIT_ZERO)
			entry->inheritance = new_inheritance;
		vm_map_try_merge_entries(map, prev_entry, entry);
	}
	vm_map_try_merge_entries(map, prev_entry, entry);
unlock:
	vm_map_unlock(map);
	return (rv);
}

/*
 *	vm_map_entry_in_transition:
 *
 *	Release the map lock, and sleep until the entry is no longer in
 *	transition.  Awake and acquire the map lock.  If the map changed while
 *	another held the lock, lookup a possibly-changed entry at or after the
 *	'start' position of the old entry.
 */
static vm_map_entry_t
vm_map_entry_in_transition(vm_map_t map, vm_offset_t in_start,
    vm_offset_t *io_end, bool holes_ok, vm_map_entry_t in_entry)
{
	vm_map_entry_t entry;
	vm_offset_t start;
	u_int last_timestamp;

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT((in_entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0,
	    ("not in-tranition map entry %p", in_entry));
	/*
	 * We have not yet clipped the entry.
	 */
	start = MAX(in_start, in_entry->start);
	in_entry->eflags |= MAP_ENTRY_NEEDS_WAKEUP;
	last_timestamp = map->timestamp;
	if (vm_map_unlock_and_wait(map, 0)) {
		/*
		 * Allow interruption of user wiring/unwiring?
		 */
	}
	vm_map_lock(map);
	if (last_timestamp + 1 == map->timestamp)
		return (in_entry);

	/*
	 * Look again for the entry because the map was modified while it was
	 * unlocked.  Specifically, the entry may have been clipped, merged, or
	 * deleted.
	 */
	if (!vm_map_lookup_entry(map, start, &entry)) {
		if (!holes_ok) {
			*io_end = start;
			return (NULL);
		}
		entry = vm_map_entry_succ(entry);
	}
	return (entry);
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
	vm_map_entry_t entry, first_entry, next_entry, prev_entry;
	int rv;
	bool holes_ok, need_wakeup, user_unwire;

	if (start == end)
		return (KERN_SUCCESS);
	holes_ok = (flags & VM_MAP_WIRE_HOLESOK) != 0;
	user_unwire = (flags & VM_MAP_WIRE_USER) != 0;
	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		if (holes_ok)
			first_entry = vm_map_entry_succ(first_entry);
		else {
			vm_map_unlock(map);
			return (KERN_INVALID_ADDRESS);
		}
	}
	rv = KERN_SUCCESS;
	for (entry = first_entry; entry->start < end; entry = next_entry) {
		if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
			/*
			 * We have not yet clipped the entry.
			 */
			next_entry = vm_map_entry_in_transition(map, start,
			    &end, holes_ok, entry);
			if (next_entry == NULL) {
				if (entry == first_entry) {
					vm_map_unlock(map);
					return (KERN_INVALID_ADDRESS);
				}
				rv = KERN_INVALID_ADDRESS;
				break;
			}
			first_entry = (entry == first_entry) ?
			    next_entry : NULL;
			continue;
		}
		rv = vm_map_clip_start(map, entry, start);
		if (rv != KERN_SUCCESS)
			break;
		rv = vm_map_clip_end(map, entry, end);
		if (rv != KERN_SUCCESS)
			break;

		/*
		 * Mark the entry in case the map lock is released.  (See
		 * above.)
		 */
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 &&
		    entry->wiring_thread == NULL,
		    ("owned map entry %p", entry));
		entry->eflags |= MAP_ENTRY_IN_TRANSITION;
		entry->wiring_thread = curthread;
		next_entry = vm_map_entry_succ(entry);
		/*
		 * Check the map for holes in the specified region.
		 * If holes_ok, skip this check.
		 */
		if (!holes_ok &&
		    entry->end < end && next_entry->start > entry->end) {
			end = entry->end;
			rv = KERN_INVALID_ADDRESS;
			break;
		}
		/*
		 * If system unwiring, require that the entry is system wired.
		 */
		if (!user_unwire &&
		    vm_map_entry_system_wired_count(entry) == 0) {
			end = entry->end;
			rv = KERN_INVALID_ARGUMENT;
			break;
		}
	}
	need_wakeup = false;
	if (first_entry == NULL &&
	    !vm_map_lookup_entry(map, start, &first_entry)) {
		KASSERT(holes_ok, ("vm_map_unwire: lookup failed"));
		prev_entry = first_entry;
		entry = vm_map_entry_succ(first_entry);
	} else {
		prev_entry = vm_map_entry_pred(first_entry);
		entry = first_entry;
	}
	for (; entry->start < end;
	    prev_entry = entry, entry = vm_map_entry_succ(entry)) {
		/*
		 * If holes_ok was specified, an empty
		 * space in the unwired region could have been mapped
		 * while the map lock was dropped for draining
		 * MAP_ENTRY_IN_TRANSITION.  Moreover, another thread
		 * could be simultaneously wiring this new mapping
		 * entry.  Detect these cases and skip any entries
		 * marked as in transition by us.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 ||
		    entry->wiring_thread != curthread) {
			KASSERT(holes_ok,
			    ("vm_map_unwire: !HOLESOK and new/changed entry"));
			continue;
		}

		if (rv == KERN_SUCCESS && (!user_unwire ||
		    (entry->eflags & MAP_ENTRY_USER_WIRED))) {
			if (entry->wired_count == 1)
				vm_map_entry_unwire(map, entry);
			else
				entry->wired_count--;
			if (user_unwire)
				entry->eflags &= ~MAP_ENTRY_USER_WIRED;
		}
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0,
		    ("vm_map_unwire: in-transition flag missing %p", entry));
		KASSERT(entry->wiring_thread == curthread,
		    ("vm_map_unwire: alien wire %p", entry));
		entry->eflags &= ~MAP_ENTRY_IN_TRANSITION;
		entry->wiring_thread = NULL;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = true;
		}
		vm_map_try_merge_entries(map, prev_entry, entry);
	}
	vm_map_try_merge_entries(map, prev_entry, entry);
	vm_map_unlock(map);
	if (need_wakeup)
		vm_map_wakeup(map);
	return (rv);
}

static void
vm_map_wire_user_count_sub(u_long npages)
{

	atomic_subtract_long(&vm_user_wire_count, npages);
}

static bool
vm_map_wire_user_count_add(u_long npages)
{
	u_long wired;

	wired = vm_user_wire_count;
	do {
		if (npages + wired > vm_page_max_user_wired)
			return (false);
	} while (!atomic_fcmpset_long(&vm_user_wire_count, &wired,
	    npages + wired));

	return (true);
}

/*
 *	vm_map_wire_entry_failure:
 *
 *	Handle a wiring failure on the given entry.
 *
 *	The map should be locked.
 */
static void
vm_map_wire_entry_failure(vm_map_t map, vm_map_entry_t entry,
    vm_offset_t failed_addr)
{

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0 &&
	    entry->wired_count == 1,
	    ("vm_map_wire_entry_failure: entry %p isn't being wired", entry));
	KASSERT(failed_addr < entry->end,
	    ("vm_map_wire_entry_failure: entry %p was fully wired", entry));

	/*
	 * If any pages at the start of this entry were successfully wired,
	 * then unwire them.
	 */
	if (failed_addr > entry->start) {
		pmap_unwire(map->pmap, entry->start, failed_addr);
		vm_object_unwire(entry->object.vm_object, entry->offset,
		    failed_addr - entry->start, PQ_ACTIVE);
	}

	/*
	 * Assign an out-of-range value to represent the failure to wire this
	 * entry.
	 */
	entry->wired_count = -1;
}

int
vm_map_wire(vm_map_t map, vm_offset_t start, vm_offset_t end, int flags)
{
	int rv;

	vm_map_lock(map);
	rv = vm_map_wire_locked(map, start, end, flags);
	vm_map_unlock(map);
	return (rv);
}

/*
 *	vm_map_wire_locked:
 *
 *	Implements both kernel and user wiring.  Returns with the map locked,
 *	the map lock may be dropped.
 */
int
vm_map_wire_locked(vm_map_t map, vm_offset_t start, vm_offset_t end, int flags)
{
	vm_map_entry_t entry, first_entry, next_entry, prev_entry;
	vm_offset_t faddr, saved_end, saved_start;
	u_long incr, npages;
	u_int bidx, last_timestamp;
	int rv;
	bool holes_ok, need_wakeup, user_wire;
	vm_prot_t prot;

	VM_MAP_ASSERT_LOCKED(map);

	if (start == end)
		return (KERN_SUCCESS);
	prot = 0;
	if (flags & VM_MAP_WIRE_WRITE)
		prot |= VM_PROT_WRITE;
	holes_ok = (flags & VM_MAP_WIRE_HOLESOK) != 0;
	user_wire = (flags & VM_MAP_WIRE_USER) != 0;
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		if (holes_ok)
			first_entry = vm_map_entry_succ(first_entry);
		else
			return (KERN_INVALID_ADDRESS);
	}
	for (entry = first_entry; entry->start < end; entry = next_entry) {
		if (entry->eflags & MAP_ENTRY_IN_TRANSITION) {
			/*
			 * We have not yet clipped the entry.
			 */
			next_entry = vm_map_entry_in_transition(map, start,
			    &end, holes_ok, entry);
			if (next_entry == NULL) {
				if (entry == first_entry)
					return (KERN_INVALID_ADDRESS);
				rv = KERN_INVALID_ADDRESS;
				goto done;
			}
			first_entry = (entry == first_entry) ?
			    next_entry : NULL;
			continue;
		}
		rv = vm_map_clip_start(map, entry, start);
		if (rv != KERN_SUCCESS)
			goto done;
		rv = vm_map_clip_end(map, entry, end);
		if (rv != KERN_SUCCESS)
			goto done;

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
			if (!holes_ok) {
				end = entry->end;
				rv = KERN_INVALID_ADDRESS;
				goto done;
			}
		} else if (entry->wired_count == 0) {
			entry->wired_count++;

			npages = atop(entry->end - entry->start);
			if (user_wire && !vm_map_wire_user_count_add(npages)) {
				vm_map_wire_entry_failure(map, entry,
				    entry->start);
				end = entry->end;
				rv = KERN_RESOURCE_SHORTAGE;
				goto done;
			}

			/*
			 * Release the map lock, relying on the in-transition
			 * mark.  Mark the map busy for fork.
			 */
			saved_start = entry->start;
			saved_end = entry->end;
			last_timestamp = map->timestamp;
			bidx = (entry->eflags & MAP_ENTRY_SPLIT_BOUNDARY_MASK)
			    >> MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
			incr =  pagesizes[bidx];
			vm_map_busy(map);
			vm_map_unlock(map);

			for (faddr = saved_start; faddr < saved_end;
			    faddr += incr) {
				/*
				 * Simulate a fault to get the page and enter
				 * it into the physical map.
				 */
				rv = vm_fault(map, faddr, VM_PROT_NONE,
				    VM_FAULT_WIRE, NULL);
				if (rv != KERN_SUCCESS)
					break;
			}
			vm_map_lock(map);
			vm_map_unbusy(map);
			if (last_timestamp + 1 != map->timestamp) {
				/*
				 * Look again for the entry because the map was
				 * modified while it was unlocked.  The entry
				 * may have been clipped, but NOT merged or
				 * deleted.
				 */
				if (!vm_map_lookup_entry(map, saved_start,
				    &next_entry))
					KASSERT(false,
					    ("vm_map_wire: lookup failed"));
				first_entry = (entry == first_entry) ?
				    next_entry : NULL;
				for (entry = next_entry; entry->end < saved_end;
				    entry = vm_map_entry_succ(entry)) {
					/*
					 * In case of failure, handle entries
					 * that were not fully wired here;
					 * fully wired entries are handled
					 * later.
					 */
					if (rv != KERN_SUCCESS &&
					    faddr < entry->end)
						vm_map_wire_entry_failure(map,
						    entry, faddr);
				}
			}
			if (rv != KERN_SUCCESS) {
				vm_map_wire_entry_failure(map, entry, faddr);
				if (user_wire)
					vm_map_wire_user_count_sub(npages);
				end = entry->end;
				goto done;
			}
		} else if (!user_wire ||
			   (entry->eflags & MAP_ENTRY_USER_WIRED) == 0) {
			entry->wired_count++;
		}
		/*
		 * Check the map for holes in the specified region.
		 * If holes_ok was specified, skip this check.
		 */
		next_entry = vm_map_entry_succ(entry);
		if (!holes_ok &&
		    entry->end < end && next_entry->start > entry->end) {
			end = entry->end;
			rv = KERN_INVALID_ADDRESS;
			goto done;
		}
	}
	rv = KERN_SUCCESS;
done:
	need_wakeup = false;
	if (first_entry == NULL &&
	    !vm_map_lookup_entry(map, start, &first_entry)) {
		KASSERT(holes_ok, ("vm_map_wire: lookup failed"));
		prev_entry = first_entry;
		entry = vm_map_entry_succ(first_entry);
	} else {
		prev_entry = vm_map_entry_pred(first_entry);
		entry = first_entry;
	}
	for (; entry->start < end;
	    prev_entry = entry, entry = vm_map_entry_succ(entry)) {
		/*
		 * If holes_ok was specified, an empty
		 * space in the unwired region could have been mapped
		 * while the map lock was dropped for faulting in the
		 * pages or draining MAP_ENTRY_IN_TRANSITION.
		 * Moreover, another thread could be simultaneously
		 * wiring this new mapping entry.  Detect these cases
		 * and skip any entries marked as in transition not by us.
		 *
		 * Another way to get an entry not marked with
		 * MAP_ENTRY_IN_TRANSITION is after failed clipping,
		 * which set rv to KERN_INVALID_ARGUMENT.
		 */
		if ((entry->eflags & MAP_ENTRY_IN_TRANSITION) == 0 ||
		    entry->wiring_thread != curthread) {
			KASSERT(holes_ok || rv == KERN_INVALID_ARGUMENT,
			    ("vm_map_wire: !HOLESOK and new/changed entry"));
			continue;
		}

		if ((entry->eflags & MAP_ENTRY_WIRE_SKIPPED) != 0) {
			/* do nothing */
		} else if (rv == KERN_SUCCESS) {
			if (user_wire)
				entry->eflags |= MAP_ENTRY_USER_WIRED;
		} else if (entry->wired_count == -1) {
			/*
			 * Wiring failed on this entry.  Thus, unwiring is
			 * unnecessary.
			 */
			entry->wired_count = 0;
		} else if (!user_wire ||
		    (entry->eflags & MAP_ENTRY_USER_WIRED) == 0) {
			/*
			 * Undo the wiring.  Wiring succeeded on this entry
			 * but failed on a later entry.  
			 */
			if (entry->wired_count == 1) {
				vm_map_entry_unwire(map, entry);
				if (user_wire)
					vm_map_wire_user_count_sub(
					    atop(entry->end - entry->start));
			} else
				entry->wired_count--;
		}
		KASSERT((entry->eflags & MAP_ENTRY_IN_TRANSITION) != 0,
		    ("vm_map_wire: in-transition flag missing %p", entry));
		KASSERT(entry->wiring_thread == curthread,
		    ("vm_map_wire: alien wire %p", entry));
		entry->eflags &= ~(MAP_ENTRY_IN_TRANSITION |
		    MAP_ENTRY_WIRE_SKIPPED);
		entry->wiring_thread = NULL;
		if (entry->eflags & MAP_ENTRY_NEEDS_WAKEUP) {
			entry->eflags &= ~MAP_ENTRY_NEEDS_WAKEUP;
			need_wakeup = true;
		}
		vm_map_try_merge_entries(map, prev_entry, entry);
	}
	vm_map_try_merge_entries(map, prev_entry, entry);
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
	vm_map_entry_t entry, first_entry, next_entry;
	vm_size_t size;
	vm_object_t object;
	vm_ooffset_t offset;
	unsigned int last_timestamp;
	int bdry_idx;
	boolean_t failed;

	vm_map_lock_read(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!vm_map_lookup_entry(map, start, &first_entry)) {
		vm_map_unlock_read(map);
		return (KERN_INVALID_ADDRESS);
	} else if (start == end) {
		start = first_entry->start;
		end = first_entry->end;
	}

	/*
	 * Make a first pass to check for user-wired memory, holes,
	 * and partial invalidation of largepage mappings.
	 */
	for (entry = first_entry; entry->start < end; entry = next_entry) {
		if (invalidate) {
			if ((entry->eflags & MAP_ENTRY_USER_WIRED) != 0) {
				vm_map_unlock_read(map);
				return (KERN_INVALID_ARGUMENT);
			}
			bdry_idx = (entry->eflags &
			    MAP_ENTRY_SPLIT_BOUNDARY_MASK) >>
			    MAP_ENTRY_SPLIT_BOUNDARY_SHIFT;
			if (bdry_idx != 0 &&
			    ((start & (pagesizes[bdry_idx] - 1)) != 0 ||
			    (end & (pagesizes[bdry_idx] - 1)) != 0)) {
				vm_map_unlock_read(map);
				return (KERN_INVALID_ARGUMENT);
			}
		}
		next_entry = vm_map_entry_succ(entry);
		if (end > entry->end &&
		    entry->end != next_entry->start) {
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
	for (entry = first_entry; entry->start < end;) {
		offset = entry->offset + (start - entry->start);
		size = (end <= entry->end ? end : entry->end) - start;
		if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0) {
			vm_map_t smap;
			vm_map_entry_t tentry;
			vm_size_t tsize;

			smap = entry->object.sub_map;
			vm_map_lock_read(smap);
			(void) vm_map_lookup_entry(smap, offset, &tentry);
			tsize = tentry->end - offset;
			if (tsize < size)
				size = tsize;
			object = tentry->object.vm_object;
			offset = tentry->offset + (offset - tentry->start);
			vm_map_unlock_read(smap);
		} else {
			object = entry->object.vm_object;
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
		    !vm_map_lookup_entry(map, start, &entry))
			entry = vm_map_entry_succ(entry);
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
	vm_size_t size;

	VM_MAP_ASSERT_LOCKED(map);
	KASSERT(entry->wired_count > 0,
	    ("vm_map_entry_unwire: entry %p isn't wired", entry));

	size = entry->end - entry->start;
	if ((entry->eflags & MAP_ENTRY_USER_WIRED) != 0)
		vm_map_wire_user_count_sub(atop(size));
	pmap_unwire(map->pmap, entry->start, entry->end);
	vm_object_unwire(entry->object.vm_object, entry->offset, size,
	    PQ_ACTIVE);
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
	vm_pindex_t offidxstart, offidxend, size1;
	vm_size_t size;

	vm_map_entry_unlink(map, entry, UNLINK_MERGE_NONE);
	object = entry->object.vm_object;

	if ((entry->eflags & MAP_ENTRY_GUARD) != 0) {
		MPASS(entry->cred == NULL);
		MPASS((entry->eflags & MAP_ENTRY_IS_SUB_MAP) == 0);
		MPASS(object == NULL);
		vm_map_entry_deallocate(entry, map->system_map);
		return;
	}

	size = entry->end - entry->start;
	map->size -= size;

	if (entry->cred != NULL) {
		swap_release_by_cred(size, entry->cred);
		crfree(entry->cred);
	}

	if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0 || object == NULL) {
		entry->object.vm_object = NULL;
	} else if ((object->flags & OBJ_ANON) != 0 ||
	    object == kernel_object) {
		KASSERT(entry->cred == NULL || object->cred == NULL ||
		    (entry->eflags & MAP_ENTRY_NEEDS_COPY),
		    ("OVERCOMMIT vm_map_entry_delete: both cred %p", entry));
		offidxstart = OFF_TO_IDX(entry->offset);
		offidxend = offidxstart + atop(size);
		VM_OBJECT_WLOCK(object);
		if (object->ref_count != 1 &&
		    ((object->flags & OBJ_ONEMAPPING) != 0 ||
		    object == kernel_object)) {
			vm_object_collapse(object);

			/*
			 * The option OBJPR_NOTMAPPED can be passed here
			 * because vm_map_delete() already performed
			 * pmap_remove() on the only mapping to this range
			 * of pages. 
			 */
			vm_object_page_remove(object, offidxstart, offidxend,
			    OBJPR_NOTMAPPED);
			if (offidxend >= object->size &&
			    offidxstart < object->size) {
				size1 = object->size;
				object->size = offidxstart;
				if (object->cred != NULL) {
					size1 -= object->size;
					KASSERT(object->charge >= ptoa(size1),
					    ("object %p charge < 0", object));
					swap_release_by_cred(ptoa(size1),
					    object->cred);
					object->charge -= ptoa(size1);
				}
			}
		}
		VM_OBJECT_WUNLOCK(object);
	}
	if (map->system_map)
		vm_map_entry_deallocate(entry, TRUE);
	else {
		entry->defer_next = curthread->td_map_def_user;
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
	vm_map_entry_t entry, next_entry, scratch_entry;
	int rv;

	VM_MAP_ASSERT_LOCKED(map);

	if (start == end)
		return (KERN_SUCCESS);

	/*
	 * Find the start of the region, and clip it.
	 * Step through all entries in this region.
	 */
	rv = vm_map_lookup_clip_start(map, start, &entry, &scratch_entry);
	if (rv != KERN_SUCCESS)
		return (rv);
	for (; entry->start < end; entry = next_entry) {
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
				rv = vm_map_lookup_clip_start(map, saved_start,
				    &next_entry, &scratch_entry);
				if (rv != KERN_SUCCESS)
					break;
			} else
				next_entry = entry;
			continue;
		}

		/* XXXKIB or delete to the upper superpage boundary ? */
		rv = vm_map_clip_end(map, entry, end);
		if (rv != KERN_SUCCESS)
			break;
		next_entry = vm_map_entry_succ(entry);

		/*
		 * Unwire before removing addresses from the pmap; otherwise,
		 * unwiring will put the entries back in the pmap.
		 */
		if (entry->wired_count != 0)
			vm_map_entry_unwire(map, entry);

		/*
		 * Remove mappings for the pages, but only if the
		 * mappings could exist.  For instance, it does not
		 * make sense to call pmap_remove() for guard entries.
		 */
		if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0 ||
		    entry->object.vm_object != NULL)
			pmap_remove(map->pmap, entry->start, entry->end);

		if (entry->end == map->anon_loc)
			map->anon_loc = entry->start;

		/*
		 * Delete the entry only after removing all pmap
		 * entries pointing to its pages.  (Otherwise, its
		 * page frames may be reallocated, and any modify bits
		 * will be set in the wrong object!)
		 */
		vm_map_entry_delete(map, entry);
	}
	return (rv);
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
		entry = vm_map_entry_succ(entry);
	}
	return (TRUE);
}

/*
 *
 *	vm_map_copy_swap_object:
 *
 *	Copies a swap-backed object from an existing map entry to a
 *	new one.  Carries forward the swap charge.  May change the
 *	src object on return.
 */
static void
vm_map_copy_swap_object(vm_map_entry_t src_entry, vm_map_entry_t dst_entry,
    vm_offset_t size, vm_ooffset_t *fork_charge)
{
	vm_object_t src_object;
	struct ucred *cred;
	int charged;

	src_object = src_entry->object.vm_object;
	charged = ENTRY_CHARGED(src_entry);
	if ((src_object->flags & OBJ_ANON) != 0) {
		VM_OBJECT_WLOCK(src_object);
		vm_object_collapse(src_object);
		if ((src_object->flags & OBJ_ONEMAPPING) != 0) {
			vm_object_split(src_entry);
			src_object = src_entry->object.vm_object;
		}
		vm_object_reference_locked(src_object);
		vm_object_clear_flag(src_object, OBJ_ONEMAPPING);
		VM_OBJECT_WUNLOCK(src_object);
	} else
		vm_object_reference(src_object);
	if (src_entry->cred != NULL &&
	    !(src_entry->eflags & MAP_ENTRY_NEEDS_COPY)) {
		KASSERT(src_object->cred == NULL,
		    ("OVERCOMMIT: vm_map_copy_anon_entry: cred %p",
		     src_object));
		src_object->cred = src_entry->cred;
		src_object->charge = size;
	}
	dst_entry->object.vm_object = src_object;
	if (charged) {
		cred = curthread->td_ucred;
		crhold(cred);
		dst_entry->cred = cred;
		*fork_charge += size;
		if (!(src_entry->eflags & MAP_ENTRY_NEEDS_COPY)) {
			crhold(cred);
			src_entry->cred = cred;
			*fork_charge += size;
		}
	}
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

	VM_MAP_ASSERT_LOCKED(dst_map);

	if ((dst_entry->eflags|src_entry->eflags) & MAP_ENTRY_IS_SUB_MAP)
		return;

	if (src_entry->wired_count == 0 ||
	    (src_entry->protection & VM_PROT_WRITE) == 0) {
		/*
		 * If the source entry is marked needs_copy, it is already
		 * write-protected.
		 */
		if ((src_entry->eflags & MAP_ENTRY_NEEDS_COPY) == 0 &&
		    (src_entry->protection & VM_PROT_WRITE) != 0) {
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
			if (src_object->type == OBJT_DEFAULT ||
			    (src_object->flags & OBJ_SWAP) != 0) {
				vm_map_copy_swap_object(src_entry, dst_entry,
				    size, fork_charge);
				/* May have split/collapsed, reload obj. */
				src_object = src_entry->object.vm_object;
			} else {
				vm_object_reference(src_object);
				dst_entry->object.vm_object = src_object;
			}
			src_entry->eflags |= MAP_ENTRY_COW |
			    MAP_ENTRY_NEEDS_COPY;
			dst_entry->eflags |= MAP_ENTRY_COW |
			    MAP_ENTRY_NEEDS_COPY;
			dst_entry->offset = src_entry->offset;
			if (src_entry->eflags & MAP_ENTRY_WRITECNT) {
				/*
				 * MAP_ENTRY_WRITECNT cannot
				 * indicate write reference from
				 * src_entry, since the entry is
				 * marked as needs copy.  Allocate a
				 * fake entry that is used to
				 * decrement object->un_pager writecount
				 * at the appropriate time.  Attach
				 * fake_entry to the deferred list.
				 */
				fake_entry = vm_map_entry_create(dst_map);
				fake_entry->eflags = MAP_ENTRY_WRITECNT;
				src_entry->eflags &= ~MAP_ENTRY_WRITECNT;
				vm_object_reference(src_object);
				fake_entry->object.vm_object = src_object;
				fake_entry->start = src_entry->start;
				fake_entry->end = src_entry->end;
				fake_entry->defer_next =
				    curthread->td_map_def_user;
				curthread->td_map_def_user = fake_entry;
			}

			pmap_copy(dst_map->pmap, src_map->pmap,
			    dst_entry->start, dst_entry->end - dst_entry->start,
			    src_entry->start);
		} else {
			dst_entry->object.vm_object = NULL;
			dst_entry->offset = 0;
			if (src_entry->cred != NULL) {
				dst_entry->cred = curthread->td_ucred;
				crhold(dst_entry->cred);
				*fork_charge += size;
			}
		}
	} else {
		/*
		 * We don't want to make writeable wired pages copy-on-write.
		 * Immediately copy these pages into the new map by simulating
		 * page faults.  The new pages are pageable.
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

	if ((entry->eflags & MAP_ENTRY_GUARD) != 0)
		return;
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
	int error, locked;
	vm_inherit_t inh;

	old_map = &vm1->vm_map;
	/* Copy immutable fields of vm1 to vm2. */
	vm2 = vmspace_alloc(vm_map_min(old_map), vm_map_max(old_map),
	    pmap_pinit);
	if (vm2 == NULL)
		return (NULL);

	vm2->vm_taddr = vm1->vm_taddr;
	vm2->vm_daddr = vm1->vm_daddr;
	vm2->vm_maxsaddr = vm1->vm_maxsaddr;
	vm2->vm_stacktop = vm1->vm_stacktop;
	vm_map_lock(old_map);
	if (old_map->busy)
		vm_map_wait_busy(old_map);
	new_map = &vm2->vm_map;
	locked = vm_map_trylock(new_map); /* trylock to silence WITNESS */
	KASSERT(locked, ("vmspace_fork: lock failed"));

	error = pmap_vmspace_copy(new_map->pmap, old_map->pmap);
	if (error != 0) {
		sx_xunlock(&old_map->lock);
		sx_xunlock(&new_map->lock);
		vm_map_process_deferred();
		vmspace_free(vm2);
		return (NULL);
	}

	new_map->anon_loc = old_map->anon_loc;
	new_map->flags |= old_map->flags & (MAP_ASLR | MAP_ASLR_IGNSTART |
	    MAP_ASLR_STACK | MAP_WXORX);

	VM_MAP_ENTRY_FOREACH(old_entry, old_map) {
		if ((old_entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0)
			panic("vm_map_fork: encountered a submap");

		inh = old_entry->inheritance;
		if ((old_entry->eflags & MAP_ENTRY_GUARD) != 0 &&
		    inh != VM_INHERIT_NONE)
			inh = VM_INHERIT_COPY;

		switch (inh) {
		case VM_INHERIT_NONE:
			break;

		case VM_INHERIT_SHARE:
			/*
			 * Clone the entry, creating the shared object if
			 * necessary.
			 */
			object = old_entry->object.vm_object;
			if (object == NULL) {
				vm_map_entry_back(old_entry);
				object = old_entry->object.vm_object;
			}

			/*
			 * Add the reference before calling vm_object_shadow
			 * to insure that a shadow object is created.
			 */
			vm_object_reference(object);
			if (old_entry->eflags & MAP_ENTRY_NEEDS_COPY) {
				vm_object_shadow(&old_entry->object.vm_object,
				    &old_entry->offset,
				    old_entry->end - old_entry->start,
				    old_entry->cred,
				    /* Transfer the second reference too. */
				    true);
				old_entry->eflags &= ~MAP_ENTRY_NEEDS_COPY;
				old_entry->cred = NULL;

				/*
				 * As in vm_map_merged_neighbor_dispose(),
				 * the vnode lock will not be acquired in
				 * this call to vm_object_deallocate().
				 */
				vm_object_deallocate(object);
				object = old_entry->object.vm_object;
			} else {
				VM_OBJECT_WLOCK(object);
				vm_object_clear_flag(object, OBJ_ONEMAPPING);
				if (old_entry->cred != NULL) {
					KASSERT(object->cred == NULL,
					    ("vmspace_fork both cred"));
					object->cred = old_entry->cred;
					object->charge = old_entry->end -
					    old_entry->start;
					old_entry->cred = NULL;
				}

				/*
				 * Assert the correct state of the vnode
				 * v_writecount while the object is locked, to
				 * not relock it later for the assertion
				 * correctness.
				 */
				if (old_entry->eflags & MAP_ENTRY_WRITECNT &&
				    object->type == OBJT_VNODE) {
					KASSERT(((struct vnode *)object->
					    handle)->v_writecount > 0,
					    ("vmspace_fork: v_writecount %p",
					    object));
					KASSERT(object->un_pager.vnp.
					    writemappings > 0,
					    ("vmspace_fork: vnp.writecount %p",
					    object));
				}
				VM_OBJECT_WUNLOCK(object);
			}

			/*
			 * Clone the entry, referencing the shared object.
			 */
			new_entry = vm_map_entry_create(new_map);
			*new_entry = *old_entry;
			new_entry->eflags &= ~(MAP_ENTRY_USER_WIRED |
			    MAP_ENTRY_IN_TRANSITION);
			new_entry->wiring_thread = NULL;
			new_entry->wired_count = 0;
			if (new_entry->eflags & MAP_ENTRY_WRITECNT) {
				vm_pager_update_writecount(object,
				    new_entry->start, new_entry->end);
			}
			vm_map_entry_set_vnode_text(new_entry, true);

			/*
			 * Insert the entry into the new map -- we know we're
			 * inserting at the end of the new map.
			 */
			vm_map_entry_link(new_map, new_entry);
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
			    MAP_ENTRY_IN_TRANSITION | MAP_ENTRY_WRITECNT);
			new_entry->wiring_thread = NULL;
			new_entry->wired_count = 0;
			new_entry->object.vm_object = NULL;
			new_entry->cred = NULL;
			vm_map_entry_link(new_map, new_entry);
			vmspace_map_entry_forked(vm1, vm2, new_entry);
			vm_map_copy_entry(old_map, new_map, old_entry,
			    new_entry, fork_charge);
			vm_map_entry_set_vnode_text(new_entry, true);
			break;

		case VM_INHERIT_ZERO:
			/*
			 * Create a new anonymous mapping entry modelled from
			 * the old one.
			 */
			new_entry = vm_map_entry_create(new_map);
			memset(new_entry, 0, sizeof(*new_entry));

			new_entry->start = old_entry->start;
			new_entry->end = old_entry->end;
			new_entry->eflags = old_entry->eflags &
			    ~(MAP_ENTRY_USER_WIRED | MAP_ENTRY_IN_TRANSITION |
			    MAP_ENTRY_WRITECNT | MAP_ENTRY_VN_EXEC |
			    MAP_ENTRY_SPLIT_BOUNDARY_MASK);
			new_entry->protection = old_entry->protection;
			new_entry->max_protection = old_entry->max_protection;
			new_entry->inheritance = VM_INHERIT_ZERO;

			vm_map_entry_link(new_map, new_entry);
			vmspace_map_entry_forked(vm1, vm2, new_entry);

			new_entry->cred = curthread->td_ucred;
			crhold(new_entry->cred);
			*fork_charge += (new_entry->end - new_entry->start);

			break;
		}
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

/*
 * Create a process's stack for exec_new_vmspace().  This function is never
 * asked to wire the newly created stack.
 */
int
vm_map_stack(vm_map_t map, vm_offset_t addrbos, vm_size_t max_ssize,
    vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_size_t growsize, init_ssize;
	rlim_t vmemlim;
	int rv;

	MPASS((map->flags & MAP_WIREFUTURE) == 0);
	growsize = sgrowsiz;
	init_ssize = (max_ssize < growsize) ? max_ssize : growsize;
	vm_map_lock(map);
	vmemlim = lim_cur(curthread, RLIMIT_VMEM);
	/* If we would blow our VMEM resource limit, no go */
	if (map->size + init_ssize > vmemlim) {
		rv = KERN_NO_SPACE;
		goto out;
	}
	rv = vm_map_stack_locked(map, addrbos, max_ssize, growsize, prot,
	    max, cow);
out:
	vm_map_unlock(map);
	return (rv);
}

static int stack_guard_page = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, stack_guard_page, CTLFLAG_RWTUN,
    &stack_guard_page, 0,
    "Specifies the number of guard pages for a stack that grows");

static int
vm_map_stack_locked(vm_map_t map, vm_offset_t addrbos, vm_size_t max_ssize,
    vm_size_t growsize, vm_prot_t prot, vm_prot_t max, int cow)
{
	vm_map_entry_t new_entry, prev_entry;
	vm_offset_t bot, gap_bot, gap_top, top;
	vm_size_t init_ssize, sgp;
	int orient, rv;

	/*
	 * The stack orientation is piggybacked with the cow argument.
	 * Extract it into orient and mask the cow argument so that we
	 * don't pass it around further.
	 */
	orient = cow & (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP);
	KASSERT(orient != 0, ("No stack grow direction"));
	KASSERT(orient != (MAP_STACK_GROWS_DOWN | MAP_STACK_GROWS_UP),
	    ("bi-dir stack"));

	if (max_ssize == 0 ||
	    !vm_map_range_valid(map, addrbos, addrbos + max_ssize))
		return (KERN_INVALID_ADDRESS);
	sgp = ((curproc->p_flag2 & P2_STKGAP_DISABLE) != 0 ||
	    (curproc->p_fctl0 & NT_FREEBSD_FCTL_STKGAP_DISABLE) != 0) ? 0 :
	    (vm_size_t)stack_guard_page * PAGE_SIZE;
	if (sgp >= max_ssize)
		return (KERN_INVALID_ARGUMENT);

	init_ssize = growsize;
	if (max_ssize < init_ssize + sgp)
		init_ssize = max_ssize - sgp;

	/* If addr is already mapped, no go */
	if (vm_map_lookup_entry(map, addrbos, &prev_entry))
		return (KERN_NO_SPACE);

	/*
	 * If we can't accommodate max_ssize in the current mapping, no go.
	 */
	if (vm_map_entry_succ(prev_entry)->start < addrbos + max_ssize)
		return (KERN_NO_SPACE);

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
	if (orient == MAP_STACK_GROWS_DOWN) {
		bot = addrbos + max_ssize - init_ssize;
		top = bot + init_ssize;
		gap_bot = addrbos;
		gap_top = bot;
	} else /* if (orient == MAP_STACK_GROWS_UP) */ {
		bot = addrbos;
		top = bot + init_ssize;
		gap_bot = top;
		gap_top = addrbos + max_ssize;
	}
	rv = vm_map_insert(map, NULL, 0, bot, top, prot, max, cow);
	if (rv != KERN_SUCCESS)
		return (rv);
	new_entry = vm_map_entry_succ(prev_entry);
	KASSERT(new_entry->end == top || new_entry->start == bot,
	    ("Bad entry start/end for new stack entry"));
	KASSERT((orient & MAP_STACK_GROWS_DOWN) == 0 ||
	    (new_entry->eflags & MAP_ENTRY_GROWS_DOWN) != 0,
	    ("new entry lacks MAP_ENTRY_GROWS_DOWN"));
	KASSERT((orient & MAP_STACK_GROWS_UP) == 0 ||
	    (new_entry->eflags & MAP_ENTRY_GROWS_UP) != 0,
	    ("new entry lacks MAP_ENTRY_GROWS_UP"));
	if (gap_bot == gap_top)
		return (KERN_SUCCESS);
	rv = vm_map_insert(map, NULL, 0, gap_bot, gap_top, VM_PROT_NONE,
	    VM_PROT_NONE, MAP_CREATE_GUARD | (orient == MAP_STACK_GROWS_DOWN ?
	    MAP_CREATE_STACK_GAP_DN : MAP_CREATE_STACK_GAP_UP));
	if (rv == KERN_SUCCESS) {
		/*
		 * Gap can never successfully handle a fault, so
		 * read-ahead logic is never used for it.  Re-use
		 * next_read of the gap entry to store
		 * stack_guard_page for vm_map_growstack().
		 */
		if (orient == MAP_STACK_GROWS_DOWN)
			vm_map_entry_pred(new_entry)->next_read = sgp;
		else
			vm_map_entry_succ(new_entry)->next_read = sgp;
	} else {
		(void)vm_map_delete(map, bot, top);
	}
	return (rv);
}

/*
 * Attempts to grow a vm stack entry.  Returns KERN_SUCCESS if we
 * successfully grow the stack.
 */
static int
vm_map_growstack(vm_map_t map, vm_offset_t addr, vm_map_entry_t gap_entry)
{
	vm_map_entry_t stack_entry;
	struct proc *p;
	struct vmspace *vm;
	struct ucred *cred;
	vm_offset_t gap_end, gap_start, grow_start;
	vm_size_t grow_amount, guard, max_grow;
	rlim_t lmemlim, stacklim, vmemlim;
	int rv, rv1;
	bool gap_deleted, grow_down, is_procstack;
#ifdef notyet
	uint64_t limit;
#endif
#ifdef RACCT
	int error;
#endif

	p = curproc;
	vm = p->p_vmspace;

	/*
	 * Disallow stack growth when the access is performed by a
	 * debugger or AIO daemon.  The reason is that the wrong
	 * resource limits are applied.
	 */
	if (p != initproc && (map != &p->p_vmspace->vm_map ||
	    p->p_textvp == NULL))
		return (KERN_FAILURE);

	MPASS(!map->system_map);

	lmemlim = lim_cur(curthread, RLIMIT_MEMLOCK);
	stacklim = lim_cur(curthread, RLIMIT_STACK);
	vmemlim = lim_cur(curthread, RLIMIT_VMEM);
retry:
	/* If addr is not in a hole for a stack grow area, no need to grow. */
	if (gap_entry == NULL && !vm_map_lookup_entry(map, addr, &gap_entry))
		return (KERN_FAILURE);
	if ((gap_entry->eflags & MAP_ENTRY_GUARD) == 0)
		return (KERN_SUCCESS);
	if ((gap_entry->eflags & MAP_ENTRY_STACK_GAP_DN) != 0) {
		stack_entry = vm_map_entry_succ(gap_entry);
		if ((stack_entry->eflags & MAP_ENTRY_GROWS_DOWN) == 0 ||
		    stack_entry->start != gap_entry->end)
			return (KERN_FAILURE);
		grow_amount = round_page(stack_entry->start - addr);
		grow_down = true;
	} else if ((gap_entry->eflags & MAP_ENTRY_STACK_GAP_UP) != 0) {
		stack_entry = vm_map_entry_pred(gap_entry);
		if ((stack_entry->eflags & MAP_ENTRY_GROWS_UP) == 0 ||
		    stack_entry->end != gap_entry->start)
			return (KERN_FAILURE);
		grow_amount = round_page(addr + 1 - stack_entry->end);
		grow_down = false;
	} else {
		return (KERN_FAILURE);
	}
	guard = ((curproc->p_flag2 & P2_STKGAP_DISABLE) != 0 ||
	    (curproc->p_fctl0 & NT_FREEBSD_FCTL_STKGAP_DISABLE) != 0) ? 0 :
	    gap_entry->next_read;
	max_grow = gap_entry->end - gap_entry->start;
	if (guard > max_grow)
		return (KERN_NO_SPACE);
	max_grow -= guard;
	if (grow_amount > max_grow)
		return (KERN_NO_SPACE);

	/*
	 * If this is the main process stack, see if we're over the stack
	 * limit.
	 */
	is_procstack = addr >= (vm_offset_t)vm->vm_maxsaddr &&
	    addr < (vm_offset_t)p->p_sysent->sv_usrstack;
	if (is_procstack && (ctob(vm->vm_ssize) + grow_amount > stacklim))
		return (KERN_NO_SPACE);

#ifdef RACCT
	if (racct_enable) {
		PROC_LOCK(p);
		if (is_procstack && racct_set(p, RACCT_STACK,
		    ctob(vm->vm_ssize) + grow_amount)) {
			PROC_UNLOCK(p);
			return (KERN_NO_SPACE);
		}
		PROC_UNLOCK(p);
	}
#endif

	grow_amount = roundup(grow_amount, sgrowsiz);
	if (grow_amount > max_grow)
		grow_amount = max_grow;
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

	if (!old_mlock && (map->flags & MAP_WIREFUTURE) != 0) {
		if (ptoa(pmap_wired_count(map->pmap)) + grow_amount > lmemlim) {
			rv = KERN_NO_SPACE;
			goto out;
		}
#ifdef RACCT
		if (racct_enable) {
			PROC_LOCK(p);
			if (racct_set(p, RACCT_MEMLOCK,
			    ptoa(pmap_wired_count(map->pmap)) + grow_amount)) {
				PROC_UNLOCK(p);
				rv = KERN_NO_SPACE;
				goto out;
			}
			PROC_UNLOCK(p);
		}
#endif
	}

	/* If we would blow our VMEM resource limit, no go */
	if (map->size + grow_amount > vmemlim) {
		rv = KERN_NO_SPACE;
		goto out;
	}
#ifdef RACCT
	if (racct_enable) {
		PROC_LOCK(p);
		if (racct_set(p, RACCT_VMEM, map->size + grow_amount)) {
			PROC_UNLOCK(p);
			rv = KERN_NO_SPACE;
			goto out;
		}
		PROC_UNLOCK(p);
	}
#endif

	if (vm_map_lock_upgrade(map)) {
		gap_entry = NULL;
		vm_map_lock_read(map);
		goto retry;
	}

	if (grow_down) {
		grow_start = gap_entry->end - grow_amount;
		if (gap_entry->start + grow_amount == gap_entry->end) {
			gap_start = gap_entry->start;
			gap_end = gap_entry->end;
			vm_map_entry_delete(map, gap_entry);
			gap_deleted = true;
		} else {
			MPASS(gap_entry->start < gap_entry->end - grow_amount);
			vm_map_entry_resize(map, gap_entry, -grow_amount);
			gap_deleted = false;
		}
		rv = vm_map_insert(map, NULL, 0, grow_start,
		    grow_start + grow_amount,
		    stack_entry->protection, stack_entry->max_protection,
		    MAP_STACK_GROWS_DOWN);
		if (rv != KERN_SUCCESS) {
			if (gap_deleted) {
				rv1 = vm_map_insert(map, NULL, 0, gap_start,
				    gap_end, VM_PROT_NONE, VM_PROT_NONE,
				    MAP_CREATE_GUARD | MAP_CREATE_STACK_GAP_DN);
				MPASS(rv1 == KERN_SUCCESS);
			} else
				vm_map_entry_resize(map, gap_entry,
				    grow_amount);
		}
	} else {
		grow_start = stack_entry->end;
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
		    grow_amount, cred != NULL)) {
			if (gap_entry->start + grow_amount == gap_entry->end) {
				vm_map_entry_delete(map, gap_entry);
				vm_map_entry_resize(map, stack_entry,
				    grow_amount);
			} else {
				gap_entry->start += grow_amount;
				stack_entry->end += grow_amount;
			}
			map->size += grow_amount;
			rv = KERN_SUCCESS;
		} else
			rv = KERN_FAILURE;
	}
	if (rv == KERN_SUCCESS && is_procstack)
		vm->vm_ssize += btoc(grow_amount);

	/*
	 * Heed the MAP_WIREFUTURE flag if it was set for this process.
	 */
	if (rv == KERN_SUCCESS && (map->flags & MAP_WIREFUTURE) != 0) {
		rv = vm_map_wire_locked(map, grow_start,
		    grow_start + grow_amount,
		    VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
	}
	vm_map_lock_downgrade(map);

out:
#ifdef RACCT
	if (racct_enable && rv != KERN_SUCCESS) {
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

	KASSERT((curthread->td_pflags & TDP_EXECVMSPC) == 0,
	    ("vmspace_exec recursed"));
	newvmspace = vmspace_alloc(minuser, maxuser, pmap_pinit);
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
	curthread->td_pflags |= TDP_EXECVMSPC;
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

	/*
	 * The caller is responsible for ensuring that the reference count
	 * cannot concurrently transition 1 -> 2.
	 */
	if (refcount_load(&oldvmspace->vm_refcnt) == 1)
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
	vm_prot_t fault_type;
	vm_object_t eobject;
	vm_size_t size;
	struct ucred *cred;

RetryLookup:

	vm_map_lock_read(map);

RetryLookupLocked:
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
	if ((fault_typea & VM_PROT_FAULT_LOOKUP) != 0) {
		fault_typea &= ~VM_PROT_FAULT_LOOKUP;
		if (prot == VM_PROT_NONE && map != kernel_map &&
		    (entry->eflags & MAP_ENTRY_GUARD) != 0 &&
		    (entry->eflags & (MAP_ENTRY_STACK_GAP_DN |
		    MAP_ENTRY_STACK_GAP_UP)) != 0 &&
		    vm_map_growstack(map, vaddr, entry) == KERN_SUCCESS)
			goto RetryLookupLocked;
	}
	fault_type = fault_typea & VM_PROT_ALL;
	if ((fault_type & prot) != fault_type || prot == VM_PROT_NONE) {
		vm_map_unlock_read(map);
		return (KERN_PROTECTION_FAILURE);
	}
	KASSERT((prot & VM_PROT_WRITE) == 0 || (entry->eflags &
	    (MAP_ENTRY_USER_WIRED | MAP_ENTRY_NEEDS_COPY)) !=
	    (MAP_ENTRY_USER_WIRED | MAP_ENTRY_NEEDS_COPY),
	    ("entry %p flags %x", entry, entry->eflags));
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
			eobject = entry->object.vm_object;
			vm_object_shadow(&entry->object.vm_object,
			    &entry->offset, size, entry->cred, false);
			if (eobject == entry->object.vm_object) {
				/*
				 * The object was not shadowed.
				 */
				swap_release_by_cred(size, entry->cred);
				crfree(entry->cred);
			}
			entry->cred = NULL;
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
	if (entry->object.vm_object == NULL && !map->system_map) {
		if (vm_map_lock_upgrade(map))
			goto RetryLookup;
		entry->object.vm_object = vm_object_allocate_anon(atop(size),
		    NULL, entry->cred, entry->cred != NULL ? size : 0);
		entry->offset = 0;
		entry->cred = NULL;
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

vm_offset_t
vm_map_max_KBI(const struct vm_map *map)
{

	return (vm_map_max(map));
}

vm_offset_t
vm_map_min_KBI(const struct vm_map *map)
{

	return (vm_map_min(map));
}

pmap_t
vm_map_pmap_KBI(vm_map_t map)
{

	return (map->pmap);
}

bool
vm_map_range_valid_KBI(vm_map_t map, vm_offset_t start, vm_offset_t end)
{

	return (vm_map_range_valid(map, start, end));
}

#ifdef INVARIANTS
static void
_vm_map_assert_consistent(vm_map_t map, int check)
{
	vm_map_entry_t entry, prev;
	vm_map_entry_t cur, header, lbound, ubound;
	vm_size_t max_left, max_right;

#ifdef DIAGNOSTIC
	++map->nupdates;
#endif
	if (enable_vmmap_check != check)
		return;

	header = prev = &map->header;
	VM_MAP_ENTRY_FOREACH(entry, map) {
		KASSERT(prev->end <= entry->start,
		    ("map %p prev->end = %jx, start = %jx", map,
		    (uintmax_t)prev->end, (uintmax_t)entry->start));
		KASSERT(entry->start < entry->end,
		    ("map %p start = %jx, end = %jx", map,
		    (uintmax_t)entry->start, (uintmax_t)entry->end));
		KASSERT(entry->left == header ||
		    entry->left->start < entry->start,
		    ("map %p left->start = %jx, start = %jx", map,
		    (uintmax_t)entry->left->start, (uintmax_t)entry->start));
		KASSERT(entry->right == header ||
		    entry->start < entry->right->start,
		    ("map %p start = %jx, right->start = %jx", map,
		    (uintmax_t)entry->start, (uintmax_t)entry->right->start));
		cur = map->root;
		lbound = ubound = header;
		for (;;) {
			if (entry->start < cur->start) {
				ubound = cur;
				cur = cur->left;
				KASSERT(cur != lbound,
				    ("map %p cannot find %jx",
				    map, (uintmax_t)entry->start));
			} else if (cur->end <= entry->start) {
				lbound = cur;
				cur = cur->right;
				KASSERT(cur != ubound,
				    ("map %p cannot find %jx",
				    map, (uintmax_t)entry->start));
			} else {
				KASSERT(cur == entry,
				    ("map %p cannot find %jx",
				    map, (uintmax_t)entry->start));
				break;
			}
		}
		max_left = vm_map_entry_max_free_left(entry, lbound);
		max_right = vm_map_entry_max_free_right(entry, ubound);
		KASSERT(entry->max_free == vm_size_max(max_left, max_right),
		    ("map %p max = %jx, max_left = %jx, max_right = %jx", map,
		    (uintmax_t)entry->max_free,
		    (uintmax_t)max_left, (uintmax_t)max_right));
		prev = entry;
	}
	KASSERT(prev->end <= entry->start,
	    ("map %p prev->end = %jx, start = %jx", map,
	    (uintmax_t)prev->end, (uintmax_t)entry->start));
}
#endif

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

static void
vm_map_print(vm_map_t map)
{
	vm_map_entry_t entry, prev;

	db_iprintf("Task map %p: pmap=%p, nentries=%d, version=%u\n",
	    (void *)map,
	    (void *)map->pmap, map->nentries, map->timestamp);

	db_indent += 2;
	prev = &map->header;
	VM_MAP_ENTRY_FOREACH(entry, map) {
		db_iprintf("map entry %p: start=%p, end=%p, eflags=%#x, \n",
		    (void *)entry, (void *)entry->start, (void *)entry->end,
		    entry->eflags);
		{
			static const char * const inheritance_name[4] =
			{"share", "copy", "none", "donate_copy"};

			db_iprintf(" prot=%x/%x/%s",
			    entry->protection,
			    entry->max_protection,
			    inheritance_name[(int)(unsigned char)
			    entry->inheritance]);
			if (entry->wired_count != 0)
				db_printf(", wired");
		}
		if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
			db_printf(", share=%p, offset=0x%jx\n",
			    (void *)entry->object.sub_map,
			    (uintmax_t)entry->offset);
			if (prev == &map->header ||
			    prev->object.sub_map !=
				entry->object.sub_map) {
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

			if (prev == &map->header ||
			    prev->object.vm_object !=
				entry->object.vm_object) {
				db_indent += 2;
				vm_object_print((db_expr_t)(intptr_t)
						entry->object.vm_object,
						0, 0, (char *)0);
				db_indent -= 2;
			}
		}
		prev = entry;
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
		p = db_lookup_proc(addr);
	} else {
		p = curproc;
	}

	db_printf("p = %p, vmspace = %p, map = %p, pmap = %p\n",
	    (void *)p, (void *)p->p_vmspace, (void *)&p->p_vmspace->vm_map,
	    (void *)vmspace_pmap(p->p_vmspace));

	vm_map_print((vm_map_t)&p->p_vmspace->vm_map);
}

#endif /* DDB */
