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
 *
 * $Id: vm_object.c,v 1.48.2.1 1995/06/14 17:21:58 davidg Exp $
 */

/*
 *	Virtual memory object module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>		/* for curproc, pageproc */
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_kern.h>

static void _vm_object_allocate(vm_size_t, vm_object_t);


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


struct vm_object kernel_object_store;
struct vm_object kmem_object_store;

int vm_object_cache_max;

#define	VM_OBJECT_HASH_COUNT	1021

struct vm_object_hash_head vm_object_hashtable[VM_OBJECT_HASH_COUNT];
#define OBJECT_HASH(pager) ((unsigned long)(pager) % VM_OBJECT_HASH_COUNT)

long object_collapses;
long object_bypasses;

static void
_vm_object_allocate(size, object)
	vm_size_t size;
	register vm_object_t object;
{
	TAILQ_INIT(&object->memq);
	TAILQ_INIT(&object->reverse_shadow_head);

	object->size = size;
	object->ref_count = 1;
	vm_object_lock_init(object);
	object->flags = OBJ_INTERNAL;	/* pager will reset */
	object->paging_in_progress = 0;
	object->resident_page_count = 0;

	object->pager = NULL;
	object->paging_offset = 0;
	object->shadow = NULL;
	object->shadow_offset = (vm_offset_t) 0;
	object->copy = NULL;

	object->last_read = 0;

	simple_lock(&vm_object_list_lock);
	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	vm_object_count++;
	simple_unlock(&vm_object_list_lock);
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init(vm_offset_t nothing)
{
	register int i;

	TAILQ_INIT(&vm_object_cached_list);
	TAILQ_INIT(&vm_object_list);
	vm_object_count = 0;
	simple_lock_init(&vm_cache_lock);
	simple_lock_init(&vm_object_list_lock);

	vm_object_cache_max = 84;
	if (cnt.v_page_count > 1000)
		vm_object_cache_max += (cnt.v_page_count - 1000) / 3;

	for (i = 0; i < VM_OBJECT_HASH_COUNT; i++)
		LIST_INIT(&vm_object_hashtable[i]);

	kernel_object = &kernel_object_store;
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
	    kernel_object);

	kmem_object = &kmem_object_store;
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
	    kmem_object);
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

