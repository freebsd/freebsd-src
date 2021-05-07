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
 *	from: @(#)vm_object.c	8.5 (Berkeley) 3/22/94
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
 *	Virtual memory object module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/blockcount.h>
#include <sys/cpuset.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/pctrie.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>
#include <sys/proc.h>		/* for curproc, pageproc */
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/resourcevar.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/swap_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/uma.h>

static int old_msync;
SYSCTL_INT(_vm, OID_AUTO, old_msync, CTLFLAG_RW, &old_msync, 0,
    "Use old (insecure) msync behavior");

static int	vm_object_page_collect_flush(vm_object_t object, vm_page_t p,
		    int pagerflags, int flags, boolean_t *allclean,
		    boolean_t *eio);
static boolean_t vm_object_page_remove_write(vm_page_t p, int flags,
		    boolean_t *allclean);
static void	vm_object_backing_remove(vm_object_t object);

/*
 *	Virtual memory objects maintain the actual data
 *	associated with allocated virtual memory.  A given
 *	page of memory exists within exactly one object.
 *
 *	An object is only deallocated when all "references"
 *	are given up.  Only one "reference" to a given
 *	region of an object should be writeable.
 *
 *	Associated with each object is a list of all resident
 *	memory pages belonging to that object; this list is
 *	maintained by the "vm_page" module, and locked by the object's
 *	lock.
 *
 *	Each object also records a "pager" routine which is
 *	used to retrieve (and store) pages to the proper backing
 *	storage.  In addition, objects may be backed by other
 *	objects from which they were virtual-copied.
 *
 *	The only items within the object structure which are
 *	modified after time of creation are:
 *		reference count		locked by object's lock
 *		pager routine		locked by object's lock
 *
 */

struct object_q vm_object_list;
struct mtx vm_object_list_mtx;	/* lock for object list and count */

struct vm_object kernel_object_store;

static SYSCTL_NODE(_vm_stats, OID_AUTO, object, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "VM object stats");

static COUNTER_U64_DEFINE_EARLY(object_collapses);
SYSCTL_COUNTER_U64(_vm_stats_object, OID_AUTO, collapses, CTLFLAG_RD,
    &object_collapses,
    "VM object collapses");

static COUNTER_U64_DEFINE_EARLY(object_bypasses);
SYSCTL_COUNTER_U64(_vm_stats_object, OID_AUTO, bypasses, CTLFLAG_RD,
    &object_bypasses,
    "VM object bypasses");

static COUNTER_U64_DEFINE_EARLY(object_collapse_waits);
SYSCTL_COUNTER_U64(_vm_stats_object, OID_AUTO, collapse_waits, CTLFLAG_RD,
    &object_collapse_waits,
    "Number of sleeps for collapse");

static uma_zone_t obj_zone;

static int vm_object_zinit(void *mem, int size, int flags);

#ifdef INVARIANTS
static void vm_object_zdtor(void *mem, int size, void *arg);

static void
vm_object_zdtor(void *mem, int size, void *arg)
{
	vm_object_t object;

	object = (vm_object_t)mem;
	KASSERT(object->ref_count == 0,
	    ("object %p ref_count = %d", object, object->ref_count));
	KASSERT(TAILQ_EMPTY(&object->memq),
	    ("object %p has resident pages in its memq", object));
	KASSERT(vm_radix_is_empty(&object->rtree),
	    ("object %p has resident pages in its trie", object));
#if VM_NRESERVLEVEL > 0
	KASSERT(LIST_EMPTY(&object->rvq),
	    ("object %p has reservations",
	    object));
#endif
	KASSERT(!vm_object_busied(object),
	    ("object %p busy = %d", object, blockcount_read(&object->busy)));
	KASSERT(object->resident_page_count == 0,
	    ("object %p resident_page_count = %d",
	    object, object->resident_page_count));
	KASSERT(object->shadow_count == 0,
	    ("object %p shadow_count = %d",
	    object, object->shadow_count));
	KASSERT(object->type == OBJT_DEAD,
	    ("object %p has non-dead type %d",
	    object, object->type));
}
#endif

static int
vm_object_zinit(void *mem, int size, int flags)
{
	vm_object_t object;

	object = (vm_object_t)mem;
	rw_init_flags(&object->lock, "vm object", RW_DUPOK | RW_NEW);

	/* These are true for any object that has been freed */
	object->type = OBJT_DEAD;
	vm_radix_init(&object->rtree);
	refcount_init(&object->ref_count, 0);
	blockcount_init(&object->paging_in_progress);
	blockcount_init(&object->busy);
	object->resident_page_count = 0;
	object->shadow_count = 0;
	object->flags = OBJ_DEAD;

	mtx_lock(&vm_object_list_mtx);
	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	mtx_unlock(&vm_object_list_mtx);
	return (0);
}

static void
_vm_object_allocate(objtype_t type, vm_pindex_t size, u_short flags,
    vm_object_t object, void *handle)
{

	TAILQ_INIT(&object->memq);
	LIST_INIT(&object->shadow_head);

	object->type = type;
	object->flags = flags;
	if ((flags & OBJ_SWAP) != 0)
		pctrie_init(&object->un_pager.swp.swp_blks);

	/*
	 * Ensure that swap_pager_swapoff() iteration over object_list
	 * sees up to date type and pctrie head if it observed
	 * non-dead object.
	 */
	atomic_thread_fence_rel();

	object->pg_color = 0;
	object->size = size;
	object->domain.dr_policy = NULL;
	object->generation = 1;
	object->cleangeneration = 1;
	refcount_init(&object->ref_count, 1);
	object->memattr = VM_MEMATTR_DEFAULT;
	object->cred = NULL;
	object->charge = 0;
	object->handle = handle;
	object->backing_object = NULL;
	object->backing_object_offset = (vm_ooffset_t) 0;
#if VM_NRESERVLEVEL > 0
	LIST_INIT(&object->rvq);
#endif
	umtx_shm_object_init(object);
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init(void)
{
	TAILQ_INIT(&vm_object_list);
	mtx_init(&vm_object_list_mtx, "vm object_list", NULL, MTX_DEF);

	rw_init(&kernel_object->lock, "kernel vm object");
	_vm_object_allocate(OBJT_PHYS, atop(VM_MAX_KERNEL_ADDRESS -
	    VM_MIN_KERNEL_ADDRESS), OBJ_UNMANAGED, kernel_object, NULL);
#if VM_NRESERVLEVEL > 0
	kernel_object->flags |= OBJ_COLORED;
	kernel_object->pg_color = (u_short)atop(VM_MIN_KERNEL_ADDRESS);
#endif
	kernel_object->un_pager.phys.ops = &default_phys_pg_ops;

	/*
	 * The lock portion of struct vm_object must be type stable due
	 * to vm_pageout_fallback_object_lock locking a vm object
	 * without holding any references to it.
	 *
	 * paging_in_progress is valid always.  Lockless references to
	 * the objects may acquire pip and then check OBJ_DEAD.
	 */
	obj_zone = uma_zcreate("VM OBJECT", sizeof (struct vm_object), NULL,
#ifdef INVARIANTS
	    vm_object_zdtor,
#else
	    NULL,
#endif
	    vm_object_zinit, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);

	vm_radix_zinit();
}

void
vm_object_clear_flag(vm_object_t object, u_short bits)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	object->flags &= ~bits;
}

/*
 *	Sets the default memory attribute for the specified object.  Pages
 *	that are allocated to this object are by default assigned this memory
 *	attribute.
 *
 *	Presently, this function must be called before any pages are allocated
 *	to the object.  In the future, this requirement may be relaxed for
 *	"default" and "swap" objects.
 */
int
vm_object_set_memattr(vm_object_t object, vm_memattr_t memattr)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	if (object->type == OBJT_DEAD)
		return (KERN_INVALID_ARGUMENT);
	if (!TAILQ_EMPTY(&object->memq))
		return (KERN_FAILURE);

	object->memattr = memattr;
	return (KERN_SUCCESS);
}

void
vm_object_pip_add(vm_object_t object, short i)
{

	if (i > 0)
		blockcount_acquire(&object->paging_in_progress, i);
}

void
vm_object_pip_wakeup(vm_object_t object)
{

	vm_object_pip_wakeupn(object, 1);
}

void
vm_object_pip_wakeupn(vm_object_t object, short i)
{

	if (i > 0)
		blockcount_release(&object->paging_in_progress, i);
}

/*
 * Atomically drop the object lock and wait for pip to drain.  This protects
 * from sleep/wakeup races due to identity changes.  The lock is not re-acquired
 * on return.
 */
static void
vm_object_pip_sleep(vm_object_t object, const char *waitid)
{

	(void)blockcount_sleep(&object->paging_in_progress, &object->lock,
	    waitid, PVM | PDROP);
}

void
vm_object_pip_wait(vm_object_t object, const char *waitid)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	blockcount_wait(&object->paging_in_progress, &object->lock, waitid,
	    PVM);
}

