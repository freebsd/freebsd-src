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
 * $Id: vm_object.c,v 1.76 1996/06/16 20:37:30 dyson Exp $
 */

/*
 *	Virtual memory object module.
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>		/* for curproc, pageproc */
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#ifdef DDB
static void	DDB_vm_object_check __P((void));
#endif

static void	_vm_object_allocate __P((objtype_t, vm_size_t, vm_object_t));
#ifdef DDB
static int	_vm_object_in_map __P((vm_map_t map, vm_object_t object,
				       vm_map_entry_t entry));
static int	vm_object_in_map __P((vm_object_t object));
#endif
static void	vm_object_qcollapse __P((vm_object_t object));
#ifdef not_used
static void	vm_object_deactivate_pages __P((vm_object_t));
#endif
static void	vm_object_terminate __P((vm_object_t));
static void	vm_object_cache_trim __P((void));

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

int vm_object_cache_max;
struct object_q vm_object_cached_list;
static int vm_object_cached;
struct object_q vm_object_list;
static long vm_object_count;
vm_object_t kernel_object;
vm_object_t kmem_object;
static struct vm_object kernel_object_store;
static struct vm_object kmem_object_store;
extern int vm_pageout_page_count;

static long object_collapses;
static long object_bypasses;

static void
_vm_object_allocate(type, size, object)
	objtype_t type;
	vm_size_t size;
	register vm_object_t object;
{
	TAILQ_INIT(&object->memq);
	TAILQ_INIT(&object->shadow_head);

	object->type = type;
	object->size = size;
	object->ref_count = 1;
	object->flags = 0;
	object->behavior = OBJ_NORMAL;
	object->paging_in_progress = 0;
	object->resident_page_count = 0;
	object->shadow_count = 0;
	object->handle = NULL;
	object->paging_offset = (vm_ooffset_t) 0;
	object->backing_object = NULL;
	object->backing_object_offset = (vm_ooffset_t) 0;

	object->last_read = 0;

	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	vm_object_count++;
}

/*
 *	vm_object_init:
 *
 *	Initialize the VM objects module.
 */
void
vm_object_init()
{
	TAILQ_INIT(&vm_object_cached_list);
	TAILQ_INIT(&vm_object_list);
	vm_object_count = 0;
	
	vm_object_cache_max = 84;
	if (cnt.v_page_count > 1000)
		vm_object_cache_max += (cnt.v_page_count - 1000) / 4;

	kernel_object = &kernel_object_store;
	_vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kernel_object);

	kmem_object = &kmem_object_store;
	_vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS),
	    kmem_object);
}

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