vm_object_t
vm_object_allocate(size)
	vm_size_t size;
{
	register vm_object_t result;

	result = (vm_object_t)
	    malloc((u_long) sizeof *result, M_VMOBJ, M_WAITOK);


	_vm_object_allocate(size, result);

	return (result);
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
inline void
vm_object_reference(object)
	register vm_object_t object;
{
	if (object == NULL)
		return;

	vm_object_lock(object);
	object->ref_count++;
	vm_object_unlock(object);
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
vm_object_deallocate(object)
	vm_object_t object;
{
	vm_object_t temp;
	vm_pager_t pager;

	while (object != NULL) {

		if (object->ref_count == 0)
			panic("vm_object_deallocate: object deallocated too many times");

		/*
		 * The cache holds a reference (uncounted) to the object; we
		 * must lock it before removing the object.
		 */

		vm_object_cache_lock();

		/*
		 * Lose the reference
		 */
		vm_object_lock(object);

		object->ref_count--;

		if (object->ref_count != 0) {
			if ((object->ref_count == 1) &&
			    (object->flags & OBJ_INTERNAL)) {
				vm_object_t robject;
				robject = object->reverse_shadow_head.tqh_first;
				if ((robject != NULL) &&
				    (robject->flags & OBJ_INTERNAL)) {
					int s;
					robject->ref_count += 2;
					object->ref_count += 2;

					do {
						s = splhigh();
						while (robject->paging_in_progress) {
							robject->flags |= OBJ_PIPWNT;
							tsleep(robject, PVM, "objde1", 0);
						}

						while (object->paging_in_progress) {
							object->flags |= OBJ_PIPWNT;
							tsleep(object, PVM, "objde2", 0);
						}
						splx(s);

					} while( object->paging_in_progress || robject->paging_in_progress);

					object->ref_count -= 2;
					robject->ref_count -= 2;
					if( robject->ref_count == 0) {
						vm_object_unlock(object);
						vm_object_cache_unlock();
						robject->ref_count += 1;
						object = robject;
						continue;
					}
					vm_object_cache_unlock();
					vm_object_unlock(object);
					vm_object_lock(robject);
					vm_object_collapse(robject);
					vm_object_unlock(robject);
					return;
				}
			}
			vm_object_unlock(object);
			/*
			 * If there are still references, then we are done.
			 */
			vm_object_cache_unlock();
			return;
		}

		pager = object->pager;

		if (pager && pager->pg_type == PG_VNODE) {
			vn_pager_t vnp = (vn_pager_t) pager->pg_data;

			vnp->vnp_vp->v_flag &= ~VTEXT;
		}

		/*
		 * See if this object can persist and has some resident
		 * pages.  If so, enter it in the cache.
		 */
		if (object->flags & OBJ_CANPERSIST) {
			if (object->resident_page_count != 0) {
				vm_object_page_clean(object, 0, 0 ,TRUE);
				TAILQ_INSERT_TAIL(&vm_object_cached_list, object,
				    cached_list);
				vm_object_cached++;
				vm_object_cache_unlock();

				vm_object_unlock(object);

				vm_object_cache_trim();
				return;
			} else {
				object->flags &= ~OBJ_CANPERSIST;
			}
		}

		/*
		 * Make sure no one can look us up now.
		 */
		object->flags |= OBJ_DEAD;
		if ((object->flags & OBJ_INTERNAL) == 0)
			vm_object_remove(pager);
		vm_object_cache_unlock();

		temp = object->shadow;
		if (temp)
			TAILQ_REMOVE(&temp->reverse_shadow_head, object, reverse_shadow_list);
		vm_object_terminate(object);
		/* unlocks and deallocates object */
		object = temp;
	}
}

/*
 *	vm_object_terminate actually destroys the specified object, freeing
 *	up all previously used resources.
 *
 *	The object must be locked.
 */
void
vm_object_terminate(object)
	register vm_object_t object;
{
	register vm_page_t p, next;
	vm_object_t shadow_object;
	int s;

	/*
	 * Detach the object from its shadow if we are the shadow's copy.
	 */
	if ((shadow_object = object->shadow) != NULL) {
		vm_object_lock(shadow_object);
		if (shadow_object->copy == object)
			shadow_object->copy = NULL;
		vm_object_unlock(shadow_object);
	}

	/*
	 * wait for the pageout daemon to be done with the object
	 */
	s = splhigh();
	while (object->paging_in_progress) {
		vm_object_unlock(object);
		object->flags |= OBJ_PIPWNT;
		tsleep((caddr_t) object, PVM, "objtrm", 0);
		vm_object_lock(object);
	}
	splx(s);

	if (object->paging_in_progress != 0)
		panic("vm_object_deallocate: pageout in progress");

	/*
	 * Clean and free the pages, as appropriate. All references to the
	 * object are gone, so we don't need to lock it.
	 */
	if (object->pager && (object->pager->pg_type == PG_VNODE)) {
		vn_pager_t vnp = object->pager->pg_data;
		struct vnode *vp;

		vp = vnp->vnp_vp;
		VOP_LOCK(vp);
		(void) _vm_object_page_clean(object, 0, 0, TRUE);
		vinvalbuf(vp, V_SAVE, NOCRED, NULL, 0, 0);
		VOP_UNLOCK(vp);
	}

	/*
	 * Now free the pages. For internal objects, this also removes them
	 * from paging queues.
	 */
	while ((p = object->memq.tqh_first) != NULL) {
		VM_PAGE_CHECK(p);
		vm_page_lock_queues();
		if (p->flags & PG_BUSY)
			printf("vm_object_terminate: freeing busy page\n");
		PAGE_WAKEUP(p);
		vm_page_free(p);
		cnt.v_pfree++;
		vm_page_unlock_queues();
	}
	vm_object_unlock(object);

	/*
	 * Let the pager know object is dead.
	 */
	if (object->pager != NULL)
		vm_pager_deallocate(object->pager);

	simple_lock(&vm_object_list_lock);
	TAILQ_REMOVE(&vm_object_list, object, object_list);
	vm_object_count--;
	simple_unlock(&vm_object_list_lock);

	wakeup(object);

	/*
	 * Free the space for the object.
	 */
	free((caddr_t) object, M_VMOBJ);
}

/*
 *	vm_object_page_clean
 *
 *	Clean all dirty pages in the specified range of object.
 *	Leaves page on whatever queue it is currently on.
 *
 *	Odd semantics: if start == end, we clean everything.
 *
 *	The object must be locked.
 */

void
_vm_object_page_clean(object, start, end, syncio)
	vm_object_t object;
	vm_offset_t start;
	vm_offset_t end;
	boolean_t syncio;
{
	register vm_page_t p;
	register vm_offset_t tstart, tend;
	int pass;
	int pgcount, s;
	int allclean;
	int entireobj;

	if (object->pager == NULL || (object->flags & OBJ_WRITEABLE) == 0)
		return;

	if (start != end) {
		start = trunc_page(start);
		end = round_page(end);
	}

	pass = 0;
startover:
	tstart = start;
	if (end == 0) {
		tend = object->size;
	} else {
		tend = end;
	}
	entireobj = 0;
	if (tstart == 0 && tend == object->size) {
		object->flags &= ~OBJ_WRITEABLE;
		entireobj = 1;
	}
	/*
	 * Wait until potential collapse operation is complete
	 */
	if (object->flags & OBJ_INTERNAL) {
		s = splhigh();
		while (object->paging_in_progress) {
			object->flags |= OBJ_PIPWNT;
			tsleep(object, PVM, "objpcw", 0);
		}
		splx(s);
	}

	pgcount = object->resident_page_count;

	if (pass == 0 &&
	    (pgcount < 128 || pgcount > (object->size / (8 * PAGE_SIZE)))) {
		allclean = 1;
		for(; pgcount && (tstart < tend); tstart += PAGE_SIZE) {
			p = vm_page_lookup(object, tstart);
			if (!p)
				continue;
			--pgcount;
			s = splhigh();
			TAILQ_REMOVE(&object->memq, p, listq);
			TAILQ_INSERT_TAIL(&object->memq, p, listq);
			splx(s);
			if (entireobj)
				vm_page_protect(p, VM_PROT_READ);
			if ((p->flags & (PG_BUSY|PG_CACHE)) || p->busy ||
				p->valid == 0) {
				continue;
			}
			vm_page_test_dirty(p);
			if ((p->valid & p->dirty) != 0) {
				vm_offset_t tincr;
				tincr = vm_pageout_clean(p, VM_PAGEOUT_FORCE);
				if( tincr) {
					pgcount -= (tincr - 1);
					tincr *= PAGE_SIZE;
					tstart += tincr - PAGE_SIZE;
				}
				allclean = 0;
			}
		}
		if (!allclean) {
			pass = 1;
			goto startover;
		}
		return;
	}

	allclean = 1;
	while ((p = object->memq.tqh_first) != NULL && pgcount > 0) {

		if (p->flags & PG_CACHE) {
			goto donext;
		}

		if (entireobj || (p->offset >= tstart && p->offset < tend)) {
			if (entireobj)
				vm_page_protect(p, VM_PROT_READ);

			if (p->valid == 0) {
				goto donext;
			}

			s = splhigh();
			if ((p->flags & PG_BUSY) || p->busy) {
				allclean = 0;
				if (pass > 0) {
					p->flags |= PG_WANTED;
					tsleep(p, PVM, "objpcn", 0);
					splx(s);
					continue;
				} else {
					splx(s);
					goto donext;
				}
			}

			TAILQ_REMOVE(&object->memq, p, listq);
			TAILQ_INSERT_TAIL(&object->memq, p, listq);
			splx(s);

			pgcount--;
			vm_page_test_dirty(p);
			if ((p->valid & p->dirty) != 0) {
				vm_pageout_clean(p, VM_PAGEOUT_FORCE);
				allclean = 0;
			}
			continue;
		}
	donext:
		TAILQ_REMOVE(&object->memq, p, listq);
		TAILQ_INSERT_TAIL(&object->memq, p, listq);
		pgcount--;
	}
	if ((!allclean && (pass == 0)) ||
	    (entireobj && (object->flags & OBJ_WRITEABLE))) {
		pass = 1;
		if (entireobj)
			object->flags &= ~OBJ_WRITEABLE;
		goto startover;
	}
	return;
}


void
vm_object_page_clean(object, start, end, syncio)
	register vm_object_t object;
	register vm_offset_t start;
	register vm_offset_t end;
	boolean_t syncio;
{
	if (object->pager && (object->flags & OBJ_WRITEABLE) &&
		(object->pager->pg_type == PG_VNODE)) {
		vn_pager_t vnp = (vn_pager_t) object->pager->pg_data;
		struct vnode *vp;

		vp = vnp->vnp_vp;
		VOP_LOCK(vp);
		_vm_object_page_clean(object, start, end, syncio);
		VOP_UNLOCK(vp);
	} else {
		_vm_object_page_clean(object, start, end, syncio);
	}
}

void
vm_object_cache_clean()
{
	vm_object_t object;
	vm_object_cache_lock();
	while(1) {
		object = vm_object_cached_list.tqh_first;
		while( object) {
			if( (object->flags & OBJ_WRITEABLE) &&
				object->pager &&
				object->pager->pg_type == PG_VNODE) {
				vm_object_page_clean(object, 0, 0, 0);
				goto loop;
			}
			object = object->cached_list.tqe_next;
		}
		return;
loop:
	}
}

/*
 *	vm_object_deactivate_pages
 *
 *	Deactivate all pages in the specified object.  (Keep its pages
 *	in memory even though it is no longer referenced.)
 *
 *	The object must be locked.
 */
void
vm_object_deactivate_pages(object)
	register vm_object_t object;
{
	register vm_page_t p, next;

	for (p = object->memq.tqh_first; p != NULL; p = next) {
		next = p->listq.tqe_next;
		vm_page_lock_queues();
		vm_page_deactivate(p);
		vm_page_unlock_queues();
	}
}

/*
 *	Trim the object cache to size.
 */
void
vm_object_cache_trim()
{
	register vm_object_t object;

	vm_object_cache_lock();
	while (vm_object_cached > vm_object_cache_max) {
		object = vm_object_cached_list.tqh_first;
		vm_object_cache_unlock();

		if (object != vm_object_lookup(object->pager))
			panic("vm_object_cache_trim: I'm sooo confused.");

		pager_cache(object, FALSE);

		vm_object_cache_lock();
	}
	vm_object_cache_unlock();
}


/*
 *	vm_object_pmap_copy:
 *
 *	Makes all physical pages in the specified
 *	object range copy-on-write.  No writeable
 *	references to these pages should remain.
 *
 *	The object must *not* be locked.
 */
void
vm_object_pmap_copy(object, start, end)
	register vm_object_t object;
	register vm_offset_t start;
	register vm_offset_t end;
{
	register vm_page_t p;

	if (object == NULL)
		return;

	vm_object_lock(object);
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if ((start <= p->offset) && (p->offset < end)) {
			vm_page_protect(p, VM_PROT_READ);
			p->flags |= PG_COPYONWRITE;
		}
	}
	vm_object_unlock(object);
}

/*
 *	vm_object_pmap_remove:
 *
 *	Removes all physical pages in the specified
 *	object range from all physical maps.
 *
 *	The object must *not* be locked.
 */
void
vm_object_pmap_remove(object, start, end)
	register vm_object_t object;
	register vm_offset_t start;
	register vm_offset_t end;
{
	register vm_page_t p;
	int s;

	if (object == NULL)
		return;
	++object->paging_in_progress;

	vm_object_lock(object);
again:
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if ((start <= p->offset) && (p->offset < end)) {
			s = splhigh();
			if ((p->flags & PG_BUSY) || p->busy) {
				p->flags |= PG_WANTED;
				tsleep((caddr_t) p, PVM, "vmopmr", 0);
				splx(s);
				goto again;
			}
			splx(s);
			vm_page_protect(p, VM_PROT_NONE);
		}
	}
	vm_object_unlock(object);
	vm_object_pip_wakeup(object);
}