void
vm_object_pip_wait_unlocked(vm_object_t object, const char *waitid)
{

	VM_OBJECT_ASSERT_UNLOCKED(object);

	blockcount_wait(&object->paging_in_progress, NULL, waitid, PVM);
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */
vm_object_t
vm_object_allocate(objtype_t type, vm_pindex_t size)
{
	vm_object_t object;
	u_short flags;

	switch (type) {
	case OBJT_DEAD:
		panic("vm_object_allocate: can't create OBJT_DEAD");
	case OBJT_DEFAULT:
		flags = OBJ_COLORED;
		break;
	case OBJT_SWAP:
		flags = OBJ_COLORED | OBJ_SWAP;
		break;
	case OBJT_DEVICE:
	case OBJT_SG:
		flags = OBJ_FICTITIOUS | OBJ_UNMANAGED;
		break;
	case OBJT_MGTDEVICE:
		flags = OBJ_FICTITIOUS;
		break;
	case OBJT_PHYS:
		flags = OBJ_UNMANAGED;
		break;
	case OBJT_VNODE:
		flags = 0;
		break;
	default:
		panic("vm_object_allocate: type %d is undefined or dynamic",
		    type);
	}
	object = (vm_object_t)uma_zalloc(obj_zone, M_WAITOK);
	_vm_object_allocate(type, size, flags, object, NULL);

	return (object);
}

vm_object_t
vm_object_allocate_dyn(objtype_t dyntype, vm_pindex_t size, u_short flags)
{
	vm_object_t object;

	MPASS(dyntype >= OBJT_FIRST_DYN /* && dyntype < nitems(pagertab) */);
	object = (vm_object_t)uma_zalloc(obj_zone, M_WAITOK);
	_vm_object_allocate(dyntype, size, flags, object, NULL);

	return (object);
}

/*
 *	vm_object_allocate_anon:
 *
 *	Returns a new default object of the given size and marked as
 *	anonymous memory for special split/collapse handling.  Color
 *	to be initialized by the caller.
 */
vm_object_t
vm_object_allocate_anon(vm_pindex_t size, vm_object_t backing_object,
    struct ucred *cred, vm_size_t charge)
{
	vm_object_t handle, object;

	if (backing_object == NULL)
		handle = NULL;
	else if ((backing_object->flags & OBJ_ANON) != 0)
		handle = backing_object->handle;
	else
		handle = backing_object;
	object = uma_zalloc(obj_zone, M_WAITOK);
	_vm_object_allocate(OBJT_DEFAULT, size, OBJ_ANON | OBJ_ONEMAPPING,
	    object, handle);
	object->cred = cred;
	object->charge = cred != NULL ? charge : 0;
	return (object);
}

static void
vm_object_reference_vnode(vm_object_t object)
{
	u_int old;

	/*
	 * vnode objects need the lock for the first reference
	 * to serialize with vnode_object_deallocate().
	 */
	if (!refcount_acquire_if_gt(&object->ref_count, 0)) {
		VM_OBJECT_RLOCK(object);
		old = refcount_acquire(&object->ref_count);
		if (object->type == OBJT_VNODE && old == 0)
			vref(object->handle);
		VM_OBJECT_RUNLOCK(object);
	}
}

/*
 *	vm_object_reference:
 *
 *	Acquires a reference to the given object.
 */
void
vm_object_reference(vm_object_t object)
{

	if (object == NULL)
		return;

	if (object->type == OBJT_VNODE)
		vm_object_reference_vnode(object);
	else
		refcount_acquire(&object->ref_count);
	KASSERT((object->flags & OBJ_DEAD) == 0,
	    ("vm_object_reference: Referenced dead object."));
}

/*
 *	vm_object_reference_locked:
 *
 *	Gets another reference to the given object.
 *
 *	The object must be locked.
 */
void
vm_object_reference_locked(vm_object_t object)
{
	u_int old;

	VM_OBJECT_ASSERT_LOCKED(object);
	old = refcount_acquire(&object->ref_count);
	if (object->type == OBJT_VNODE && old == 0)
		vref(object->handle);
	KASSERT((object->flags & OBJ_DEAD) == 0,
	    ("vm_object_reference: Referenced dead object."));
}

/*
 * Handle deallocating an object of type OBJT_VNODE.
 */
static void
vm_object_deallocate_vnode(vm_object_t object)
{
	struct vnode *vp = (struct vnode *) object->handle;
	bool last;

	KASSERT(object->type == OBJT_VNODE,
	    ("vm_object_deallocate_vnode: not a vnode object"));
	KASSERT(vp != NULL, ("vm_object_deallocate_vnode: missing vp"));

	/* Object lock to protect handle lookup. */
	last = refcount_release(&object->ref_count);
	VM_OBJECT_RUNLOCK(object);

	if (!last)
		return;

	if (!umtx_shm_vnobj_persistent)
		umtx_shm_object_terminated(object);

	/* vrele may need the vnode lock. */
	vrele(vp);
}

/*
 * We dropped a reference on an object and discovered that it had a
 * single remaining shadow.  This is a sibling of the reference we
 * dropped.  Attempt to collapse the sibling and backing object.
 */
static vm_object_t
vm_object_deallocate_anon(vm_object_t backing_object)
{
	vm_object_t object;

	/* Fetch the final shadow.  */
	object = LIST_FIRST(&backing_object->shadow_head);
	KASSERT(object != NULL && backing_object->shadow_count == 1,
	    ("vm_object_anon_deallocate: ref_count: %d, shadow_count: %d",
	    backing_object->ref_count, backing_object->shadow_count));
	KASSERT((object->flags & OBJ_ANON) != 0,
	    ("invalid shadow object %p", object));

	if (!VM_OBJECT_TRYWLOCK(object)) {
		/*
		 * Prevent object from disappearing since we do not have a
		 * reference.
		 */
		vm_object_pip_add(object, 1);
		VM_OBJECT_WUNLOCK(backing_object);
		VM_OBJECT_WLOCK(object);
		vm_object_pip_wakeup(object);
	} else
		VM_OBJECT_WUNLOCK(backing_object);

	/*
	 * Check for a collapse/terminate race with the last reference holder.
	 */
	if ((object->flags & (OBJ_DEAD | OBJ_COLLAPSING)) != 0 ||
	    !refcount_acquire_if_not_zero(&object->ref_count)) {
		VM_OBJECT_WUNLOCK(object);
		return (NULL);
	}
	backing_object = object->backing_object;
	if (backing_object != NULL && (backing_object->flags & OBJ_ANON) != 0)
		vm_object_collapse(object);
	VM_OBJECT_WUNLOCK(object);

	return (object);
}

/*
 *	vm_object_deallocate:
 *
 *	Release a reference to the specified object,
 *	gained either through a vm_object_allocate
 *	or a vm_object_reference call.  When all references
 *	are gone, storage associated with this object
 *	may be relinquished.
 *
 *	No object may be locked.
 */
void
vm_object_deallocate(vm_object_t object)
{
	vm_object_t temp;
	bool released;

	while (object != NULL) {
		/*
		 * If the reference count goes to 0 we start calling
		 * vm_object_terminate() on the object chain.  A ref count
		 * of 1 may be a special case depending on the shadow count
		 * being 0 or 1.  These cases require a write lock on the
		 * object.
		 */
		if ((object->flags & OBJ_ANON) == 0)
			released = refcount_release_if_gt(&object->ref_count, 1);
		else
			released = refcount_release_if_gt(&object->ref_count, 2);
		if (released)
			return;

		if (object->type == OBJT_VNODE) {
			VM_OBJECT_RLOCK(object);
			if (object->type == OBJT_VNODE) {
				vm_object_deallocate_vnode(object);
				return;
			}
			VM_OBJECT_RUNLOCK(object);
		}

		VM_OBJECT_WLOCK(object);
		KASSERT(object->ref_count > 0,
		    ("vm_object_deallocate: object deallocated too many times: %d",
		    object->type));

		/*
		 * If this is not the final reference to an anonymous
		 * object we may need to collapse the shadow chain.
		 */
		if (!refcount_release(&object->ref_count)) {
			if (object->ref_count > 1 ||
			    object->shadow_count == 0) {
				if ((object->flags & OBJ_ANON) != 0 &&
				    object->ref_count == 1)
					vm_object_set_flag(object,
					    OBJ_ONEMAPPING);
				VM_OBJECT_WUNLOCK(object);
				return;
			}

			/* Handle collapsing last ref on anonymous objects. */
			object = vm_object_deallocate_anon(object);
			continue;
		}

		/*
		 * Handle the final reference to an object.  We restart
		 * the loop with the backing object to avoid recursion.
		 */
		umtx_shm_object_terminated(object);
		temp = object->backing_object;
		if (temp != NULL) {
			KASSERT(object->type == OBJT_DEFAULT ||
			    object->type == OBJT_SWAP,
			    ("shadowed tmpfs v_object 2 %p", object));
			vm_object_backing_remove(object);
		}

		KASSERT((object->flags & OBJ_DEAD) == 0,
		    ("vm_object_deallocate: Terminating dead object."));
		vm_object_set_flag(object, OBJ_DEAD);
		vm_object_terminate(object);
		object = temp;
	}
}

/*
 *	vm_object_destroy removes the object from the global object list
 *      and frees the space for the object.
 */
void
vm_object_destroy(vm_object_t object)
{

	/*
	 * Release the allocation charge.
	 */
	if (object->cred != NULL) {
		swap_release_by_cred(object->charge, object->cred);
		object->charge = 0;
		crfree(object->cred);
		object->cred = NULL;
	}

	/*
	 * Free the space for the object.
	 */
	uma_zfree(obj_zone, object);
}

static void
vm_object_backing_remove_locked(vm_object_t object)
{
	vm_object_t backing_object;

	backing_object = object->backing_object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	VM_OBJECT_ASSERT_WLOCKED(backing_object);

	KASSERT((object->flags & OBJ_COLLAPSING) == 0,
	    ("vm_object_backing_remove: Removing collapsing object."));

	if ((object->flags & OBJ_SHADOWLIST) != 0) {
		LIST_REMOVE(object, shadow_list);
		backing_object->shadow_count--;
		object->flags &= ~OBJ_SHADOWLIST;
	}
	object->backing_object = NULL;
}

static void
vm_object_backing_remove(vm_object_t object)
{
	vm_object_t backing_object;

	VM_OBJECT_ASSERT_WLOCKED(object);

	if ((object->flags & OBJ_SHADOWLIST) != 0) {
		backing_object = object->backing_object;
		VM_OBJECT_WLOCK(backing_object);
		vm_object_backing_remove_locked(object);
		VM_OBJECT_WUNLOCK(backing_object);
	} else
		object->backing_object = NULL;
}

static void
vm_object_backing_insert_locked(vm_object_t object, vm_object_t backing_object)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	if ((backing_object->flags & OBJ_ANON) != 0) {
		VM_OBJECT_ASSERT_WLOCKED(backing_object);
		LIST_INSERT_HEAD(&backing_object->shadow_head, object,
		    shadow_list);
		backing_object->shadow_count++;
		object->flags |= OBJ_SHADOWLIST;
	}
	object->backing_object = backing_object;
}

static void
vm_object_backing_insert(vm_object_t object, vm_object_t backing_object)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	if ((backing_object->flags & OBJ_ANON) != 0) {
		VM_OBJECT_WLOCK(backing_object);
		vm_object_backing_insert_locked(object, backing_object);
		VM_OBJECT_WUNLOCK(backing_object);
	} else
		object->backing_object = backing_object;
}

/*
 * Insert an object into a backing_object's shadow list with an additional
 * reference to the backing_object added.
 */
static void
vm_object_backing_insert_ref(vm_object_t object, vm_object_t backing_object)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	if ((backing_object->flags & OBJ_ANON) != 0) {
		VM_OBJECT_WLOCK(backing_object);
		KASSERT((backing_object->flags & OBJ_DEAD) == 0,
		    ("shadowing dead anonymous object"));
		vm_object_reference_locked(backing_object);
		vm_object_backing_insert_locked(object, backing_object);
		vm_object_clear_flag(backing_object, OBJ_ONEMAPPING);
		VM_OBJECT_WUNLOCK(backing_object);
	} else {
		vm_object_reference(backing_object);
		object->backing_object = backing_object;
	}
}

/*
 * Transfer a backing reference from backing_object to object.
 */