vm_object_t
vm_object_allocate(type, size)
	objtype_t type;
	vm_size_t size;
{
	register vm_object_t result;

	result = (vm_object_t)
	    malloc((u_long) sizeof *result, M_VMOBJ, M_WAITOK);

	_vm_object_allocate(type, size, result);

	return (result);
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
void
vm_object_reference(object)
	register vm_object_t object;
{
	if (object == NULL)
		return;

	if (object->ref_count == 0) {
		if ((object->flags & OBJ_CANPERSIST) == 0)
			panic("vm_object_reference: non-persistent object with 0 ref_count");
		TAILQ_REMOVE(&vm_object_cached_list, object, cached_list);
		vm_object_cached--;
	}
	object->ref_count++;
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

	while (object != NULL) {

		if (object->ref_count == 0)
			panic("vm_object_deallocate: object deallocated too many times");

		/*
		 * Lose the reference
		 */
		object->ref_count--;
		if (object->ref_count != 0) {
			if ((object->ref_count == 1) &&
			    (object->handle == NULL) &&
			    (object->type == OBJT_DEFAULT ||
			     object->type == OBJT_SWAP)) {
				vm_object_t robject;
				robject = TAILQ_FIRST(&object->shadow_head);
				if ((robject != NULL) &&
				    (robject->handle == NULL) &&
				    (robject->type == OBJT_DEFAULT ||
				     robject->type == OBJT_SWAP)) {
					int s;
					robject->ref_count += 2;
					object->ref_count += 2;

					do {
						s = splvm();
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
						robject->ref_count += 1;
						object = robject;
						continue;
					}
					vm_object_collapse(robject);
					return;
				}
			}
			/*
			 * If there are still references, then we are done.
			 */
			return;
		}

		if (object->type == OBJT_VNODE) {
			struct vnode *vp = object->handle;

			vp->v_flag &= ~VTEXT;
		}

		/*
		 * See if this object can persist and has some resident
		 * pages.  If so, enter it in the cache.
		 */
		if (object->flags & OBJ_CANPERSIST) {
			if (object->resident_page_count != 0) {
				vm_object_page_clean(object, 0, 0 ,TRUE, TRUE);
				TAILQ_INSERT_TAIL(&vm_object_cached_list, object,
				    cached_list);
				vm_object_cached++;

				vm_object_cache_trim();
				return;
			} else {
				object->flags &= ~OBJ_CANPERSIST;
			}
		}

		/*
		 * Make sure no one uses us.
		 */
		object->flags |= OBJ_DEAD;

		temp = object->backing_object;
		if (temp) {
			TAILQ_REMOVE(&temp->shadow_head, object, shadow_list);
			--temp->shadow_count;
		}
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
static void
vm_object_terminate(object)
	register vm_object_t object;
{
	register vm_page_t p;
	int s;

	/*
	 * wait for the pageout daemon to be done with the object
	 */
	s = splvm();
	while (object->paging_in_progress) {
		object->flags |= OBJ_PIPWNT;
		tsleep(object, PVM, "objtrm", 0);
	}
	splx(s);

	if (object->paging_in_progress != 0)
		panic("vm_object_deallocate: pageout in progress");

	/*
	 * Clean and free the pages, as appropriate. All references to the
	 * object are gone, so we don't need to lock it.
	 */
	if (object->type == OBJT_VNODE) {
		struct vnode *vp = object->handle;

		VOP_LOCK(vp);
		vm_object_page_clean(object, 0, 0, TRUE, FALSE);
		vinvalbuf(vp, V_SAVE, NOCRED, NULL, 0, 0);
		VOP_UNLOCK(vp);
	} 
	/*
	 * Now free the pages. For internal objects, this also removes them
	 * from paging queues.
	 */
	while ((p = TAILQ_FIRST(&object->memq)) != NULL) {
#if defined(DIAGNOSTIC)
		if (p->flags & PG_BUSY)
			printf("vm_object_terminate: freeing busy page\n");
#endif
		PAGE_WAKEUP(p);
		vm_page_free(p);
		cnt.v_pfree++;
	}

	/*
	 * Let the pager know object is dead.
	 */
	vm_pager_deallocate(object);

	TAILQ_REMOVE(&vm_object_list, object, object_list);
	vm_object_count--;

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
vm_object_page_clean(object, start, end, syncio, lockflag)
	vm_object_t object;
	vm_pindex_t start;
	vm_pindex_t end;
	boolean_t syncio;
	boolean_t lockflag;
{
	register vm_page_t p, np, tp;
	register vm_offset_t tstart, tend;
	vm_pindex_t pi;
	int s;
	struct vnode *vp;
	int runlen;
	int maxf;
	int chkb;
	int maxb;
	int i;
	vm_page_t maf[vm_pageout_page_count];
	vm_page_t mab[vm_pageout_page_count];
	vm_page_t ma[vm_pageout_page_count];

	if (object->type != OBJT_VNODE ||
		(object->flags & OBJ_MIGHTBEDIRTY) == 0)
		return;

	vp = object->handle;

	if (lockflag)
		VOP_LOCK(vp);
	object->flags |= OBJ_CLEANING;

	tstart = start;
	if (end == 0) {
		tend = object->size;
	} else {
		tend = end;
	}
	if ((tstart == 0) && (tend == object->size)) {
		object->flags &= ~(OBJ_WRITEABLE|OBJ_MIGHTBEDIRTY);
	}
	for(p = TAILQ_FIRST(&object->memq); p; p = TAILQ_NEXT(p, listq))
		p->flags |= PG_CLEANCHK;

rescan:
	for(p = TAILQ_FIRST(&object->memq); p; p = np) {
		np = TAILQ_NEXT(p, listq);

		pi = p->pindex;
		if (((p->flags & PG_CLEANCHK) == 0) ||
			(pi < tstart) || (pi >= tend) ||
			(p->valid == 0) || (p->queue == PQ_CACHE)) {
			p->flags &= ~PG_CLEANCHK;
			continue;
		}

		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0) {
			p->flags &= ~PG_CLEANCHK;
			continue;
		}

		s = splvm();
		if ((p->flags & PG_BUSY) || p->busy) {
			p->flags |= PG_WANTED|PG_REFERENCED;
			tsleep(p, PVM, "vpcwai", 0);
			splx(s);
			goto rescan;
		}
		splx(s);
			
		s = splvm();
		maxf = 0;
		for(i=1;i<vm_pageout_page_count;i++) {
			if (tp = vm_page_lookup(object, pi + i)) {
				if ((tp->flags & PG_BUSY) ||
					(tp->flags & PG_CLEANCHK) == 0)
					break;
				if (tp->queue == PQ_CACHE) {
					tp->flags &= ~PG_CLEANCHK;
					break;
				}
				vm_page_test_dirty(tp);
				if ((tp->dirty & tp->valid) == 0) {
					tp->flags &= ~PG_CLEANCHK;
					break;
				}
				maf[ i - 1 ] = tp;
				maxf++;
				continue;
			}
			break;
		}

		maxb = 0;
		chkb = vm_pageout_page_count -  maxf;
		if (chkb) {
			for(i = 1; i < chkb;i++) {
				if (tp = vm_page_lookup(object, pi - i)) {
					if ((tp->flags & PG_BUSY) ||
						(tp->flags & PG_CLEANCHK) == 0)
						break;
					if (tp->queue == PQ_CACHE) {
						tp->flags &= ~PG_CLEANCHK;
						break;
					}
					vm_page_test_dirty(tp);
					if ((tp->dirty & tp->valid) == 0) {
						tp->flags &= ~PG_CLEANCHK;
						break;
					}
					mab[ i - 1 ] = tp;
					maxb++;
					continue;
				}
				break;
			}
		}

		for(i=0;i<maxb;i++) {
			int index = (maxb - i) - 1;
			ma[index] = mab[i];
			ma[index]->flags |= PG_BUSY;
			ma[index]->flags &= ~PG_CLEANCHK;
			vm_page_protect(ma[index], VM_PROT_READ);
		}
		vm_page_protect(p, VM_PROT_READ);
		p->flags |= PG_BUSY;
		p->flags &= ~PG_CLEANCHK;
		ma[maxb] = p;
		for(i=0;i<maxf;i++) {
			int index = (maxb + i) + 1;
			ma[index] = maf[i];
			ma[index]->flags |= PG_BUSY;
			ma[index]->flags &= ~PG_CLEANCHK;
			vm_page_protect(ma[index], VM_PROT_READ);
		}
		runlen = maxb + maxf + 1;
		splx(s);
		vm_pageout_flush(ma, runlen, 0);
		goto rescan;
	}

	VOP_FSYNC(vp, NULL, syncio, curproc);

	if (lockflag)
		VOP_UNLOCK(vp);
	object->flags &= ~OBJ_CLEANING;
	return;
}

#ifdef not_used
/* XXX I cannot tell if this should be an exported symbol */
/*
 *	vm_object_deactivate_pages
 *
 *	Deactivate all pages in the specified object.  (Keep its pages
 *	in memory even though it is no longer referenced.)
 *
 *	The object must be locked.
 */
static void
vm_object_deactivate_pages(object)
	register vm_object_t object;
{
	register vm_page_t p, next;

	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
		next = TAILQ_NEXT(p, listq);
		vm_page_deactivate(p);
	}
}
#endif

/*
 *	Trim the object cache to size.
 */
static void
vm_object_cache_trim()
{
	register vm_object_t object;

	while (vm_object_cached > vm_object_cache_max) {
		object = TAILQ_FIRST(&vm_object_cached_list);

		vm_object_reference(object);
		pager_cache(object, FALSE);
	}
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
	register vm_pindex_t start;
	register vm_pindex_t end;
{
	register vm_page_t p;

	if (object == NULL || (object->flags & OBJ_WRITEABLE) == 0)
		return;

	for (p = TAILQ_FIRST(&object->memq);
		p != NULL;
		p = TAILQ_NEXT(p, listq)) {
		vm_page_protect(p, VM_PROT_READ);
	}

	object->flags &= ~OBJ_WRITEABLE;
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
	register vm_pindex_t start;
	register vm_pindex_t end;
{
	register vm_page_t p;
	if (object == NULL)
		return;
	for (p = TAILQ_FIRST(&object->memq);
		p != NULL;
		p = TAILQ_NEXT(p, listq)) {
		if (p->pindex >= start && p->pindex < end)
			vm_page_protect(p, VM_PROT_NONE);
	}
}

/*
 *	vm_object_madvise:
 *
 *	Implements the madvise function at the object/page level.
 */
void
vm_object_madvise(object, pindex, count, advise)
	vm_object_t object;
	vm_pindex_t pindex;
	int count;
	int advise;
{
	vm_pindex_t end;
	vm_page_t m;

	if (object == NULL)
		return;

	end = pindex + count;

	for (; pindex < end; pindex += 1) {
		m = vm_page_lookup(object, pindex);

		/*
		 * If the page is busy or not in a normal active state,
		 * we skip it.  Things can break if we mess with pages
		 * in any of the below states.
		 */
		if (m == NULL || m->busy || (m->flags & PG_BUSY) ||
			m->hold_count || m->wire_count ||
			m->valid != VM_PAGE_BITS_ALL)
			continue;

		if (advise == MADV_WILLNEED) {
			if (m->queue != PQ_ACTIVE)
				vm_page_activate(m);
		} else if ((advise == MADV_DONTNEED) ||
			((advise == MADV_FREE) &&
				((object->type != OBJT_DEFAULT) &&
					(object->type != OBJT_SWAP)))) {
			vm_page_deactivate(m);
		} else if (advise == MADV_FREE) {
			/*
			 * Force a demand-zero on next ref
			 */
			if (object->type == OBJT_SWAP)
				swap_pager_dmzspace(object, m->pindex, 1);
			vm_page_protect(m, VM_PROT_NONE);
			vm_page_free(m);
		}
	}	
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
	vm_ooffset_t *offset;	/* IN/OUT */
	vm_size_t length;
{
	register vm_object_t source;
	register vm_object_t result;

	source = *object;

	/*
	 * Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(OBJT_DEFAULT, length)) == NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 * The new object shadows the source object, adding a reference to it.
	 * Our caller changes his reference to point to the new object,
	 * removing a reference to the source object.  Net result: no change
	 * of reference count.
	 */
	result->backing_object = source;
	if (source) {
		TAILQ_INSERT_TAIL(&source->shadow_head, result, shadow_list);
		++source->shadow_count;
	}

	/*
	 * Store the offset into the source object, and fix up the offset into
	 * the new object.
	 */

	result->backing_object_offset = *offset;

	/*
	 * Return the new things
	 */

	*offset = 0;
	*object = result;
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
	register vm_pindex_t backing_offset_index, paging_offset_index;
	vm_pindex_t backing_object_paging_offset_index;
	vm_pindex_t new_pindex;
	register vm_page_t p, pp;
	register vm_size_t size;

	backing_object = object->backing_object;
	if (backing_object->ref_count != 1)
		return;

	backing_object->ref_count += 2;

	backing_offset_index = OFF_TO_IDX(object->backing_object_offset);
	backing_object_paging_offset_index = OFF_TO_IDX(backing_object->paging_offset);
	paging_offset_index = OFF_TO_IDX(object->paging_offset);
	size = object->size;
	p = TAILQ_FIRST(&backing_object->memq);
	while (p) {
		vm_page_t next;

		next = TAILQ_NEXT(p, listq);
		if ((p->flags & (PG_BUSY | PG_FICTITIOUS)) ||
		    (p->queue == PQ_CACHE) || !p->valid || p->hold_count || p->wire_count || p->busy) {
			p = next;
			continue;
		}
		new_pindex = p->pindex - backing_offset_index;
		if (p->pindex < backing_offset_index ||
		    new_pindex >= size) {
			if (backing_object->type == OBJT_SWAP)
				swap_pager_freespace(backing_object,
				    backing_object_paging_offset_index+p->pindex,
				    1);
			vm_page_protect(p, VM_PROT_NONE);
			vm_page_free(p);
		} else {
			pp = vm_page_lookup(object, new_pindex);
			if (pp != NULL || (object->type == OBJT_SWAP && vm_pager_has_page(object,
				    paging_offset_index + new_pindex, NULL, NULL))) {
				if (backing_object->type == OBJT_SWAP)
					swap_pager_freespace(backing_object,
					    backing_object_paging_offset_index + p->pindex, 1);
				vm_page_protect(p, VM_PROT_NONE);
				vm_page_free(p);
			} else {
				if (backing_object->type == OBJT_SWAP)
					swap_pager_freespace(backing_object,
					    backing_object_paging_offset_index + p->pindex, 1);
				vm_page_rename(p, object, new_pindex);
				p->dirty = VM_PAGE_BITS_ALL;
			}
		}
		p = next;
	}
	backing_object->ref_count -= 2;
}

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 */
void
vm_object_collapse(object)
	vm_object_t object;

{
	vm_object_t backing_object;
	vm_ooffset_t backing_offset;
	vm_size_t size;
	vm_pindex_t new_pindex, backing_offset_index;
	vm_page_t p, pp;

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
		if ((backing_object = object->backing_object) == NULL)
			return;

		/*
		 * we check the backing object first, because it is most likely
		 * not collapsable.
		 */
		if (backing_object->handle != NULL ||
		    (backing_object->type != OBJT_DEFAULT &&
		     backing_object->type != OBJT_SWAP) ||
		    (backing_object->flags & OBJ_DEAD) ||
		    object->handle != NULL ||
		    (object->type != OBJT_DEFAULT &&
		     object->type != OBJT_SWAP) ||
		    (object->flags & OBJ_DEAD)) {
			return;
		}

		if (object->paging_in_progress != 0 ||
		    backing_object->paging_in_progress != 0) {
			vm_object_qcollapse(object);
			return;
		}

		/*
		 * We know that we can either collapse the backing object (if
		 * the parent is the only reference to it) or (perhaps) remove
		 * the parent's reference to it.
		 */

		backing_offset = object->backing_object_offset;
		backing_offset_index = OFF_TO_IDX(backing_offset);
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

			while ((p = TAILQ_FIRST(&backing_object->memq)) != 0) {

				new_pindex = p->pindex - backing_offset_index;

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, dispose of
				 * it.
				 *
				 * Otherwise, move it as planned.
				 */

				if (p->pindex < backing_offset_index ||
				    new_pindex >= size) {
					vm_page_protect(p, VM_PROT_NONE);
					PAGE_WAKEUP(p);
					vm_page_free(p);
				} else {
					pp = vm_page_lookup(object, new_pindex);
					if (pp != NULL || (object->type == OBJT_SWAP && vm_pager_has_page(object,
					    OFF_TO_IDX(object->paging_offset) + new_pindex, NULL, NULL))) {
						vm_page_protect(p, VM_PROT_NONE);
						PAGE_WAKEUP(p);
						vm_page_free(p);
					} else {
						vm_page_rename(p, object, new_pindex);
					}
				}
			}

			/*
			 * Move the pager from backing_object to object.
			 */

			if (backing_object->type == OBJT_SWAP) {
				backing_object->paging_in_progress++;
				if (object->type == OBJT_SWAP) {
					object->paging_in_progress++;
					/*
					 * copy shadow object pages into ours
					 * and destroy unneeded pages in
					 * shadow object.
					 */
					swap_pager_copy(
					    backing_object,
					    OFF_TO_IDX(backing_object->paging_offset),
					    object,
					    OFF_TO_IDX(object->paging_offset),
					    OFF_TO_IDX(object->backing_object_offset));
					vm_object_pip_wakeup(object);
				} else {
					object->paging_in_progress++;
					/*
					 * move the shadow backing_object's pager data to
					 * "object" and convert "object" type to OBJT_SWAP.
					 */
					object->type = OBJT_SWAP;
					object->un_pager.swp.swp_nblocks =
					    backing_object->un_pager.swp.swp_nblocks;
					object->un_pager.swp.swp_allocsize =
					    backing_object->un_pager.swp.swp_allocsize;
					object->un_pager.swp.swp_blocks =
					    backing_object->un_pager.swp.swp_blocks;
					object->un_pager.swp.swp_poip =		/* XXX */
					    backing_object->un_pager.swp.swp_poip;
					object->paging_offset = backing_object->paging_offset + backing_offset;
					TAILQ_INSERT_TAIL(&swap_pager_un_object_list, object, pager_object_list);

					/*
					 * Convert backing object from OBJT_SWAP to
					 * OBJT_DEFAULT. XXX - only the TAILQ_REMOVE is
					 * actually necessary.
					 */
					backing_object->type = OBJT_DEFAULT;
					TAILQ_REMOVE(&swap_pager_un_object_list, backing_object, pager_object_list);
					/*
					 * free unnecessary blocks
					 */
					swap_pager_freespace(object, 0,
						OFF_TO_IDX(object->paging_offset));
					vm_object_pip_wakeup(object);
				}

				vm_object_pip_wakeup(backing_object);
			}
			/*
			 * Object now shadows whatever backing_object did.
			 * Note that the reference to backing_object->backing_object
			 * moves from within backing_object to within object.
			 */

			TAILQ_REMOVE(&object->backing_object->shadow_head, object,
			    shadow_list);
			--object->backing_object->shadow_count;
			if (backing_object->backing_object) {
				TAILQ_REMOVE(&backing_object->backing_object->shadow_head,
				    backing_object, shadow_list);
				--backing_object->backing_object->shadow_count;
			}
			object->backing_object = backing_object->backing_object;
			if (object->backing_object) {
				TAILQ_INSERT_TAIL(&object->backing_object->shadow_head,
				    object, shadow_list);
				++object->backing_object->shadow_count;
			}

			object->backing_object_offset += backing_object->backing_object_offset;
			/*
			 * Discard backing_object.
			 *
			 * Since the backing object has no pages, no pager left,
			 * and no object references within it, all that is
			 * necessary is to dispose of it.
			 */

			TAILQ_REMOVE(&vm_object_list, backing_object,
			    object_list);
			vm_object_count--;

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

			if (backing_object->type != OBJT_DEFAULT) {
				return;
			}
			/*
			 * Should have a check for a 'small' number of pages
			 * here.
			 */

			for (p = TAILQ_FIRST(&backing_object->memq); p; p = TAILQ_NEXT(p, listq)) {
				new_pindex = p->pindex - backing_offset_index;

				/*
				 * If the parent has a page here, or if this
				 * page falls outside the parent, keep going.
				 *
				 * Otherwise, the backing_object must be left in
				 * the chain.
				 */

				if (p->pindex >= backing_offset_index &&
					new_pindex <= size) {

					pp = vm_page_lookup(object, new_pindex);

					if ((pp == NULL || pp->valid == 0) &&
				   	    !vm_pager_has_page(object, OFF_TO_IDX(object->paging_offset) + new_pindex, NULL, NULL)) {
						/*
						 * Page still needed. Can't go any
						 * further.
						 */
						return;
					}
				}
			}

			/*
			 * Make the parent shadow the next object in the
			 * chain.  Deallocating backing_object will not remove
			 * it, since its reference count is at least 2.
			 */

			TAILQ_REMOVE(&object->backing_object->shadow_head,
			    object, shadow_list);
			--object->backing_object->shadow_count;
			vm_object_reference(object->backing_object = backing_object->backing_object);
			if (object->backing_object) {
				TAILQ_INSERT_TAIL(&object->backing_object->shadow_head,
				    object, shadow_list);
				++object->backing_object->shadow_count;
			}
			object->backing_object_offset += backing_object->backing_object_offset;

			/*
			 * Drop the reference count on backing_object. Since
			 * its ref_count was at least 2, it will not vanish;
			 * so we don't need to call vm_object_deallocate.
			 */
			if (backing_object->ref_count == 1)
				printf("should have called obj deallocate\n");
			backing_object->ref_count--;

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
	register vm_pindex_t start;
	register vm_pindex_t end;
	boolean_t clean_only;
{
	register vm_page_t p, next;
	unsigned int size;
	int s;

	if (object == NULL)
		return;

	object->paging_in_progress++;
again:
	size = end - start;
	if (size > 4 || size >= object->size / 4) {
		for (p = TAILQ_FIRST(&object->memq); p != NULL; p = next) {
			next = TAILQ_NEXT(p, listq);
			if ((start <= p->pindex) && (p->pindex < end)) {
				if (p->wire_count != 0) {
					vm_page_protect(p, VM_PROT_NONE);
					p->valid = 0;
					continue;
				}

				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */
				if ((p->flags & PG_BUSY) || p->busy) {
					s = splvm();
					if ((p->flags & PG_BUSY) || p->busy) {
						p->flags |= PG_WANTED;
						tsleep(p, PVM, "vmopar", 0);
						splx(s);
						goto again;
					}
					splx(s);
				}

				if (clean_only) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty)
						continue;
				}
				vm_page_protect(p, VM_PROT_NONE);
				PAGE_WAKEUP(p);
				vm_page_free(p);
			}
		}
	} else {
		while (size > 0) {
			if ((p = vm_page_lookup(object, start)) != 0) {
				if (p->wire_count != 0) {
					p->valid = 0;
					vm_page_protect(p, VM_PROT_NONE);
					start += 1;
					size -= 1;
					continue;
				}
				/*
				 * The busy flags are only cleared at
				 * interrupt -- minimize the spl transitions
				 */
				if ((p->flags & PG_BUSY) || p->busy) {
					s = splvm();
					if ((p->flags & PG_BUSY) || p->busy) {
						p->flags |= PG_WANTED;
						tsleep(p, PVM, "vmopar", 0);
						splx(s);
						goto again;
					}
					splx(s);
				}
				if (clean_only) {
					vm_page_test_dirty(p);
					if (p->valid & p->dirty) {
						start += 1;
						size -= 1;
						continue;
					}
				}
				vm_page_protect(p, VM_PROT_NONE);
				PAGE_WAKEUP(p);
				vm_page_free(p);
			}
			start += 1;
			size -= 1;
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
vm_object_coalesce(prev_object, prev_pindex, prev_size, next_size)
	register vm_object_t prev_object;
	vm_pindex_t prev_pindex;
	vm_size_t prev_size, next_size;
{
	vm_size_t newsize;

	if (prev_object == NULL) {
		return (TRUE);
	}

	if (prev_object->type != OBJT_DEFAULT) {
		return (FALSE);
	}

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
	    prev_object->backing_object != NULL) {
		return (FALSE);
	}

	prev_size >>= PAGE_SHIFT;
	next_size >>= PAGE_SHIFT;
	/*
	 * Remove any pages that may still be in the object from a previous
	 * deallocation.
	 */

	vm_object_page_remove(prev_object,
	    prev_pindex + prev_size,
	    prev_pindex + prev_size + next_size, FALSE);

	/*
	 * Extend the object if necessary.
	 */
	newsize = prev_pindex + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	return (TRUE);
}

#ifdef DDB

static int
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
		for(; obj; obj=obj->backing_object)
			if( obj == object) {
				return 1;
			}
	}
	return 0;
}

static int
vm_object_in_map( object)
	vm_object_t object;
{
	struct proc *p;
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if( !p->p_vmspace /* || (p->p_flag & (P_SYSTEM|P_WEXIT)) */)
			continue;
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


#ifdef DDB
static void
DDB_vm_object_check()
{
	vm_object_t object;

	/*
	 * make sure that internal objs are in a map somewhere
	 * and none have zero ref counts.
	 */
	for (object = TAILQ_FIRST(&vm_object_list);
			object != NULL;
			object = TAILQ_NEXT(object, object_list)) {
		if (object->handle == NULL &&
		    (object->type == OBJT_DEFAULT || object->type == OBJT_SWAP)) {
			if (object->ref_count == 0) {
				printf("vmochk: internal obj has zero ref count: %d\n",
					object->size);
			}
			if (!vm_object_in_map(object)) {
				printf("vmochk: internal obj is not in a map: "
		"ref: %d, size: %d: 0x%x, backing_object: 0x%x\n",
				    object->ref_count, object->size, 
				    object->size, object->backing_object);
			}
		}
	}
}
#endif /* DDB */

/*
 *	vm_object_print:	[ debug ]
 */
void
vm_object_print(iobject, full, dummy3, dummy4)
	/* db_expr_t */ int iobject;
	boolean_t full;
	/* db_expr_t */ int dummy3;
	char *dummy4;
{
	vm_object_t object = (vm_object_t)iobject;	/* XXX */
	register vm_page_t p;

	register int count;

	if (object == NULL)
		return;

	iprintf("Object 0x%x: size=0x%x, res=%d, ref=%d, ",
	    (int) object, (int) object->size,
	    object->resident_page_count, object->ref_count);
	printf("offset=0x%x, backing_object=(0x%x)+0x%x\n",
	    (int) object->paging_offset,
	    (int) object->backing_object, (int) object->backing_object_offset);
	printf("cache: next=%p, prev=%p\n",
	    TAILQ_NEXT(object, cached_list), TAILQ_PREV(object, cached_list));

	if (!full)
		return;

	indent += 2;
	count = 0;
	for (p = TAILQ_FIRST(&object->memq); p != NULL; p = TAILQ_NEXT(p, listq)) {
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
		    (u_long) p->pindex, (u_long) VM_PAGE_TO_PHYS(p));
	}
	if (count != 0)
		printf("\n");
	indent -= 2;
}
#endif /* DDB */