/*
 *	vm_object_copy:
 *
 *	Create a new object which is a copy of an existing
 *	object, and mark all of the pages in the existing
 *	object 'copy-on-write'.  The new object has one reference.
 *	Returns the new object.
 *
 *	May defer the copy until later if the object is not backed
 *	up by a non-default pager.
 */
void
vm_object_copy(src_object, src_offset, size,
    dst_object, dst_offset, src_needs_copy)
	register vm_object_t src_object;
	vm_offset_t src_offset;
	vm_size_t size;
	vm_object_t *dst_object;/* OUT */
	vm_offset_t *dst_offset;/* OUT */
	boolean_t *src_needs_copy;	/* OUT */
{
	register vm_object_t new_copy;
	register vm_object_t old_copy;
	vm_offset_t new_start, new_end;

	register vm_page_t p;

	if (src_object == NULL) {
		/*
		 * Nothing to copy
		 */
		*dst_object = NULL;
		*dst_offset = 0;
		*src_needs_copy = FALSE;
		return;
	}
	/*
	 * If the object's pager is null_pager or the default pager, we don't
	 * have to make a copy of it.  Instead, we set the needs copy flag and
	 * make a shadow later.
	 */

	vm_object_lock(src_object);

	/*
	 * Try to collapse the object before copying it.
	 */

	vm_object_collapse(src_object);

	if (src_object->pager == NULL ||
	    (src_object->flags & OBJ_INTERNAL)) {

		/*
		 * Make another reference to the object
		 */
		src_object->ref_count++;

		/*
		 * Mark all of the pages copy-on-write.
		 */
		for (p = src_object->memq.tqh_first; p; p = p->listq.tqe_next)
			if (src_offset <= p->offset &&
			    p->offset < src_offset + size)
				p->flags |= PG_COPYONWRITE;
		vm_object_unlock(src_object);

		*dst_object = src_object;
		*dst_offset = src_offset;

		/*
		 * Must make a shadow when write is desired
		 */
		*src_needs_copy = TRUE;
		return;
	}
	/*
	 * If the object has a pager, the pager wants to see all of the
	 * changes.  We need a copy-object for the changed pages.
	 *
	 * If there is a copy-object, and it is empty, no changes have been made
	 * to the object since the copy-object was made.  We can use the same
	 * copy- object.
	 */

Retry1:
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 * Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);

			/* should spin a bit here... */
			tsleep((caddr_t) old_copy, PVM, "cpylck", 1);
			vm_object_lock(src_object);
			goto Retry1;
		}
		if (old_copy->resident_page_count == 0 &&
		    old_copy->pager == NULL) {
			/*
			 * Return another reference to the existing
			 * copy-object.
			 */
			old_copy->ref_count++;
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);
			*dst_object = old_copy;
			*dst_offset = src_offset;
			*src_needs_copy = FALSE;
			return;
		}
		vm_object_unlock(old_copy);
	}
	vm_object_unlock(src_object);

	/*
	 * If the object has a pager, the pager wants to see all of the
	 * changes.  We must make a copy-object and put the changed pages
	 * there.
	 *
	 * The copy-object is always made large enough to completely shadow the
	 * original object, since it may have several users who want to shadow
	 * the original object at different points.
	 */

	new_copy = vm_object_allocate(src_object->size);