static void
vm_object_backing_transfer(vm_object_t object, vm_object_t backing_object)
{
	vm_object_t new_backing_object;

	/*
	 * Note that the reference to backing_object->backing_object
	 * moves from within backing_object to within object.
	 */
	vm_object_backing_remove_locked(object);
	new_backing_object = backing_object->backing_object;
	if (new_backing_object == NULL)
		return;
	if ((new_backing_object->flags & OBJ_ANON) != 0) {
		VM_OBJECT_WLOCK(new_backing_object);
		vm_object_backing_remove_locked(backing_object);
		vm_object_backing_insert_locked(object, new_backing_object);
		VM_OBJECT_WUNLOCK(new_backing_object);
	} else {
		object->backing_object = new_backing_object;
		backing_object->backing_object = NULL;
	}
}

/*
 * Wait for a concurrent collapse to settle.
 */
static void
vm_object_collapse_wait(vm_object_t object)
{

	VM_OBJECT_ASSERT_WLOCKED(object);

	while ((object->flags & OBJ_COLLAPSING) != 0) {
		vm_object_pip_wait(object, "vmcolwait");
		counter_u64_add(object_collapse_waits, 1);
	}
}

/*
 * Waits for a backing object to clear a pending collapse and returns
 * it locked if it is an ANON object.
 */
static vm_object_t
vm_object_backing_collapse_wait(vm_object_t object)
{
	vm_object_t backing_object;

	VM_OBJECT_ASSERT_WLOCKED(object);

	for (;;) {
		backing_object = object->backing_object;
		if (backing_object == NULL ||
		    (backing_object->flags & OBJ_ANON) == 0)
			return (NULL);
		VM_OBJECT_WLOCK(backing_object);
		if ((backing_object->flags & (OBJ_DEAD | OBJ_COLLAPSING)) == 0)
			break;
		VM_OBJECT_WUNLOCK(object);
		vm_object_pip_sleep(backing_object, "vmbckwait");
		counter_u64_add(object_collapse_waits, 1);
		VM_OBJECT_WLOCK(object);
	}
	return (backing_object);
}

/*
 *	vm_object_terminate_pages removes any remaining pageable pages
 *	from the object and resets the object to an empty state.
 */
static void
vm_object_terminate_pages(vm_object_t object)
{
	vm_page_t p, p_next;

	VM_OBJECT_ASSERT_WLOCKED(object);

	/*
	 * Free any remaining pageable pages.  This also removes them from the
	 * paging queues.  However, don't free wired pages, just remove them
	 * from the object.  Rather than incrementally removing each page from
	 * the object, the page and object are reset to any empty state. 
	 */
	TAILQ_FOREACH_SAFE(p, &object->memq, listq, p_next) {
		vm_page_assert_unbusied(p);
		KASSERT(p->object == object &&
		    (p->ref_count & VPRC_OBJREF) != 0,
		    ("vm_object_terminate_pages: page %p is inconsistent", p));

		p->object = NULL;
		if (vm_page_drop(p, VPRC_OBJREF) == VPRC_OBJREF) {
			VM_CNT_INC(v_pfree);
			vm_page_free(p);
		}
	}

	/*
	 * If the object contained any pages, then reset it to an empty state.
	 * None of the object's fields, including "resident_page_count", were
	 * modified by the preceding loop.
	 */
	if (object->resident_page_count != 0) {
		vm_radix_reclaim_allnodes(&object->rtree);
		TAILQ_INIT(&object->memq);
		object->resident_page_count = 0;
		if (object->type == OBJT_VNODE)
			vdrop(object->handle);
	}
}

/*
 *	vm_object_terminate actually destroys the specified object, freeing
 *	up all previously used resources.
 *
 *	The object must be locked.
 *	This routine may block.
 */
void
vm_object_terminate(vm_object_t object)
{

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT((object->flags & OBJ_DEAD) != 0,
	    ("terminating non-dead obj %p", object));
	KASSERT((object->flags & OBJ_COLLAPSING) == 0,
	    ("terminating collapsing obj %p", object));
	KASSERT(object->backing_object == NULL,
	    ("terminating shadow obj %p", object));

	/*
	 * Wait for the pageout daemon and other current users to be
	 * done with the object.  Note that new paging_in_progress
	 * users can come after this wait, but they must check
	 * OBJ_DEAD flag set (without unlocking the object), and avoid
	 * the object being terminated.
	 */
	vm_object_pip_wait(object, "objtrm");

	KASSERT(object->ref_count == 0,
	    ("vm_object_terminate: object with references, ref_count=%d",
	    object->ref_count));

	if ((object->flags & OBJ_PG_DTOR) == 0)
		vm_object_terminate_pages(object);

#if VM_NRESERVLEVEL > 0
	if (__predict_false(!LIST_EMPTY(&object->rvq)))
		vm_reserv_break_all(object);
#endif

	KASSERT(object->cred == NULL || object->type == OBJT_DEFAULT ||
	    (object->flags & OBJ_SWAP) != 0,
	    ("%s: non-swap obj %p has cred", __func__, object));

	/*
	 * Let the pager know object is dead.
	 */
	vm_pager_deallocate(object);
	VM_OBJECT_WUNLOCK(object);

	vm_object_destroy(object);
}

/*
 * Make the page read-only so that we can clear the object flags.  However, if
 * this is a nosync mmap then the object is likely to stay dirty so do not
 * mess with the page and do not clear the object flags.  Returns TRUE if the
 * page should be flushed, and FALSE otherwise.
 */
static boolean_t
vm_object_page_remove_write(vm_page_t p, int flags, boolean_t *allclean)
{

	vm_page_assert_busied(p);

	/*
	 * If we have been asked to skip nosync pages and this is a
	 * nosync page, skip it.  Note that the object flags were not
	 * cleared in this case so we do not have to set them.
	 */
	if ((flags & OBJPC_NOSYNC) != 0 && (p->a.flags & PGA_NOSYNC) != 0) {
		*allclean = FALSE;
		return (FALSE);
	} else {
		pmap_remove_write(p);
		return (p->dirty != 0);
	}
}

/*
 *	vm_object_page_clean
 *
 *	Clean all dirty pages in the specified range of object.  Leaves page 
 * 	on whatever queue it is currently on.   If NOSYNC is set then do not
 *	write out pages with PGA_NOSYNC set (originally comes from MAP_NOSYNC),
 *	leaving the object dirty.
 *
 *	For swap objects backing tmpfs regular files, do not flush anything,
 *	but remove write protection on the mapped pages to update mtime through
 *	mmaped writes.
 *
 *	When stuffing pages asynchronously, allow clustering.  XXX we need a
 *	synchronous clustering mode implementation.
 *
 *	Odd semantics: if start == end, we clean everything.
 *
 *	The object must be locked.
 *
 *	Returns FALSE if some page from the range was not written, as
 *	reported by the pager, and TRUE otherwise.
 */
boolean_t
vm_object_page_clean(vm_object_t object, vm_ooffset_t start, vm_ooffset_t end,
    int flags)
{
	vm_page_t np, p;
	vm_pindex_t pi, tend, tstart;
	int curgeneration, n, pagerflags;
	boolean_t eio, res, allclean;

	VM_OBJECT_ASSERT_WLOCKED(object);

	if (!vm_object_mightbedirty(object) || object->resident_page_count == 0)
		return (TRUE);

	pagerflags = (flags & (OBJPC_SYNC | OBJPC_INVAL)) != 0 ?
	    VM_PAGER_PUT_SYNC : VM_PAGER_CLUSTER_OK;
	pagerflags |= (flags & OBJPC_INVAL) != 0 ? VM_PAGER_PUT_INVAL : 0;

	tstart = OFF_TO_IDX(start);
	tend = (end == 0) ? object->size : OFF_TO_IDX(end + PAGE_MASK);
	allclean = tstart == 0 && tend >= object->size;
	res = TRUE;

rescan:
	curgeneration = object->generation;

	for (p = vm_page_find_least(object, tstart); p != NULL; p = np) {
		pi = p->pindex;
		if (pi >= tend)
			break;
		np = TAILQ_NEXT(p, listq);
		if (vm_page_none_valid(p))
			continue;
		if (vm_page_busy_acquire(p, VM_ALLOC_WAITFAIL) == 0) {
			if (object->generation != curgeneration &&
			    (flags & OBJPC_SYNC) != 0)
				goto rescan;
			np = vm_page_find_least(object, pi);
			continue;
		}
		if (!vm_object_page_remove_write(p, flags, &allclean)) {
			vm_page_xunbusy(p);
			continue;
		}
		if (object->type == OBJT_VNODE) {
			n = vm_object_page_collect_flush(object, p, pagerflags,
			    flags, &allclean, &eio);
			if (eio) {
				res = FALSE;
				allclean = FALSE;
			}
			if (object->generation != curgeneration &&
			    (flags & OBJPC_SYNC) != 0)
				goto rescan;

			/*
			 * If the VOP_PUTPAGES() did a truncated write, so
			 * that even the first page of the run is not fully
			 * written, vm_pageout_flush() returns 0 as the run
			 * length.  Since the condition that caused truncated
			 * write may be permanent, e.g. exhausted free space,
			 * accepting n == 0 would cause an infinite loop.
			 *
			 * Forwarding the iterator leaves the unwritten page
			 * behind, but there is not much we can do there if
			 * filesystem refuses to write it.
			 */
			if (n == 0) {
				n = 1;
				allclean = FALSE;
			}
		} else {
			n = 1;
			vm_page_xunbusy(p);
		}
		np = vm_page_find_least(object, pi + n);
	}
#if 0
	VOP_FSYNC(vp, (pagerflags & VM_PAGER_PUT_SYNC) ? MNT_WAIT : 0);
#endif

	/*
	 * Leave updating cleangeneration for tmpfs objects to tmpfs
	 * scan.  It needs to update mtime, which happens for other
	 * filesystems during page writeouts.
	 */
	if (allclean && object->type == OBJT_VNODE)
		object->cleangeneration = curgeneration;
	return (res);
}

static int
vm_object_page_collect_flush(vm_object_t object, vm_page_t p, int pagerflags,
    int flags, boolean_t *allclean, boolean_t *eio)
{
	vm_page_t ma[vm_pageout_page_count], p_first, tp;
	int count, i, mreq, runlen;

	vm_page_lock_assert(p, MA_NOTOWNED);
	vm_page_assert_xbusied(p);
	VM_OBJECT_ASSERT_WLOCKED(object);

	count = 1;
	mreq = 0;

	for (tp = p; count < vm_pageout_page_count; count++) {
		tp = vm_page_next(tp);
		if (tp == NULL || vm_page_tryxbusy(tp) == 0)
			break;
		if (!vm_object_page_remove_write(tp, flags, allclean)) {
			vm_page_xunbusy(tp);
			break;
		}
	}

	for (p_first = p; count < vm_pageout_page_count; count++) {
		tp = vm_page_prev(p_first);
		if (tp == NULL || vm_page_tryxbusy(tp) == 0)
			break;
		if (!vm_object_page_remove_write(tp, flags, allclean)) {
			vm_page_xunbusy(tp);
			break;
		}
		p_first = tp;
		mreq++;
	}

	for (tp = p_first, i = 0; i < count; tp = TAILQ_NEXT(tp, listq), i++)
		ma[i] = tp;

	vm_pageout_flush(ma, count, pagerflags, mreq, &runlen, eio);
	return (runlen);
}

/*
 * Note that there is absolutely no sense in writing out
 * anonymous objects, so we track down the vnode object
 * to write out.
 * We invalidate (remove) all pages from the address space
 * for semantic correctness.
 *
 * If the backing object is a device object with unmanaged pages, then any
 * mappings to the specified range of pages must be removed before this
 * function is called.
 *
 * Note: certain anonymous maps, such as MAP_NOSYNC maps,
 * may start out with a NULL object.
 */
boolean_t
vm_object_sync(vm_object_t object, vm_ooffset_t offset, vm_size_t size,
    boolean_t syncio, boolean_t invalidate)
{
	vm_object_t backing_object;
	struct vnode *vp;
	struct mount *mp;
	int error, flags, fsync_after;
	boolean_t res;

	if (object == NULL)
		return (TRUE);
	res = TRUE;
	error = 0;
	VM_OBJECT_WLOCK(object);
	while ((backing_object = object->backing_object) != NULL) {
		VM_OBJECT_WLOCK(backing_object);
		offset += object->backing_object_offset;
		VM_OBJECT_WUNLOCK(object);
		object = backing_object;
		if (object->size < OFF_TO_IDX(offset + size))
			size = IDX_TO_OFF(object->size) - offset;
	}
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
	if (object->type == OBJT_VNODE &&
	    vm_object_mightbedirty(object) != 0 &&
	    ((vp = object->handle)->v_vflag & VV_NOSYNC) == 0) {
		VM_OBJECT_WUNLOCK(object);
		(void) vn_start_write(vp, &mp, V_WAIT);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		if (syncio && !invalidate && offset == 0 &&
		    atop(size) == object->size) {
			/*
			 * If syncing the whole mapping of the file,
			 * it is faster to schedule all the writes in
			 * async mode, also allowing the clustering,
			 * and then wait for i/o to complete.
			 */
			flags = 0;
			fsync_after = TRUE;
		} else {
			flags = (syncio || invalidate) ? OBJPC_SYNC : 0;
			flags |= invalidate ? (OBJPC_SYNC | OBJPC_INVAL) : 0;
			fsync_after = FALSE;
		}
		VM_OBJECT_WLOCK(object);
		res = vm_object_page_clean(object, offset, offset + size,
		    flags);
		VM_OBJECT_WUNLOCK(object);
		if (fsync_after)
			error = VOP_FSYNC(vp, MNT_WAIT, curthread);
		VOP_UNLOCK(vp);
		vn_finished_write(mp);
		if (error != 0)
			res = FALSE;
		VM_OBJECT_WLOCK(object);
	}
	if ((object->type == OBJT_VNODE ||
	     object->type == OBJT_DEVICE) && invalidate) {
		if (object->type == OBJT_DEVICE)
			/*
			 * The option OBJPR_NOTMAPPED must be passed here
			 * because vm_object_page_remove() cannot remove
			 * unmanaged mappings.
			 */
			flags = OBJPR_NOTMAPPED;
		else if (old_msync)
			flags = 0;
		else
			flags = OBJPR_CLEANONLY;
		vm_object_page_remove(object, OFF_TO_IDX(offset),
		    OFF_TO_IDX(offset + size + PAGE_MASK), flags);
	}
	VM_OBJECT_WUNLOCK(object);
	return (res);
}

/*
 * Determine whether the given advice can be applied to the object.  Advice is
 * not applied to unmanaged pages since they never belong to page queues, and
 * since MADV_FREE is destructive, it can apply only to anonymous pages that
 * have been mapped at most once.
 */
static bool
vm_object_advice_applies(vm_object_t object, int advice)
{

	if ((object->flags & OBJ_UNMANAGED) != 0)
		return (false);
	if (advice != MADV_FREE)
		return (true);
	return ((object->flags & (OBJ_ONEMAPPING | OBJ_ANON)) ==
	    (OBJ_ONEMAPPING | OBJ_ANON));
}

static void
vm_object_madvise_freespace(vm_object_t object, int advice, vm_pindex_t pindex,
    vm_size_t size)
{

	if (advice == MADV_FREE)
		vm_pager_freespace(object, pindex, size);
}

/*
 *	vm_object_madvise:
 *
 *	Implements the madvise function at the object/page level.
 *
 *	MADV_WILLNEED	(any object)
 *
 *	    Activate the specified pages if they are resident.
 *
 *	MADV_DONTNEED	(any object)
 *
 *	    Deactivate the specified pages if they are resident.
 *
 *	MADV_FREE	(OBJT_DEFAULT/OBJT_SWAP objects,
 *			 OBJ_ONEMAPPING only)
 *
 *	    Deactivate and clean the specified pages if they are
 *	    resident.  This permits the process to reuse the pages
 *	    without faulting or the kernel to reclaim the pages
 *	    without I/O.
 */
void
vm_object_madvise(vm_object_t object, vm_pindex_t pindex, vm_pindex_t end,
    int advice)
{
	vm_pindex_t tpindex;
	vm_object_t backing_object, tobject;
	vm_page_t m, tm;

	if (object == NULL)
		return;

relookup:
	VM_OBJECT_WLOCK(object);
	if (!vm_object_advice_applies(object, advice)) {
		VM_OBJECT_WUNLOCK(object);
		return;
	}
	for (m = vm_page_find_least(object, pindex); pindex < end; pindex++) {
		tobject = object;

		/*
		 * If the next page isn't resident in the top-level object, we
		 * need to search the shadow chain.  When applying MADV_FREE, we
		 * take care to release any swap space used to store
		 * non-resident pages.
		 */
		if (m == NULL || pindex < m->pindex) {
			/*
			 * Optimize a common case: if the top-level object has
			 * no backing object, we can skip over the non-resident
			 * range in constant time.
			 */
			if (object->backing_object == NULL) {
				tpindex = (m != NULL && m->pindex < end) ?
				    m->pindex : end;
				vm_object_madvise_freespace(object, advice,
				    pindex, tpindex - pindex);
				if ((pindex = tpindex) == end)
					break;
				goto next_page;
			}

			tpindex = pindex;
			do {
				vm_object_madvise_freespace(tobject, advice,
				    tpindex, 1);
				/*
				 * Prepare to search the next object in the
				 * chain.
				 */
				backing_object = tobject->backing_object;
				if (backing_object == NULL)
					goto next_pindex;
				VM_OBJECT_WLOCK(backing_object);
				tpindex +=
				    OFF_TO_IDX(tobject->backing_object_offset);
				if (tobject != object)
					VM_OBJECT_WUNLOCK(tobject);
				tobject = backing_object;
				if (!vm_object_advice_applies(tobject, advice))
					goto next_pindex;
			} while ((tm = vm_page_lookup(tobject, tpindex)) ==
			    NULL);
		} else {
next_page:
			tm = m;
			m = TAILQ_NEXT(m, listq);
		}

		/*
		 * If the page is not in a normal state, skip it.  The page
		 * can not be invalidated while the object lock is held.
		 */
		if (!vm_page_all_valid(tm) || vm_page_wired(tm))
			goto next_pindex;
		KASSERT((tm->flags & PG_FICTITIOUS) == 0,
		    ("vm_object_madvise: page %p is fictitious", tm));
		KASSERT((tm->oflags & VPO_UNMANAGED) == 0,
		    ("vm_object_madvise: page %p is not managed", tm));
		if (vm_page_tryxbusy(tm) == 0) {
			if (object != tobject)
				VM_OBJECT_WUNLOCK(object);
			if (advice == MADV_WILLNEED) {
				/*
				 * Reference the page before unlocking and
				 * sleeping so that the page daemon is less
				 * likely to reclaim it.
				 */
				vm_page_aflag_set(tm, PGA_REFERENCED);
			}
			vm_page_busy_sleep(tm, "madvpo", false);
  			goto relookup;
		}
		vm_page_advise(tm, advice);
		vm_page_xunbusy(tm);
		vm_object_madvise_freespace(tobject, advice, tm->pindex, 1);
next_pindex:
		if (tobject != object)
			VM_OBJECT_WUNLOCK(tobject);
	}
	VM_OBJECT_WUNLOCK(object);
}

/*
 *	vm_object_shadow:
 *
 *	Create a new object which is backed by the
 *	specified existing object range.  The source
 *	object reference is deallocated.
 *
 *	The new object and offset into that object
 *	are returned in the source parameters.
 */
void
vm_object_shadow(vm_object_t *object, vm_ooffset_t *offset, vm_size_t length,
    struct ucred *cred, bool shared)
{
	vm_object_t source;
	vm_object_t result;

	source = *object;

	/*
	 * Don't create the new object if the old object isn't shared.
	 *
	 * If we hold the only reference we can guarantee that it won't
	 * increase while we have the map locked.  Otherwise the race is
	 * harmless and we will end up with an extra shadow object that
	 * will be collapsed later.
	 */
	if (source != NULL && source->ref_count == 1 &&
	    (source->flags & OBJ_ANON) != 0)
		return;

	/*
	 * Allocate a new object with the given length.
	 */
	result = vm_object_allocate_anon(atop(length), source, cred, length);

	/*
	 * Store the offset into the source object, and fix up the offset into
	 * the new object.
	 */
	result->backing_object_offset = *offset;

	if (shared || source != NULL) {
		VM_OBJECT_WLOCK(result);

		/*
		 * The new object shadows the source object, adding a
		 * reference to it.  Our caller changes his reference
		 * to point to the new object, removing a reference to
		 * the source object.  Net result: no change of
		 * reference count, unless the caller needs to add one
		 * more reference due to forking a shared map entry.
		 */
		if (shared) {
			vm_object_reference_locked(result);
			vm_object_clear_flag(result, OBJ_ONEMAPPING);
		}

		/*
		 * Try to optimize the result object's page color when
		 * shadowing in order to maintain page coloring
		 * consistency in the combined shadowed object.
		 */
		if (source != NULL) {
			vm_object_backing_insert(result, source);
			result->domain = source->domain;
#if VM_NRESERVLEVEL > 0
			result->flags |= source->flags & OBJ_COLORED;
			result->pg_color = (source->pg_color +
			    OFF_TO_IDX(*offset)) & ((1 << (VM_NFREEORDER -
			    1)) - 1);
#endif
		}
		VM_OBJECT_WUNLOCK(result);
	}

	/*
	 * Return the new things
	 */
	*offset = 0;
	*object = result;
}

/*
 *	vm_object_split:
 *
 * Split the pages in a map entry into a new object.  This affords
 * easier removal of unused pages, and keeps object inheritance from
 * being a negative impact on memory usage.
 */