Retry2:
	vm_object_lock(src_object);
	/*
	 * Copy object may have changed while we were unlocked
	 */
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 * Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);
			tsleep((caddr_t) old_copy, PVM, "cpylck", 1);
			goto Retry2;
		}
		/*
		 * Consistency check
		 */
		if (old_copy->shadow != src_object ||
		    old_copy->shadow_offset != (vm_offset_t) 0)
			panic("vm_object_copy: copy/shadow inconsistency");

		/*
		 * Make the old copy-object shadow the new one. It will
		 * receive no more pages from the original object.
		 */

		src_object->ref_count--;	/* remove ref. from old_copy */
		if (old_copy->shadow)
			TAILQ_REMOVE(&old_copy->shadow->reverse_shadow_head, old_copy, reverse_shadow_list);
		old_copy->shadow = new_copy;
		TAILQ_INSERT_TAIL(&old_copy->shadow->reverse_shadow_head, old_copy, reverse_shadow_list);
		new_copy->ref_count++;	/* locking not needed - we have the
					 * only pointer */
		vm_object_unlock(old_copy);	/* done with old_copy */
	}
	new_start = (vm_offset_t) 0;	/* always shadow original at 0 */
	new_end = (vm_offset_t) new_copy->size;	/* for the whole object */

	/*
	 * Point the new copy at the existing object.
	 */

	new_copy->shadow = src_object;
	TAILQ_INSERT_TAIL(&new_copy->shadow->reverse_shadow_head, new_copy, reverse_shadow_list);
	new_copy->shadow_offset = new_start;
	src_object->ref_count++;
	src_object->copy = new_copy;

	/*
	 * Mark all the affected pages of the existing object copy-on-write.
	 */
	for (p = src_object->memq.tqh_first; p != NULL; p = p->listq.tqe_next)
		if ((new_start <= p->offset) && (p->offset < new_end))
			p->flags |= PG_COPYONWRITE;

	vm_object_unlock(src_object);

	*dst_object = new_copy;
	*dst_offset = src_offset - new_start;
	*src_needs_copy = FALSE;
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
vm_object_shadow(object, offset, length)
	vm_object_t *object;	/* IN/OUT */
	vm_offset_t *offset;	/* IN/OUT */
	vm_size_t length;
{
	register vm_object_t source;
	register vm_object_t result;

	source = *object;

	/*
	 * Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(length)) == NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 * The new object shadows the source object, adding a reference to it.
	 * Our caller changes his reference to point to the new object,
	 * removing a reference to the source object.  Net result: no change
	 * of reference count.
	 */
	result->shadow = source;
	if (source)
		TAILQ_INSERT_TAIL(&result->shadow->reverse_shadow_head, result, reverse_shadow_list);

	/*
	 * Store the offset into the source object, and fix up the offset into
	 * the new object.
	 */

	result->shadow_offset = *offset;

	/*
	 * Return the new things
	 */

	*offset = 0;
	*object = result;
}