void
vm_object_split(vm_map_entry_t entry)
{
	vm_page_t m, m_busy, m_next;
	vm_object_t orig_object, new_object, backing_object;
	vm_pindex_t idx, offidxstart;
	vm_size_t size;

	orig_object = entry->object.vm_object;
	KASSERT((orig_object->flags & OBJ_ONEMAPPING) != 0,
	    ("vm_object_split:  Splitting object with multiple mappings."));
	if ((orig_object->flags & OBJ_ANON) == 0)
		return;
	if (orig_object->ref_count <= 1)
		return;
	VM_OBJECT_WUNLOCK(orig_object);

	offidxstart = OFF_TO_IDX(entry->offset);
	size = atop(entry->end - entry->start);

	/*
	 * If swap_pager_copy() is later called, it will convert new_object
	 * into a swap object.
	 */
	new_object = vm_object_allocate_anon(size, orig_object,
	    orig_object->cred, ptoa(size));

	/*
	 * We must wait for the orig_object to complete any in-progress
	 * collapse so that the swap blocks are stable below.  The
	 * additional reference on backing_object by new object will
	 * prevent further collapse operations until split completes.
	 */
	VM_OBJECT_WLOCK(orig_object);
	vm_object_collapse_wait(orig_object);

	/*
	 * At this point, the new object is still private, so the order in
	 * which the original and new objects are locked does not matter.
	 */
	VM_OBJECT_WLOCK(new_object);
	new_object->domain = orig_object->domain;
	backing_object = orig_object->backing_object;
	if (backing_object != NULL) {
		vm_object_backing_insert_ref(new_object, backing_object);
		new_object->backing_object_offset = 
		    orig_object->backing_object_offset + entry->offset;
	}
	if (orig_object->cred != NULL) {
		crhold(orig_object->cred);
		KASSERT(orig_object->charge >= ptoa(size),
		    ("orig_object->charge < 0"));
		orig_object->charge -= ptoa(size);
	}

	/*
	 * Mark the split operation so that swap_pager_getpages() knows
	 * that the object is in transition.
	 */
	vm_object_set_flag(orig_object, OBJ_SPLIT);
	m_busy = NULL;
#ifdef INVARIANTS
	idx = 0;
#endif
retry:
	m = vm_page_find_least(orig_object, offidxstart);
	KASSERT(m == NULL || idx <= m->pindex - offidxstart,
	    ("%s: object %p was repopulated", __func__, orig_object));
	for (; m != NULL && (idx = m->pindex - offidxstart) < size;
	    m = m_next) {
		m_next = TAILQ_NEXT(m, listq);

		/*
		 * We must wait for pending I/O to complete before we can
		 * rename the page.
		 *
		 * We do not have to VM_PROT_NONE the page as mappings should
		 * not be changed by this operation.
		 */
		if (vm_page_tryxbusy(m) == 0) {
			VM_OBJECT_WUNLOCK(new_object);
			vm_page_sleep_if_busy(m, "spltwt");
			VM_OBJECT_WLOCK(new_object);
			goto retry;
		}

		/*
		 * The page was left invalid.  Likely placed there by
		 * an incomplete fault.  Just remove and ignore.
		 */
		if (vm_page_none_valid(m)) {
			if (vm_page_remove(m))
				vm_page_free(m);
			continue;
		}

		/* vm_page_rename() will dirty the page. */
		if (vm_page_rename(m, new_object, idx)) {
			vm_page_xunbusy(m);
			VM_OBJECT_WUNLOCK(new_object);
			VM_OBJECT_WUNLOCK(orig_object);
			vm_radix_wait();
			VM_OBJECT_WLOCK(orig_object);
			VM_OBJECT_WLOCK(new_object);
			goto retry;
		}

#if VM_NRESERVLEVEL > 0
		/*
		 * If some of the reservation's allocated pages remain with
		 * the original object, then transferring the reservation to
		 * the new object is neither particularly beneficial nor
		 * particularly harmful as compared to leaving the reservation
		 * with the original object.  If, however, all of the
		 * reservation's allocated pages are transferred to the new
		 * object, then transferring the reservation is typically
		 * beneficial.  Determining which of these two cases applies
		 * would be more costly than unconditionally renaming the
		 * reservation.
		 */
		vm_reserv_rename(m, new_object, orig_object, offidxstart);
#endif

		/*
		 * orig_object's type may change while sleeping, so keep track
		 * of the beginning of the busied range.
		 */
		if (orig_object->type != OBJT_SWAP)
			vm_page_xunbusy(m);
		else if (m_busy == NULL)
			m_busy = m;
	}
	if ((orig_object->flags & OBJ_SWAP) != 0) {
		/*
		 * swap_pager_copy() can sleep, in which case the orig_object's
		 * and new_object's locks are released and reacquired. 
		 */
		swap_pager_copy(orig_object, new_object, offidxstart, 0);
		if (m_busy != NULL)
			TAILQ_FOREACH_FROM(m_busy, &new_object->memq, listq)
				vm_page_xunbusy(m_busy);
	}
	vm_object_clear_flag(orig_object, OBJ_SPLIT);
	VM_OBJECT_WUNLOCK(orig_object);
	VM_OBJECT_WUNLOCK(new_object);
	entry->object.vm_object = new_object;
	entry->offset = 0LL;
	vm_object_deallocate(orig_object);
	VM_OBJECT_WLOCK(new_object);
}

static vm_page_t
vm_object_collapse_scan_wait(vm_object_t object, vm_page_t p)
{
	vm_object_t backing_object;

	VM_OBJECT_ASSERT_WLOCKED(object);
	backing_object = object->backing_object;
	VM_OBJECT_ASSERT_WLOCKED(backing_object);

	KASSERT(p == NULL || p->object == object || p->object == backing_object,
	    ("invalid ownership %p %p %p", p, object, backing_object));
	/* The page is only NULL when rename fails. */
	if (p == NULL) {
		VM_OBJECT_WUNLOCK(object);
		VM_OBJECT_WUNLOCK(backing_object);
		vm_radix_wait();
	} else {
		if (p->object == object)
			VM_OBJECT_WUNLOCK(backing_object);
		else
			VM_OBJECT_WUNLOCK(object);
		vm_page_busy_sleep(p, "vmocol", false);
	}
	VM_OBJECT_WLOCK(object);
	VM_OBJECT_WLOCK(backing_object);
	return (TAILQ_FIRST(&backing_object->memq));
}

static bool
vm_object_scan_all_shadowed(vm_object_t object)
{
	vm_object_t backing_object;
	vm_page_t p, pp;
	vm_pindex_t backing_offset_index, new_pindex, pi, ps;

	VM_OBJECT_ASSERT_WLOCKED(object);
	VM_OBJECT_ASSERT_WLOCKED(object->backing_object);

	backing_object = object->backing_object;

	if ((backing_object->flags & OBJ_ANON) == 0)
		return (false);

	pi = backing_offset_index = OFF_TO_IDX(object->backing_object_offset);
	p = vm_page_find_least(backing_object, pi);
	ps = swap_pager_find_least(backing_object, pi);

	/*
	 * Only check pages inside the parent object's range and
	 * inside the parent object's mapping of the backing object.
	 */
	for (;; pi++) {
		if (p != NULL && p->pindex < pi)
			p = TAILQ_NEXT(p, listq);
		if (ps < pi)
			ps = swap_pager_find_least(backing_object, pi);
		if (p == NULL && ps >= backing_object->size)
			break;
		else if (p == NULL)
			pi = ps;
		else
			pi = MIN(p->pindex, ps);

		new_pindex = pi - backing_offset_index;
		if (new_pindex >= object->size)
			break;

		if (p != NULL) {
			/*
			 * If the backing object page is busy a
			 * grandparent or older page may still be
			 * undergoing CoW.  It is not safe to collapse
			 * the backing object until it is quiesced.
			 */
			if (vm_page_tryxbusy(p) == 0)
				return (false);

			/*
			 * We raced with the fault handler that left
			 * newly allocated invalid page on the object
			 * queue and retried.
			 */
			if (!vm_page_all_valid(p))
				goto unbusy_ret;
		}

		/*
		 * See if the parent has the page or if the parent's object
		 * pager has the page.  If the parent has the page but the page
		 * is not valid, the parent's object pager must have the page.
		 *
		 * If this fails, the parent does not completely shadow the
		 * object and we might as well give up now.
		 */
		pp = vm_page_lookup(object, new_pindex);

		/*
		 * The valid check here is stable due to object lock
		 * being required to clear valid and initiate paging.
		 * Busy of p disallows fault handler to validate pp.
		 */
		if ((pp == NULL || vm_page_none_valid(pp)) &&
		    !vm_pager_has_page(object, new_pindex, NULL, NULL))
			goto unbusy_ret;
		if (p != NULL)
			vm_page_xunbusy(p);
	}
	return (true);

unbusy_ret:
	if (p != NULL)
		vm_page_xunbusy(p);
	return (false);
}