/*
 *	vm_object_lookup looks in the object cache for an object with the
 *	specified pager and paging id.
 */

vm_object_t
vm_object_lookup(pager)
	vm_pager_t pager;
{
	register vm_object_hash_entry_t entry;
	vm_object_t object;

	vm_object_cache_lock();

	for (entry = vm_object_hashtable[OBJECT_HASH(pager)].lh_first;
	    entry != NULL; entry = entry->hash_links.le_next) {
		object = entry->object;
		if (object->pager == pager) {
			vm_object_lock(object);
			if (object->ref_count == 0) {
				TAILQ_REMOVE(&vm_object_cached_list, object,
				    cached_list);
				vm_object_cached--;
			}
			object->ref_count++;
			vm_object_unlock(object);
			vm_object_cache_unlock();
			return (object);
		}
	}

	vm_object_cache_unlock();
	return (NULL);
}

/*
 *	vm_object_enter enters the specified object/pager/id into
 *	the hash table.
 */

void
vm_object_enter(object, pager)
	vm_object_t object;
	vm_pager_t pager;
{
	struct vm_object_hash_head *bucket;
	register vm_object_hash_entry_t entry;

	/*
	 * We don't cache null objects, and we can't cache objects with the
	 * null pager.
	 */

	if (object == NULL)
		return;
	if (pager == NULL)
		return;

	bucket = &vm_object_hashtable[OBJECT_HASH(pager)];
	entry = (vm_object_hash_entry_t)
	    malloc((u_long) sizeof *entry, M_VMOBJHASH, M_WAITOK);
	entry->object = object;

	vm_object_cache_lock();
	LIST_INSERT_HEAD(bucket, entry, hash_links);
	vm_object_cache_unlock();
}

/*
 *	vm_object_remove:
 *
 *	Remove the pager from the hash table.
 *	Note:  This assumes that the object cache
 *	is locked.  XXX this should be fixed
 *	by reorganizing vm_object_deallocate.
 */
void
vm_object_remove(pager)
	register vm_pager_t pager;
{
	struct vm_object_hash_head *bucket;
	register vm_object_hash_entry_t entry;
	register vm_object_t object;

	bucket = &vm_object_hashtable[OBJECT_HASH(pager)];

	for (entry = bucket->lh_first;
	    entry != NULL; entry = entry->hash_links.le_next) {
		object = entry->object;
		if (object->pager == pager) {
			LIST_REMOVE(entry, hash_links);
			free((caddr_t) entry, M_VMOBJHASH);
			break;
		}
	}
}

/*
 * this version of collapse allows the operation to occur earlier and
 * when paging_in_progress is true for an object...  This is not a complete
 * operation, but should plug 99.9% of the rest of the leaks.
 */
static void
vm_object_qcollapse(object)
	register vm_object_t object;
{
	register vm_object_t backing_object;
	register vm_offset_t backing_offset, new_offset;
	register vm_page_t p, pp;
	register vm_size_t size;

	backing_object = object->shadow;
	if (backing_object->shadow != NULL &&
	    backing_object->shadow->copy == backing_object)
		return;
	if (backing_object->ref_count != 1)
		return;

	backing_object->ref_count += 2;

	backing_offset = object->shadow_offset;
	size = object->size;
	p = backing_object->memq.tqh_first;
	while (p) {
		vm_page_t next;

		next = p->listq.tqe_next;
		if ((p->flags & (PG_BUSY | PG_FICTITIOUS | PG_CACHE)) ||
		    !p->valid || p->hold_count || p->wire_count || p->busy) {
			p = next;
			continue;
		}
		vm_page_protect(p, VM_PROT_NONE);
		new_offset = (p->offset - backing_offset);
		if (p->offset < backing_offset ||
		    new_offset >= size) {
			if (backing_object->pager)
				swap_pager_freespace(backing_object->pager,
				    backing_object->paging_offset + p->offset, PAGE_SIZE);
			vm_page_lock_queues();
			vm_page_free(p);
			vm_page_unlock_queues();
		} else {
			pp = vm_page_lookup(object, new_offset);
			if (pp != NULL || (object->pager && vm_pager_has_page(object->pager,
				    object->paging_offset + new_offset))) {
				if (backing_object->pager)
					swap_pager_freespace(backing_object->pager,
					    backing_object->paging_offset + p->offset, PAGE_SIZE);
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
			} else {
				if( backing_object->pager)
					swap_pager_freespace(backing_object->pager,
					    backing_object->paging_offset + p->offset, PAGE_SIZE);
				vm_page_rename(p, object, new_offset);
				p->dirty = VM_PAGE_BITS_ALL;
			}
		}
		p = next;
	}
	backing_object->ref_count -= 2;
}