static void
vm_object_collapse_scan(vm_object_t object)
{
	vm_object_t backing_object;
	vm_page_t next, p, pp;
	vm_pindex_t backing_offset_index, new_pindex;

	VM_OBJECT_ASSERT_WLOCKED(object);
	VM_OBJECT_ASSERT_WLOCKED(object->backing_object);

	backing_object = object->backing_object;
	backing_offset_index = OFF_TO_IDX(object->backing_object_offset);

	/*
	 * Our scan
	 */
	for (p = TAILQ_FIRST(&backing_object->memq); p != NULL; p = next) {
		next = TAILQ_NEXT(p, listq);
		new_pindex = p->pindex - backing_offset_index;

		/*
		 * Check for busy page
		 */
		if (vm_page_tryxbusy(p) == 0) {
			next = vm_object_collapse_scan_wait(object, p);
			continue;
		}

		KASSERT(object->backing_object == backing_object,
		    ("vm_object_collapse_scan: backing object mismatch %p != %p",
		    object->backing_object, backing_object));
		KASSERT(p->object == backing_object,
		    ("vm_object_collapse_scan: object mismatch %p != %p",
		    p->object, backing_object));

		if (p->pindex < backing_offset_index ||
		    new_pindex >= object->size) {
			vm_pager_freespace(backing_object, p->pindex, 1);

			KASSERT(!pmap_page_is_mapped(p),
			    ("freeing mapped page %p", p));
			if (vm_page_remove(p))
				vm_page_free(p);
			continue;
		}

		if (!vm_page_all_valid(p)) {
			KASSERT(!pmap_page_is_mapped(p),
			    ("freeing mapped page %p", p));
			if (vm_page_remove(p))
				vm_page_free(p);
			continue;
		}

		pp = vm_page_lookup(object, new_pindex);
		if (pp != NULL && vm_page_tryxbusy(pp) == 0) {
			vm_page_xunbusy(p);
			/*
			 * The page in the parent is busy and possibly not
			 * (yet) valid.  Until its state is finalized by the
			 * busy bit owner, we can't tell whether it shadows the
			 * original page.
			 */
			next = vm_object_collapse_scan_wait(object, pp);
			continue;
		}

		if (pp != NULL && vm_page_none_valid(pp)) {
			/*
			 * The page was invalid in the parent.  Likely placed
			 * there by an incomplete fault.  Just remove and
			 * ignore.  p can replace it.
			 */
			if (vm_page_remove(pp))
				vm_page_free(pp);
			pp = NULL;
		}

		if (pp != NULL || vm_pager_has_page(object, new_pindex, NULL,
			NULL)) {
			/*
			 * The page already exists in the parent OR swap exists
			 * for this location in the parent.  Leave the parent's
			 * page alone.  Destroy the original page from the
			 * backing object.
			 */
			vm_pager_freespace(backing_object, p->pindex, 1);
			KASSERT(!pmap_page_is_mapped(p),
			    ("freeing mapped page %p", p));
			if (vm_page_remove(p))
				vm_page_free(p);
			if (pp != NULL)
				vm_page_xunbusy(pp);
			continue;
		}

		/*
		 * Page does not exist in parent, rename the page from the
		 * backing object to the main object.
		 *
		 * If the page was mapped to a process, it can remain mapped
		 * through the rename.  vm_page_rename() will dirty the page.
		 */
		if (vm_page_rename(p, object, new_pindex)) {
			vm_page_xunbusy(p);
			next = vm_object_collapse_scan_wait(object, NULL);
			continue;
		}

		/* Use the old pindex to free the right page. */
		vm_pager_freespace(backing_object, new_pindex +
		    backing_offset_index, 1);

#if VM_NRESERVLEVEL > 0
		/*
		 * Rename the reservation.
		 */
		vm_reserv_rename(p, object, backing_object,
		    backing_offset_index);
#endif
		vm_page_xunbusy(p);
	}
	return;
}

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 */
void
vm_object_collapse(vm_object_t object)
{
	vm_object_t backing_object, new_backing_object;

	VM_OBJECT_ASSERT_WLOCKED(object);

	while (TRUE) {
		KASSERT((object->flags & (OBJ_DEAD | OBJ_ANON)) == OBJ_ANON,
		    ("collapsing invalid object"));

		/*
		 * Wait for the backing_object to finish any pending
		 * collapse so that the caller sees the shortest possible
		 * shadow chain.
		 */
		backing_object = vm_object_backing_collapse_wait(object);
		if (backing_object == NULL)
			return;

		KASSERT(object->ref_count > 0 &&
		    object->ref_count > object->shadow_count,
		    ("collapse with invalid ref %d or shadow %d count.",
		    object->ref_count, object->shadow_count));
		KASSERT((backing_object->flags &
		    (OBJ_COLLAPSING | OBJ_DEAD)) == 0,
		    ("vm_object_collapse: Backing object already collapsing."));
		KASSERT((object->flags & (OBJ_COLLAPSING | OBJ_DEAD)) == 0,
		    ("vm_object_collapse: object is already collapsing."));

		/*
		 * We know that we can either collapse the backing object if
		 * the parent is the only reference to it, or (perhaps) have
		 * the parent bypass the object if the parent happens to shadow
		 * all the resident pages in the entire backing object.
		 */
		if (backing_object->ref_count == 1) {
			KASSERT(backing_object->shadow_count == 1,
			    ("vm_object_collapse: shadow_count: %d",
			    backing_object->shadow_count));
			vm_object_pip_add(object, 1);
			vm_object_set_flag(object, OBJ_COLLAPSING);
			vm_object_pip_add(backing_object, 1);
			vm_object_set_flag(backing_object, OBJ_DEAD);

			/*
			 * If there is exactly one reference to the backing
			 * object, we can collapse it into the parent.
			 */
			vm_object_collapse_scan(object);

#if VM_NRESERVLEVEL > 0
			/*
			 * Break any reservations from backing_object.
			 */
			if (__predict_false(!LIST_EMPTY(&backing_object->rvq)))
				vm_reserv_break_all(backing_object);
#endif

			/*
			 * Move the pager from backing_object to object.
			 */
			if ((backing_object->flags & OBJ_SWAP) != 0) {
				/*
				 * swap_pager_copy() can sleep, in which case
				 * the backing_object's and object's locks are
				 * released and reacquired.
				 * Since swap_pager_copy() is being asked to
				 * destroy backing_object, it will change the
				 * type to OBJT_DEFAULT.
				 */
				swap_pager_copy(
				    backing_object,
				    object,
				    OFF_TO_IDX(object->backing_object_offset), TRUE);
			}

			/*
			 * Object now shadows whatever backing_object did.
			 */
			vm_object_clear_flag(object, OBJ_COLLAPSING);
			vm_object_backing_transfer(object, backing_object);
			object->backing_object_offset +=
			    backing_object->backing_object_offset;
			VM_OBJECT_WUNLOCK(object);
			vm_object_pip_wakeup(object);

			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */
			KASSERT(backing_object->ref_count == 1, (
"backing_object %p was somehow re-referenced during collapse!",
			    backing_object));
			vm_object_pip_wakeup(backing_object);
			(void)refcount_release(&backing_object->ref_count);
			vm_object_terminate(backing_object);
			counter_u64_add(object_collapses, 1);
			VM_OBJECT_WLOCK(object);
		} else {
			/*
			 * If we do not entirely shadow the backing object,
			 * there is nothing we can do so we give up.
			 *
			 * The object lock and backing_object lock must not
			 * be dropped during this sequence.
			 */
			if (!vm_object_scan_all_shadowed(object)) {
				VM_OBJECT_WUNLOCK(backing_object);
				break;
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */
			vm_object_backing_remove_locked(object);
			new_backing_object = backing_object->backing_object;
			if (new_backing_object != NULL) {
				vm_object_backing_insert_ref(object,
				    new_backing_object);
				object->backing_object_offset +=
				    backing_object->backing_object_offset;
			}

			/*
			 * Drop the reference count on backing_object. Since
			 * its ref_count was at least 2, it will not vanish.
			 */
			(void)refcount_release(&backing_object->ref_count);
			KASSERT(backing_object->ref_count >= 1, (
"backing_object %p was somehow dereferenced during collapse!",
			    backing_object));
			VM_OBJECT_WUNLOCK(backing_object);
			counter_u64_add(object_bypasses, 1);
		}

		/*
		 * Try again with this object's new backing object.
		 */
	}
}

/*
 *	vm_object_page_remove:
 *
 *	For the given object, either frees or invalidates each of the
 *	specified pages.  In general, a page is freed.  However, if a page is
 *	wired for any reason other than the existence of a managed, wired
 *	mapping, then it may be invalidated but not removed from the object.
 *	Pages are specified by the given range ["start", "end") and the option
 *	OBJPR_CLEANONLY.  As a special case, if "end" is zero, then the range
 *	extends from "start" to the end of the object.  If the option
 *	OBJPR_CLEANONLY is specified, then only the non-dirty pages within the
 *	specified range are affected.  If the option OBJPR_NOTMAPPED is
 *	specified, then the pages within the specified range must have no
 *	mappings.  Otherwise, if this option is not specified, any mappings to
 *	the specified pages are removed before the pages are freed or
 *	invalidated.
 *
 *	In general, this operation should only be performed on objects that
 *	contain managed pages.  There are, however, two exceptions.  First, it
 *	is performed on the kernel and kmem objects by vm_map_entry_delete().
 *	Second, it is used by msync(..., MS_INVALIDATE) to invalidate device-
 *	backed pages.  In both of these cases, the option OBJPR_CLEANONLY must
 *	not be specified and the option OBJPR_NOTMAPPED must be specified.
 *
 *	The object must be locked.
 */
void
vm_object_page_remove(vm_object_t object, vm_pindex_t start, vm_pindex_t end,
    int options)
{
	vm_page_t p, next;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT((object->flags & OBJ_UNMANAGED) == 0 ||
	    (options & (OBJPR_CLEANONLY | OBJPR_NOTMAPPED)) == OBJPR_NOTMAPPED,
	    ("vm_object_page_remove: illegal options for object %p", object));
	if (object->resident_page_count == 0)
		return;
	vm_object_pip_add(object, 1);
again:
	p = vm_page_find_least(object, start);

	/*
	 * Here, the variable "p" is either (1) the page with the least pindex
	 * greater than or equal to the parameter "start" or (2) NULL. 
	 */
	for (; p != NULL && (p->pindex < end || end == 0); p = next) {
		next = TAILQ_NEXT(p, listq);

		/*
		 * If the page is wired for any reason besides the existence
		 * of managed, wired mappings, then it cannot be freed.  For
		 * example, fictitious pages, which represent device memory,
		 * are inherently wired and cannot be freed.  They can,
		 * however, be invalidated if the option OBJPR_CLEANONLY is
		 * not specified.
		 */
		if (vm_page_tryxbusy(p) == 0) {
			vm_page_sleep_if_busy(p, "vmopar");
			goto again;
		}
		if (vm_page_wired(p)) {
wired:
			if ((options & OBJPR_NOTMAPPED) == 0 &&
			    object->ref_count != 0)
				pmap_remove_all(p);
			if ((options & OBJPR_CLEANONLY) == 0) {
				vm_page_invalid(p);
				vm_page_undirty(p);
			}
			vm_page_xunbusy(p);
			continue;
		}
		KASSERT((p->flags & PG_FICTITIOUS) == 0,
		    ("vm_object_page_remove: page %p is fictitious", p));
		if ((options & OBJPR_CLEANONLY) != 0 &&
		    !vm_page_none_valid(p)) {
			if ((options & OBJPR_NOTMAPPED) == 0 &&
			    object->ref_count != 0 &&
			    !vm_page_try_remove_write(p))
				goto wired;
			if (p->dirty != 0) {
				vm_page_xunbusy(p);
				continue;
			}
		}
		if ((options & OBJPR_NOTMAPPED) == 0 &&
		    object->ref_count != 0 && !vm_page_try_remove_all(p))
			goto wired;
		vm_page_free(p);
	}
	vm_object_pip_wakeup(object);

	vm_pager_freespace(object, start, (end == 0 ? object->size : end) -
	    start);
}

/*
 *	vm_object_page_noreuse:
 *
 *	For the given object, attempt to move the specified pages to
 *	the head of the inactive queue.  This bypasses regular LRU
 *	operation and allows the pages to be reused quickly under memory
 *	pressure.  If a page is wired for any reason, then it will not
 *	be queued.  Pages are specified by the range ["start", "end").
 *	As a special case, if "end" is zero, then the range extends from
 *	"start" to the end of the object.
 *
 *	This operation should only be performed on objects that
 *	contain non-fictitious, managed pages.
 *
 *	The object must be locked.
 */
void
vm_object_page_noreuse(vm_object_t object, vm_pindex_t start, vm_pindex_t end)
{
	vm_page_t p, next;

	VM_OBJECT_ASSERT_LOCKED(object);
	KASSERT((object->flags & (OBJ_FICTITIOUS | OBJ_UNMANAGED)) == 0,
	    ("vm_object_page_noreuse: illegal object %p", object));
	if (object->resident_page_count == 0)
		return;
	p = vm_page_find_least(object, start);

	/*
	 * Here, the variable "p" is either (1) the page with the least pindex
	 * greater than or equal to the parameter "start" or (2) NULL. 
	 */
	for (; p != NULL && (p->pindex < end || end == 0); p = next) {
		next = TAILQ_NEXT(p, listq);
		vm_page_deactivate_noreuse(p);
	}
}

/*
 *	Populate the specified range of the object with valid pages.  Returns
 *	TRUE if the range is successfully populated and FALSE otherwise.
 *
 *	Note: This function should be optimized to pass a larger array of
 *	pages to vm_pager_get_pages() before it is applied to a non-
 *	OBJT_DEVICE object.
 *
 *	The object must be locked.
 */
boolean_t
vm_object_populate(vm_object_t object, vm_pindex_t start, vm_pindex_t end)
{
	vm_page_t m;
	vm_pindex_t pindex;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
	for (pindex = start; pindex < end; pindex++) {
		rv = vm_page_grab_valid(&m, object, pindex, VM_ALLOC_NORMAL);
		if (rv != VM_PAGER_OK)
			break;

		/*
		 * Keep "m" busy because a subsequent iteration may unlock
		 * the object.
		 */
	}
	if (pindex > start) {
		m = vm_page_lookup(object, start);
		while (m != NULL && m->pindex < pindex) {
			vm_page_xunbusy(m);
			m = TAILQ_NEXT(m, listq);
		}
	}
	return (pindex == end);
}

/*
 *	Routine:	vm_object_coalesce
 *	Function:	Coalesces two objects backing up adjoining
 *			regions of memory into a single object.
 *
 *	returns TRUE if objects were combined.
 *
 *	NOTE:	Only works at the moment if the second object is NULL -
 *		if it's not, which object do we lock first?
 *
 *	Parameters:
 *		prev_object	First object to coalesce
 *		prev_offset	Offset into prev_object
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to the second object
 *		reserved	Indicator that extension region has
 *				swap accounted for
 *
 *	Conditions:
 *	The object must *not* be locked.
 */
boolean_t
vm_object_coalesce(vm_object_t prev_object, vm_ooffset_t prev_offset,
    vm_size_t prev_size, vm_size_t next_size, boolean_t reserved)
{
	vm_pindex_t next_pindex;

	if (prev_object == NULL)
		return (TRUE);
	if ((prev_object->flags & OBJ_ANON) == 0)
		return (FALSE);

	VM_OBJECT_WLOCK(prev_object);
	/*
	 * Try to collapse the object first.
	 */
	vm_object_collapse(prev_object);

	/*
	 * Can't coalesce if: . more than one reference . paged out . shadows
	 * another object . has a copy elsewhere (any of which mean that the
	 * pages not mapped to prev_entry may be in use anyway)
	 */
	if (prev_object->backing_object != NULL) {
		VM_OBJECT_WUNLOCK(prev_object);
		return (FALSE);
	}

	prev_size >>= PAGE_SHIFT;
	next_size >>= PAGE_SHIFT;
	next_pindex = OFF_TO_IDX(prev_offset) + prev_size;

	if (prev_object->ref_count > 1 &&
	    prev_object->size != next_pindex &&
	    (prev_object->flags & OBJ_ONEMAPPING) == 0) {
		VM_OBJECT_WUNLOCK(prev_object);
		return (FALSE);
	}

	/*
	 * Account for the charge.
	 */
	if (prev_object->cred != NULL) {
		/*
		 * If prev_object was charged, then this mapping,
		 * although not charged now, may become writable
		 * later. Non-NULL cred in the object would prevent
		 * swap reservation during enabling of the write
		 * access, so reserve swap now. Failed reservation
		 * cause allocation of the separate object for the map
		 * entry, and swap reservation for this entry is
		 * managed in appropriate time.
		 */
		if (!reserved && !swap_reserve_by_cred(ptoa(next_size),
		    prev_object->cred)) {
			VM_OBJECT_WUNLOCK(prev_object);
			return (FALSE);
		}
		prev_object->charge += ptoa(next_size);
	}

	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */
	if (next_pindex < prev_object->size) {
		vm_object_page_remove(prev_object, next_pindex, next_pindex +
		    next_size, 0);
#if 0
		if (prev_object->cred != NULL) {
			KASSERT(prev_object->charge >=
			    ptoa(prev_object->size - next_pindex),
			    ("object %p overcharged 1 %jx %jx", prev_object,
				(uintmax_t)next_pindex, (uintmax_t)next_size));
			prev_object->charge -= ptoa(prev_object->size -
			    next_pindex);
		}
#endif
	}

	/*
	 * Extend the object if necessary.
	 */
	if (next_pindex + next_size > prev_object->size)
		prev_object->size = next_pindex + next_size;

	VM_OBJECT_WUNLOCK(prev_object);
	return (TRUE);
}

void
vm_object_set_writeable_dirty_(vm_object_t object)
{
	atomic_add_int(&object->generation, 1);
}

bool
vm_object_mightbedirty_(vm_object_t object)
{
	return (object->generation != object->cleangeneration);
}

/*
 *	vm_object_unwire:
 *
 *	For each page offset within the specified range of the given object,
 *	find the highest-level page in the shadow chain and unwire it.  A page
 *	must exist at every page offset, and the highest-level page must be
 *	wired.
 */
void
vm_object_unwire(vm_object_t object, vm_ooffset_t offset, vm_size_t length,
    uint8_t queue)
{
	vm_object_t tobject, t1object;
	vm_page_t m, tm;
	vm_pindex_t end_pindex, pindex, tpindex;
	int depth, locked_depth;

	KASSERT((offset & PAGE_MASK) == 0,
	    ("vm_object_unwire: offset is not page aligned"));
	KASSERT((length & PAGE_MASK) == 0,
	    ("vm_object_unwire: length is not a multiple of PAGE_SIZE"));
	/* The wired count of a fictitious page never changes. */
	if ((object->flags & OBJ_FICTITIOUS) != 0)
		return;
	pindex = OFF_TO_IDX(offset);
	end_pindex = pindex + atop(length);
again:
	locked_depth = 1;
	VM_OBJECT_RLOCK(object);
	m = vm_page_find_least(object, pindex);
	while (pindex < end_pindex) {
		if (m == NULL || pindex < m->pindex) {
			/*
			 * The first object in the shadow chain doesn't
			 * contain a page at the current index.  Therefore,
			 * the page must exist in a backing object.
			 */
			tobject = object;
			tpindex = pindex;
			depth = 0;
			do {
				tpindex +=
				    OFF_TO_IDX(tobject->backing_object_offset);
				tobject = tobject->backing_object;
				KASSERT(tobject != NULL,
				    ("vm_object_unwire: missing page"));
				if ((tobject->flags & OBJ_FICTITIOUS) != 0)
					goto next_page;
				depth++;
				if (depth == locked_depth) {
					locked_depth++;
					VM_OBJECT_RLOCK(tobject);
				}
			} while ((tm = vm_page_lookup(tobject, tpindex)) ==
			    NULL);
		} else {
			tm = m;
			m = TAILQ_NEXT(m, listq);
		}
		if (vm_page_trysbusy(tm) == 0) {
			for (tobject = object; locked_depth >= 1;
			    locked_depth--) {
				t1object = tobject->backing_object;
				if (tm->object != tobject)
					VM_OBJECT_RUNLOCK(tobject);
				tobject = t1object;
			}
			vm_page_busy_sleep(tm, "unwbo", true);
			goto again;
		}
		vm_page_unwire(tm, queue);
		vm_page_sunbusy(tm);
next_page:
		pindex++;
	}
	/* Release the accumulated object locks. */
	for (tobject = object; locked_depth >= 1; locked_depth--) {
		t1object = tobject->backing_object;
		VM_OBJECT_RUNLOCK(tobject);
		tobject = t1object;
	}
}

/*
 * Return the vnode for the given object, or NULL if none exists.
 * For tmpfs objects, the function may return NULL if there is
 * no vnode allocated at the time of the call.
 */
struct vnode *
vm_object_vnode(vm_object_t object)
{
	struct vnode *vp;

	VM_OBJECT_ASSERT_LOCKED(object);
	vm_pager_getvp(object, &vp, NULL);
	return (vp);
}

/*
 * Busy the vm object.  This prevents new pages belonging to the object from
 * becoming busy.  Existing pages persist as busy.  Callers are responsible
 * for checking page state before proceeding.
 */
void
vm_object_busy(vm_object_t obj)
{

	VM_OBJECT_ASSERT_LOCKED(obj);

	blockcount_acquire(&obj->busy, 1);
	/* The fence is required to order loads of page busy. */
	atomic_thread_fence_acq_rel();
}

void
vm_object_unbusy(vm_object_t obj)
{

	blockcount_release(&obj->busy, 1);
}

void
vm_object_busy_wait(vm_object_t obj, const char *wmesg)
{

	VM_OBJECT_ASSERT_UNLOCKED(obj);

	(void)blockcount_sleep(&obj->busy, NULL, wmesg, PVM);
}

static int
vm_object_list_handler(struct sysctl_req *req, bool swap_only)
{
	struct kinfo_vmobject *kvo;
	char *fullpath, *freepath;
	struct vnode *vp;
	struct vattr va;
	vm_object_t obj;
	vm_page_t m;
	u_long sp;
	int count, error;

	if (req->oldptr == NULL) {
		/*
		 * If an old buffer has not been provided, generate an
		 * estimate of the space needed for a subsequent call.
		 */
		mtx_lock(&vm_object_list_mtx);
		count = 0;
		TAILQ_FOREACH(obj, &vm_object_list, object_list) {
			if (obj->type == OBJT_DEAD)
				continue;
			count++;
		}
		mtx_unlock(&vm_object_list_mtx);
		return (SYSCTL_OUT(req, NULL, sizeof(struct kinfo_vmobject) *
		    count * 11 / 10));
	}

	kvo = malloc(sizeof(*kvo), M_TEMP, M_WAITOK);
	error = 0;

	/*
	 * VM objects are type stable and are never removed from the
	 * list once added.  This allows us to safely read obj->object_list
	 * after reacquiring the VM object lock.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(obj, &vm_object_list, object_list) {
		if (obj->type == OBJT_DEAD ||
		    (swap_only && (obj->flags & (OBJ_ANON | OBJ_SWAP)) == 0))
			continue;
		VM_OBJECT_RLOCK(obj);
		if (obj->type == OBJT_DEAD ||
		    (swap_only && (obj->flags & (OBJ_ANON | OBJ_SWAP)) == 0)) {
			VM_OBJECT_RUNLOCK(obj);
			continue;
		}
		mtx_unlock(&vm_object_list_mtx);
		kvo->kvo_size = ptoa(obj->size);
		kvo->kvo_resident = obj->resident_page_count;
		kvo->kvo_ref_count = obj->ref_count;
		kvo->kvo_shadow_count = obj->shadow_count;
		kvo->kvo_memattr = obj->memattr;
		kvo->kvo_active = 0;
		kvo->kvo_inactive = 0;
		if (!swap_only) {
			TAILQ_FOREACH(m, &obj->memq, listq) {
				/*
				 * A page may belong to the object but be
				 * dequeued and set to PQ_NONE while the
				 * object lock is not held.  This makes the
				 * reads of m->queue below racy, and we do not
				 * count pages set to PQ_NONE.  However, this
				 * sysctl is only meant to give an
				 * approximation of the system anyway.
				 */
				if (m->a.queue == PQ_ACTIVE)
					kvo->kvo_active++;
				else if (m->a.queue == PQ_INACTIVE)
					kvo->kvo_inactive++;
			}
		}

		kvo->kvo_vn_fileid = 0;
		kvo->kvo_vn_fsid = 0;
		kvo->kvo_vn_fsid_freebsd11 = 0;
		freepath = NULL;
		fullpath = "";
		vp = NULL;
		kvo->kvo_type = vm_object_kvme_type(obj, swap_only ? NULL : &vp);
		if (vp != NULL) {
			vref(vp);
		} else if ((obj->flags & OBJ_ANON) != 0) {
			MPASS(kvo->kvo_type == KVME_TYPE_DEFAULT ||
			    kvo->kvo_type == KVME_TYPE_SWAP);
			kvo->kvo_me = (uintptr_t)obj;
			/* tmpfs objs are reported as vnodes */
			kvo->kvo_backing_obj = (uintptr_t)obj->backing_object;
			sp = swap_pager_swapped_pages(obj);
			kvo->kvo_swapped = sp > UINT32_MAX ? UINT32_MAX : sp;
		}
		VM_OBJECT_RUNLOCK(obj);
		if (vp != NULL) {
			vn_fullpath(vp, &fullpath, &freepath);
			vn_lock(vp, LK_SHARED | LK_RETRY);
			if (VOP_GETATTR(vp, &va, curthread->td_ucred) == 0) {
				kvo->kvo_vn_fileid = va.va_fileid;
				kvo->kvo_vn_fsid = va.va_fsid;
				kvo->kvo_vn_fsid_freebsd11 = va.va_fsid;
								/* truncate */
			}
			vput(vp);
		}

		strlcpy(kvo->kvo_path, fullpath, sizeof(kvo->kvo_path));
		if (freepath != NULL)
			free(freepath, M_TEMP);

		/* Pack record size down */
		kvo->kvo_structsize = offsetof(struct kinfo_vmobject, kvo_path)
		    + strlen(kvo->kvo_path) + 1;
		kvo->kvo_structsize = roundup(kvo->kvo_structsize,
		    sizeof(uint64_t));
		error = SYSCTL_OUT(req, kvo, kvo->kvo_structsize);
		maybe_yield();
		mtx_lock(&vm_object_list_mtx);
		if (error)
			break;
	}
	mtx_unlock(&vm_object_list_mtx);
	free(kvo, M_TEMP);
	return (error);
}