boolean_t vm_object_collapse_allowed = TRUE;

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 *
 *	Requires that the object be locked and the page
 *	queues be unlocked.
 *
 *	This routine has significant changes by John S. Dyson
 *	to fix some swap memory leaks.  18 Dec 93
 *
 */
void
vm_object_collapse(object)
	register vm_object_t object;

{
	register vm_object_t backing_object;
	register vm_offset_t backing_offset;
	register vm_size_t size;
	register vm_offset_t new_offset;
	register vm_page_t p, pp;

	if (!vm_object_collapse_allowed)
		return;

	while (TRUE) {
		/*
		 * Verify that the conditions are right for collapse:
		 *
		 * The object exists and no pages in it are currently being paged
		 * out.
		 */
		if (object == NULL)
			return;

		/*
		 * Make sure there is a backing object.
		 */
		if ((backing_object = object->shadow) == NULL)
			return;

		/*
		 * we check the backing object first, because it is most likely
		 * !OBJ_INTERNAL.
		 */
		if ((backing_object->flags & OBJ_INTERNAL) == 0 ||
		    (backing_object->flags & OBJ_DEAD) ||
		    (object->flags & OBJ_INTERNAL) == 0 ||
		    (object->flags & OBJ_DEAD))
			return;

		if (object->paging_in_progress != 0 ||
		    backing_object->paging_in_progress != 0) {
			if (vm_object_lock_try(backing_object)) {
				vm_object_qcollapse(object);
				vm_object_unlock(backing_object);
			}
			return;
		}

		vm_object_lock(backing_object);

		/*
		 * The backing object can't be a copy-object: the
		 * shadow_offset for the copy-object must stay as 0.
		 * Furthermore (for the 'we have all the pages' case), if we
		 * bypass backing_object and just shadow the next object in
		 * the chain, old pages from that object would then have to be
		 * copied BOTH into the (former) backing_object and into the
		 * parent object.
		 */
		if (backing_object->shadow != NULL &&
		    backing_object->shadow->copy == backing_object) {
			vm_object_unlock(backing_object);
			return;
		}

		/*
		 * We know that we can either collapse the backing object (if
		 * the parent is the only reference to it) or (perhaps) remove
		 * the parent's reference to it.
		 */

		backing_offset = object->shadow_offset;
		size = object->size;

		/*
		 * If there is exactly one reference to the backing object, we
		 * can collapse it into the parent.
		 */

		if (backing_object->ref_count == 1) {

			backing_object->flags |= OBJ_DEAD;
			/*
			 * We can collapse the backing object.
			 *
			 * Move all in-memory pages from backing_object to the
			 * parent.  Pages that have been paged out will be
			 * overwritten by any of the parent's pages that
			 * shadow them.
			 */

			while ((p = backing_object->memq.tqh_first) != 0) {

				new_offset = (p->offset - backing_offset);

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, dispose of
				 * it.
				 *
				 * Otherwise, move it as planned.
				 */

				if (p->offset < backing_offset ||
				    new_offset >= size) {
					vm_page_lock_queues();
					vm_page_protect(p, VM_PROT_NONE);
					PAGE_WAKEUP(p);
					vm_page_free(p);
					vm_page_unlock_queues();
				} else {
					pp = vm_page_lookup(object, new_offset);
					if (pp != NULL || (object->pager && vm_pager_has_page(object->pager,
					    object->paging_offset + new_offset))) {
						vm_page_lock_queues();
						vm_page_protect(p, VM_PROT_NONE);
						PAGE_WAKEUP(p);
						vm_page_free(p);
						vm_page_unlock_queues();
					} else {
						vm_page_rename(p, object, new_offset);
					}
				}
			}

			/*
			 * Move the pager from backing_object to object.
			 */

			if (backing_object->pager) {
				backing_object->paging_in_progress++;
				if (object->pager) {
					vm_pager_t bopager;

					object->paging_in_progress++;
					/*
					 * copy shadow object pages into ours
					 * and destroy unneeded pages in
					 * shadow object.
					 */
					bopager = backing_object->pager;
					backing_object->pager = NULL;
					swap_pager_copy(
					    bopager, backing_object->paging_offset,
					    object->pager, object->paging_offset,
					    object->shadow_offset);
					vm_object_pip_wakeup(object);
				} else {
					object->paging_in_progress++;
					/*
					 * grab the shadow objects pager
					 */
					object->pager = backing_object->pager;
					object->paging_offset = backing_object->paging_offset + backing_offset;
					backing_object->pager = NULL;
					/*
					 * free unnecessary blocks
					 */
					swap_pager_freespace(object->pager, 0, object->paging_offset);
					vm_object_pip_wakeup(object);
				}

				vm_object_pip_wakeup(backing_object);
			}
			/*
			 * Object now shadows whatever backing_object did.
			 * Note that the reference to backing_object->shadow
			 * moves from within backing_object to within object.
			 */

			TAILQ_REMOVE(&object->shadow->reverse_shadow_head, object,
			    reverse_shadow_list);
			if (backing_object->shadow)
				TAILQ_REMOVE(&backing_object->shadow->reverse_shadow_head,
				    backing_object, reverse_shadow_list);
			object->shadow = backing_object->shadow;
			if (object->shadow)
				TAILQ_INSERT_TAIL(&object->shadow->reverse_shadow_head,
				    object, reverse_shadow_list);

			object->shadow_offset += backing_object->shadow_offset;
			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */

			vm_object_unlock(backing_object);

			simple_lock(&vm_object_list_lock);
			TAILQ_REMOVE(&vm_object_list, backing_object,
			    object_list);
			vm_object_count--;
			simple_unlock(&vm_object_list_lock);

			free((caddr_t) backing_object, M_VMOBJ);

			object_collapses++;
		} else {
			/*
			 * If all of the pages in the backing object are
			 * shadowed by the parent object, the parent object no
			 * longer has to shadow the backing object; it can
			 * shadow the next one in the chain.
			 *
			 * The backing object must not be paged out - we'd have
			 * to check all of the paged-out pages, as well.
			 */

			if (backing_object->pager != NULL) {
				vm_object_unlock(backing_object);
				return;
			}
			/*
			 * Should have a check for a 'small' number of pages
			 * here.
			 */

			for (p = backing_object->memq.tqh_first; p; p = p->listq.tqe_next) {
				new_offset = (p->offset - backing_offset);

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, keep going.
				 *
				 * Otherwise, the backing_object must be left in
				 * the chain.
				 */

				if (p->offset >= backing_offset &&
				    new_offset <= size &&
				    ((pp = vm_page_lookup(object, new_offset)) == NULL ||
					!pp->valid) &&
				    (!object->pager || !vm_pager_has_page(object->pager, object->paging_offset + new_offset))) {
					/*
					 * Page still needed. Can't go any
					 * further.
					 */
					vm_object_unlock(backing_object);
					return;
				}
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */

			TAILQ_REMOVE(&object->shadow->reverse_shadow_head,
			    object, reverse_shadow_list);
			vm_object_reference(object->shadow = backing_object->shadow);
			if (object->shadow)
				TAILQ_INSERT_TAIL(&object->shadow->reverse_shadow_head,
				    object, reverse_shadow_list);
			object->shadow_offset += backing_object->shadow_offset;

			/*
			 * Backing object might have had a copy pointer to us.
			 * If it did, clear it.
			 */
			if (backing_object->copy == object) {
				backing_object->copy = NULL;
			}
			/*
			 * Drop the reference count on backing_object. Since
			 * its ref_count was at least 2, it will not vanish;
			 * so we don't need to call vm_object_deallocate.
			 */
			if (backing_object->ref_count == 1)
				printf("should have called obj deallocate\n");
			backing_object->ref_count--;
			vm_object_unlock(backing_object);

			object_bypasses++;

		}

		/*
		 * Try again with this object's new backing object.
		 */
	}
}

/*
 *	vm_object_page_remove: [internal]
 *
 *	Removes all physical pages in the specified
 *	object range from the object's list of pages.
 *
 *	The object must be locked.
 */
void
vm_object_page_remove(object, start, end, clean_only)
	register vm_object_t object;
	register vm_offset_t start;
	register vm_offset_t end;
	boolean_t clean_only;
{
	register vm_page_t p, next;
	vm_offset_t size;
	int s;

	if (object == NULL)
		return;

	object->paging_in_progress++;
	start = trunc_page(start);
	end = round_page(end);
again:
	size = end - start;
	if (size > 4 * PAGE_SIZE || size >= object->size / 4) {
		for (p = object->memq.tqh_first; p != NULL; p = next) {
			next = p->listq.tqe_next;
			if ((start <= p->offset) && (p->offset < end)) {
				s = splhigh();
				if (p->bmapped) {
					splx(s);
					continue;
				}
				if ((p->flags & PG_BUSY) || p->busy) {
					p->flags |= PG_WANTED;
					tsleep((caddr_t) p, PVM, "vmopar", 0);
					splx(s);
					goto again;
				}
				splx(s);
				if (clean_only) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty)
						continue;
				}
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_lock_queues();
				PAGE_WAKEUP(p);
				vm_page_free(p);
				vm_page_unlock_queues();
			}
		}
	} else {
		while (size > 0) {
			while ((p = vm_page_lookup(object, start)) != 0) {
				s = splhigh();
				if (p->bmapped) {
					splx(s);
					break;
				}
				if ((p->flags & PG_BUSY) || p->busy) {
					p->flags |= PG_WANTED;
					tsleep((caddr_t) p, PVM, "vmopar", 0);
					splx(s);
					goto again;
				}
				splx(s);
				if (clean_only) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty)
						continue;
				}
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_lock_queues();
				PAGE_WAKEUP(p);
				vm_page_free(p);
				vm_page_unlock_queues();
			}
			start += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}
	vm_object_pip_wakeup(object);
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
 *		next_object	Second object into coalesce
 *		next_offset	Offset into next_object
 *
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to next_object
 *
 *	Conditions:
 *	The object must *not* be locked.
 */