static int
sysctl_vm_object_list(SYSCTL_HANDLER_ARGS)
{
	return (vm_object_list_handler(req, false));
}

SYSCTL_PROC(_vm, OID_AUTO, objects, CTLTYPE_STRUCT | CTLFLAG_RW | CTLFLAG_SKIP |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_vm_object_list, "S,kinfo_vmobject",
    "List of VM objects");

static int
sysctl_vm_object_list_swap(SYSCTL_HANDLER_ARGS)
{
	return (vm_object_list_handler(req, true));
}

/*
 * This sysctl returns list of the anonymous or swap objects. Intent
 * is to provide stripped optimized list useful to analyze swap use.
 * Since technically non-swap (default) objects participate in the
 * shadow chains, and are converted to swap type as needed by swap
 * pager, we must report them.
 */
SYSCTL_PROC(_vm, OID_AUTO, swap_objects,
    CTLTYPE_STRUCT | CTLFLAG_RW | CTLFLAG_SKIP | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_vm_object_list_swap, "S,kinfo_vmobject",
    "List of swap VM objects");

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <sys/cons.h>

#include <ddb/ddb.h>

static int
_vm_object_in_map(vm_map_t map, vm_object_t object, vm_map_entry_t entry)
{
	vm_map_t tmpm;
	vm_map_entry_t tmpe;
	vm_object_t obj;

	if (map == 0)
		return 0;

	if (entry == 0) {
		VM_MAP_ENTRY_FOREACH(tmpe, map) {
			if (_vm_object_in_map(map, object, tmpe)) {
				return 1;
			}
		}
	} else if (entry->eflags & MAP_ENTRY_IS_SUB_MAP) {
		tmpm = entry->object.sub_map;
		VM_MAP_ENTRY_FOREACH(tmpe, tmpm) {
			if (_vm_object_in_map(tmpm, object, tmpe)) {
				return 1;
			}
		}
	} else if ((obj = entry->object.vm_object) != NULL) {
		for (; obj; obj = obj->backing_object)
			if (obj == object) {
				return 1;
			}
	}
	return 0;
}

static int
vm_object_in_map(vm_object_t object)
{
	struct proc *p;

	/* sx_slock(&allproc_lock); */
	FOREACH_PROC_IN_SYSTEM(p) {
		if (!p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
		if (_vm_object_in_map(&p->p_vmspace->vm_map, object, 0)) {
			/* sx_sunlock(&allproc_lock); */
			return 1;
		}
	}
	/* sx_sunlock(&allproc_lock); */
	if (_vm_object_in_map(kernel_map, object, 0))
		return 1;
	return 0;
}

DB_SHOW_COMMAND(vmochk, vm_object_check)
{
	vm_object_t object;

	/*
	 * make sure that internal objs are in a map somewhere
	 * and none have zero ref counts.
	 */
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		if ((object->flags & OBJ_ANON) != 0) {
			if (object->ref_count == 0) {
				db_printf("vmochk: internal obj has zero ref count: %ld\n",
					(long)object->size);
			}
			if (!vm_object_in_map(object)) {
				db_printf(
			"vmochk: internal obj is not in a map: "
			"ref: %d, size: %lu: 0x%lx, backing_object: %p\n",
				    object->ref_count, (u_long)object->size, 
				    (u_long)object->size,
				    (void *)object->backing_object);
			}
		}
		if (db_pager_quit)
			return;
	}
}

/*
 *	vm_object_print:	[ debug ]
 */
DB_SHOW_COMMAND(object, vm_object_print_static)
{
	/* XXX convert args. */
	vm_object_t object = (vm_object_t)addr;
	boolean_t full = have_addr;

	vm_page_t p;

	/* XXX count is an (unused) arg.  Avoid shadowing it. */
#define	count	was_count

	int count;

	if (object == NULL)
		return;

	db_iprintf(
	    "Object %p: type=%d, size=0x%jx, res=%d, ref=%d, flags=0x%x ruid %d charge %jx\n",
	    object, (int)object->type, (uintmax_t)object->size,
	    object->resident_page_count, object->ref_count, object->flags,
	    object->cred ? object->cred->cr_ruid : -1, (uintmax_t)object->charge);
	db_iprintf(" sref=%d, backing_object(%d)=(%p)+0x%jx\n",
	    object->shadow_count, 
	    object->backing_object ? object->backing_object->ref_count : 0,
	    object->backing_object, (uintmax_t)object->backing_object_offset);

	if (!full)
		return;

	db_indent += 2;
	count = 0;
	TAILQ_FOREACH(p, &object->memq, listq) {
		if (count == 0)
			db_iprintf("memory:=");
		else if (count == 6) {
			db_printf("\n");
			db_iprintf(" ...");
			count = 0;
		} else
			db_printf(",");
		count++;

		db_printf("(off=0x%jx,page=0x%jx)",
		    (uintmax_t)p->pindex, (uintmax_t)VM_PAGE_TO_PHYS(p));

		if (db_pager_quit)
			break;
	}
	if (count != 0)
		db_printf("\n");
	db_indent -= 2;
}

/* XXX. */
#undef count

/* XXX need this non-static entry for calling from vm_map_print. */
void
vm_object_print(
        /* db_expr_t */ long addr,
	boolean_t have_addr,
	/* db_expr_t */ long count,
	char *modif)
{
	vm_object_print_static(addr, have_addr, count, modif);
}

DB_SHOW_COMMAND(vmopag, vm_object_print_pages)
{
	vm_object_t object;
	vm_pindex_t fidx;
	vm_paddr_t pa;
	vm_page_t m, prev_m;
	int rcount, nl, c;

	nl = 0;
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		db_printf("new object: %p\n", (void *)object);
		if (nl > 18) {
			c = cngetc();
			if (c != ' ')
				return;
			nl = 0;
		}
		nl++;
		rcount = 0;
		fidx = 0;
		pa = -1;
		TAILQ_FOREACH(m, &object->memq, listq) {
			if (m->pindex > 128)
				break;
			if ((prev_m = TAILQ_PREV(m, pglist, listq)) != NULL &&
			    prev_m->pindex + 1 != m->pindex) {
				if (rcount) {
					db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
						(long)fidx, rcount, (long)pa);
					if (nl > 18) {
						c = cngetc();
						if (c != ' ')
							return;
						nl = 0;
					}
					nl++;
					rcount = 0;
				}
			}				
			if (rcount &&
				(VM_PAGE_TO_PHYS(m) == pa + rcount * PAGE_SIZE)) {
				++rcount;
				continue;
			}
			if (rcount) {
				db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
					(long)fidx, rcount, (long)pa);
				if (nl > 18) {
					c = cngetc();
					if (c != ' ')
						return;
					nl = 0;
				}
				nl++;
			}
			fidx = m->pindex;
			pa = VM_PAGE_TO_PHYS(m);
			rcount = 1;
		}
		if (rcount) {
			db_printf(" index(%ld)run(%d)pa(0x%lx)\n",
				(long)fidx, rcount, (long)pa);
			if (nl > 18) {
				c = cngetc();
				if (c != ' ')
					return;
				nl = 0;
			}
			nl++;
		}
	}
}
#endif /* DDB */