boolean_t
vm_object_coalesce(prev_object, next_object,
    prev_offset, next_offset,
    prev_size, next_size)
	register vm_object_t prev_object;
	vm_object_t next_object;
	vm_offset_t prev_offset, next_offset;
	vm_size_t prev_size, next_size;
{
	vm_size_t newsize;

	if (next_object != NULL) {
		return (FALSE);
	}
	if (prev_object == NULL) {
		return (TRUE);
	}
	vm_object_lock(prev_object);

	/*
	 * Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 * Can't coalesce if: . more than one reference . paged out . shadows
	 * another object . has a copy elsewhere (any of which mean that the
	 * pages not mapped to prev_entry may be in use anyway)
	 */

	if (prev_object->ref_count > 1 ||
	    prev_object->pager != NULL ||
	    prev_object->shadow != NULL ||
	    prev_object->copy != NULL) {
		vm_object_unlock(prev_object);
		return (FALSE);
	}
	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */

	vm_object_page_remove(prev_object,
	    prev_offset + prev_size,
	    prev_offset + prev_size + next_size, FALSE);

	/*
	 * Extend the object if necessary.
	 */
	newsize = prev_offset + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	vm_object_unlock(prev_object);
	return (TRUE);
}

/*
 * returns page after looking up in shadow chain
 */

vm_page_t
vm_object_page_lookup(object, offset)
	vm_object_t object;
	vm_offset_t offset;
{
	vm_page_t m;

	if (!(m = vm_page_lookup(object, offset))) {
		if (!object->shadow)
			return 0;
		else
			return vm_object_page_lookup(object->shadow, offset + object->shadow_offset);
	}
	return m;
}

#ifdef DDB

int
_vm_object_in_map(map, object, entry)
	vm_map_t map;
	vm_object_t object;
	vm_map_entry_t entry;
{
	vm_map_t tmpm;
	vm_map_entry_t tmpe;
	vm_object_t obj;
	int entcount;

	if (map == 0)
		return 0;

	if (entry == 0) {
		tmpe = map->header.next;
		entcount = map->nentries;
		while (entcount-- && (tmpe != &map->header)) {
			if( _vm_object_in_map(map, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if (entry->is_sub_map || entry->is_a_map) {
		tmpm = entry->object.share_map;
		tmpe = tmpm->header.next;
		entcount = tmpm->nentries;
		while (entcount-- && tmpe != &tmpm->header) {
			if( _vm_object_in_map(tmpm, object, tmpe)) {
				return 1;
			}
			tmpe = tmpe->next;
		}
	} else if (obj = entry->object.vm_object) {
		for(; obj; obj=obj->shadow)
			if( obj == object) {
				return 1;
			}
	}
	return 0;
}

int
vm_object_in_map( object)
	vm_object_t object;
{
	struct proc *p;
	for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
		if( !p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
/*
		if (p->p_stat != SRUN && p->p_stat != SSLEEP) {
			continue;
		}
*/
		if( _vm_object_in_map(&p->p_vmspace->vm_map, object, 0))
			return 1;
	}
	if( _vm_object_in_map( kernel_map, object, 0))
		return 1;
	if( _vm_object_in_map( kmem_map, object, 0))
		return 1;
	if( _vm_object_in_map( pager_map, object, 0))
		return 1;
	if( _vm_object_in_map( buffer_map, object, 0))
		return 1;
	if( _vm_object_in_map( io_map, object, 0))
		return 1;
	if( _vm_object_in_map( phys_map, object, 0))
		return 1;
	if( _vm_object_in_map( mb_map, object, 0))
		return 1;
	if( _vm_object_in_map( u_map, object, 0))
		return 1;
	return 0;
}


void
vm_object_check() {
	int i;
	int maxhash = 0;
	vm_object_t object;
	vm_object_hash_entry_t entry;

	/*
	 * make sure that no internal objs are hashed
	 */
	for (i=0; i<VM_OBJECT_HASH_COUNT;i++) {
		int lsize = 0;
		for (entry = vm_object_hashtable[i].lh_first;
		    entry != NULL; entry = entry->hash_links.le_next) {
			if( entry->object->flags & OBJ_INTERNAL) {
				printf("vmochk: internal obj on hash: size: %d\n", entry->object->size);
			}
			++lsize;
		}
		if( lsize > maxhash)
			maxhash = lsize;
	}

	printf("maximum object hash queue size: %d\n",  maxhash);

	/*
	 * make sure that internal objs are in a map somewhere
	 * and none have zero ref counts.
	 */
	for (object = vm_object_list.tqh_first;
			object != NULL;
			object = object->object_list.tqe_next) {
		if (object->flags & OBJ_INTERNAL) {
			if (object->ref_count == 0) {
				printf("vmochk: internal obj has zero ref count: %d\n",
					object->size);
			}
			if (!vm_object_in_map(object)) {
				printf("vmochk: internal obj is not in a map: ref: %d, size: %d, pager: 0x%x, shadow: 0x%x\n",
				    object->ref_count, object->size, object->pager, object->shadow);
			}
		}
	}
}

/*
 *	vm_object_print:	[ debug ]
 */
void
vm_object_print(object, full)
	vm_object_t object;
	boolean_t full;
{
	register vm_page_t p;

	register int count;

	if (object == NULL)
		return;

	iprintf("Object 0x%x: size=0x%x, res=%d, ref=%d, ",
	    (int) object, (int) object->size,
	    object->resident_page_count, object->ref_count);
	printf("pager=0x%x+0x%x, shadow=(0x%x)+0x%x\n",
	    (int) object->pager, (int) object->paging_offset,
	    (int) object->shadow, (int) object->shadow_offset);
	printf("cache: next=%p, prev=%p\n",
	    object->cached_list.tqe_next, object->cached_list.tqe_prev);

	if (!full)
		return;

	indent += 2;
	count = 0;
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if (count == 0)
			iprintf("memory:=");
		else if (count == 6) {
			printf("\n");
			iprintf(" ...");
			count = 0;
		} else
			printf(",");
		count++;

		printf("(off=0x%lx,page=0x%lx)",
		    (u_long) p->offset, (u_long) VM_PAGE_TO_PHYS(p));
	}
	if (count != 0)
		printf("\n");
	indent -= 2;
}
#endif /* DDB */
