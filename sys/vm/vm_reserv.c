/*-
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007-2008 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Alan L. Cox,
 * Olivier Crameri, Peter Druschel, Sitaram Iyer, and Juan Navarro.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	Superpage reservation management module
 *
 * Any external functions defined by this module are only to be used by the
 * virtual memory system.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>

/*
 * The reservation system supports the speculative allocation of large physical
 * pages ("superpages").  Speculative allocation enables the fully-automatic
 * utilization of superpages by the virtual memory system.  In other words, no
 * programmatic directives are required to use superpages.
 */

#if VM_NRESERVLEVEL > 0

/*
 * The number of small pages that are contained in a level 0 reservation
 */
#define	VM_LEVEL_0_NPAGES	(1 << VM_LEVEL_0_ORDER)

/*
 * The number of bits by which a physical address is shifted to obtain the
 * reservation number
 */
#define	VM_LEVEL_0_SHIFT	(VM_LEVEL_0_ORDER + PAGE_SHIFT)

/*
 * The size of a level 0 reservation in bytes
 */
#define	VM_LEVEL_0_SIZE		(1 << VM_LEVEL_0_SHIFT)

/*
 * Computes the index of the small page underlying the given (object, pindex)
 * within the reservation's array of small pages.
 */
#define	VM_RESERV_INDEX(object, pindex)	\
    (((object)->pg_color + (pindex)) & (VM_LEVEL_0_NPAGES - 1))

/*
 * The reservation structure
 *
 * A reservation structure is constructed whenever a large physical page is
 * speculatively allocated to an object.  The reservation provides the small
 * physical pages for the range [pindex, pindex + VM_LEVEL_0_NPAGES) of offsets
 * within that object.  The reservation's "popcnt" tracks the number of these
 * small physical pages that are in use at any given time.  When and if the
 * reservation is not fully utilized, it appears in the queue of partially-
 * populated reservations.  The reservation always appears on the containing
 * object's list of reservations.
 *
 * A partially-populated reservation can be broken and reclaimed at any time.
 */
struct vm_reserv {
	TAILQ_ENTRY(vm_reserv) partpopq;
	LIST_ENTRY(vm_reserv) objq;
	vm_object_t	object;			/* containing object */
	vm_pindex_t	pindex;			/* offset within object */
	vm_page_t	pages;			/* first page of a superpage */
	int		popcnt;			/* # of pages in use */
	char		inpartpopq;
};

/*
 * The reservation array
 *
 * This array is analoguous in function to vm_page_array.  It differs in the
 * respect that it may contain a greater number of useful reservation
 * structures than there are (physical) superpages.  These "invalid"
 * reservation structures exist to trade-off space for time in the
 * implementation of vm_reserv_from_page().  Invalid reservation structures are
 * distinguishable from "valid" reservation structures by inspecting the
 * reservation's "pages" field.  Invalid reservation structures have a NULL
 * "pages" field.
 *
 * vm_reserv_from_page() maps a small (physical) page to an element of this
 * array by computing a physical reservation number from the page's physical
 * address.  The physical reservation number is used as the array index.
 *
 * An "active" reservation is a valid reservation structure that has a non-NULL
 * "object" field and a non-zero "popcnt" field.  In other words, every active
 * reservation belongs to a particular object.  Moreover, every active
 * reservation has an entry in the containing object's list of reservations.  
 */
static vm_reserv_t vm_reserv_array;

/*
 * The partially-populated reservation queue
 *
 * This queue enables the fast recovery of an unused cached or free small page
 * from a partially-populated reservation.  The reservation at the head of
 * this queue is the least-recently-changed, partially-populated reservation.
 *
 * Access to this queue is synchronized by the free page queue lock.
 */
static TAILQ_HEAD(, vm_reserv) vm_rvq_partpop =
			    TAILQ_HEAD_INITIALIZER(vm_rvq_partpop);

static SYSCTL_NODE(_vm, OID_AUTO, reserv, CTLFLAG_RD, 0, "Reservation Info");

static long vm_reserv_broken;
SYSCTL_LONG(_vm_reserv, OID_AUTO, broken, CTLFLAG_RD,
    &vm_reserv_broken, 0, "Cumulative number of broken reservations");

static long vm_reserv_freed;
SYSCTL_LONG(_vm_reserv, OID_AUTO, freed, CTLFLAG_RD,
    &vm_reserv_freed, 0, "Cumulative number of freed reservations");

static int sysctl_vm_reserv_partpopq(SYSCTL_HANDLER_ARGS);

SYSCTL_OID(_vm_reserv, OID_AUTO, partpopq, CTLTYPE_STRING | CTLFLAG_RD, NULL, 0,
    sysctl_vm_reserv_partpopq, "A", "Partially-populated reservation queues");

static long vm_reserv_reclaimed;
SYSCTL_LONG(_vm_reserv, OID_AUTO, reclaimed, CTLFLAG_RD,
    &vm_reserv_reclaimed, 0, "Cumulative number of reclaimed reservations");

static void		vm_reserv_depopulate(vm_reserv_t rv);
static vm_reserv_t	vm_reserv_from_page(vm_page_t m);
static boolean_t	vm_reserv_has_pindex(vm_reserv_t rv,
			    vm_pindex_t pindex);
static void		vm_reserv_populate(vm_reserv_t rv);
static void		vm_reserv_reclaim(vm_reserv_t rv);

/*
 * Describes the current state of the partially-populated reservation queue.
 */
static int
sysctl_vm_reserv_partpopq(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	vm_reserv_t rv;
	int counter, error, level, unused_pages;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	sbuf_printf(&sbuf, "\nLEVEL     SIZE  NUMBER\n\n");
	for (level = -1; level <= VM_NRESERVLEVEL - 2; level++) {
		counter = 0;
		unused_pages = 0;
		mtx_lock(&vm_page_queue_free_mtx);
		TAILQ_FOREACH(rv, &vm_rvq_partpop/*[level]*/, partpopq) {
			counter++;
			unused_pages += VM_LEVEL_0_NPAGES - rv->popcnt;
		}
		mtx_unlock(&vm_page_queue_free_mtx);
		sbuf_printf(&sbuf, "%5d: %6dK, %6d\n", level,
		    unused_pages * ((int)PAGE_SIZE / 1024), counter);
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

/*
 * Reduces the given reservation's population count.  If the population count
 * becomes zero, the reservation is destroyed.  Additionally, moves the
 * reservation to the tail of the partially-populated reservations queue if the
 * population count is non-zero.
 *
 * The free page queue lock must be held.
 */
static void
vm_reserv_depopulate(vm_reserv_t rv)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	KASSERT(rv->object != NULL,
	    ("vm_reserv_depopulate: reserv %p is free", rv));
	KASSERT(rv->popcnt > 0,
	    ("vm_reserv_depopulate: reserv %p's popcnt is corrupted", rv));
	if (rv->inpartpopq) {
		TAILQ_REMOVE(&vm_rvq_partpop, rv, partpopq);
		rv->inpartpopq = FALSE;
	}
	rv->popcnt--;
	if (rv->popcnt == 0) {
		LIST_REMOVE(rv, objq);
		rv->object = NULL;
		vm_phys_free_pages(rv->pages, VM_LEVEL_0_ORDER);
		vm_reserv_freed++;
	} else {
		rv->inpartpopq = TRUE;
		TAILQ_INSERT_TAIL(&vm_rvq_partpop, rv, partpopq);
	}
}

/*
 * Returns the reservation to which the given page might belong.
 */
static __inline vm_reserv_t
vm_reserv_from_page(vm_page_t m)
{

	return (&vm_reserv_array[VM_PAGE_TO_PHYS(m) >> VM_LEVEL_0_SHIFT]);
}

/*
 * Returns TRUE if the given reservation contains the given page index and
 * FALSE otherwise.
 */
static __inline boolean_t
vm_reserv_has_pindex(vm_reserv_t rv, vm_pindex_t pindex)
{

	return (((pindex - rv->pindex) & ~(VM_LEVEL_0_NPAGES - 1)) == 0);
}

/*
 * Increases the given reservation's population count.  Moves the reservation
 * to the tail of the partially-populated reservation queue.
 *
 * The free page queue must be locked.
 */
static void
vm_reserv_populate(vm_reserv_t rv)
{

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	KASSERT(rv->object != NULL,
	    ("vm_reserv_populate: reserv %p is free", rv));
	KASSERT(rv->popcnt < VM_LEVEL_0_NPAGES,
	    ("vm_reserv_populate: reserv %p is already full", rv));
	if (rv->inpartpopq) {
		TAILQ_REMOVE(&vm_rvq_partpop, rv, partpopq);
		rv->inpartpopq = FALSE;
	}
	rv->popcnt++;
	if (rv->popcnt < VM_LEVEL_0_NPAGES) {
		rv->inpartpopq = TRUE;
		TAILQ_INSERT_TAIL(&vm_rvq_partpop, rv, partpopq);
	}
}

/*
 * Allocates a contiguous set of physical pages of the given size "npages"
 * from an existing or newly-created reservation.  All of the physical pages
 * must be at or above the given physical address "low" and below the given
 * physical address "high".  The given value "alignment" determines the
 * alignment of the first physical page in the set.  If the given value
 * "boundary" is non-zero, then the set of physical pages cannot cross any
 * physical address boundary that is a multiple of that value.  Both
 * "alignment" and "boundary" must be a power of two.
 *
 * The object and free page queue must be locked.
 */
vm_page_t
vm_reserv_alloc_contig(vm_object_t object, vm_pindex_t pindex, u_long npages,
    vm_paddr_t low, vm_paddr_t high, u_long alignment, vm_paddr_t boundary)
{
	vm_paddr_t pa, size;
	vm_page_t m, m_ret, mpred, msucc;
	vm_pindex_t first, leftcap, rightcap;
	vm_reserv_t rv;
	u_long allocpages, maxpages, minpages;
	int i, index, n;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(npages != 0, ("vm_reserv_alloc_contig: npages is 0"));

	/*
	 * Is a reservation fundamentally impossible?
	 */
	if (pindex < VM_RESERV_INDEX(object, pindex) ||
	    pindex + npages > object->size)
		return (NULL);

	/*
	 * All reservations of a particular size have the same alignment.
	 * Assuming that the first page is allocated from a reservation, the
	 * least significant bits of its physical address can be determined
	 * from its offset from the beginning of the reservation and the size
	 * of the reservation.
	 *
	 * Could the specified index within a reservation of the smallest
	 * possible size satisfy the alignment and boundary requirements?
	 */
	pa = VM_RESERV_INDEX(object, pindex) << PAGE_SHIFT;
	if ((pa & (alignment - 1)) != 0)
		return (NULL);
	size = npages << PAGE_SHIFT;
	if (((pa ^ (pa + size - 1)) & ~(boundary - 1)) != 0)
		return (NULL);

	/*
	 * Look for an existing reservation.
	 */
	mpred = vm_radix_lookup_le(&object->rtree, pindex);
	if (mpred != NULL) {
		KASSERT(mpred->pindex < pindex,
		    ("vm_reserv_alloc_contig: pindex already allocated"));
		rv = vm_reserv_from_page(mpred);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
		msucc = TAILQ_NEXT(mpred, listq);
	} else
		msucc = TAILQ_FIRST(&object->memq);
	if (msucc != NULL) {
		KASSERT(msucc->pindex > pindex,
		    ("vm_reserv_alloc_page: pindex already allocated"));
		rv = vm_reserv_from_page(msucc);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
	}

	/*
	 * Could at least one reservation fit between the first index to the
	 * left that can be used and the first index to the right that cannot
	 * be used?
	 */
	first = pindex - VM_RESERV_INDEX(object, pindex);
	if (mpred != NULL) {
		if ((rv = vm_reserv_from_page(mpred))->object != object)
			leftcap = mpred->pindex + 1;
		else
			leftcap = rv->pindex + VM_LEVEL_0_NPAGES;
		if (leftcap > first)
			return (NULL);
	}
	minpages = VM_RESERV_INDEX(object, pindex) + npages;
	maxpages = roundup2(minpages, VM_LEVEL_0_NPAGES);
	allocpages = maxpages;
	if (msucc != NULL) {
		if ((rv = vm_reserv_from_page(msucc))->object != object)
			rightcap = msucc->pindex;
		else
			rightcap = rv->pindex;
		if (first + maxpages > rightcap) {
			if (maxpages == VM_LEVEL_0_NPAGES)
				return (NULL);
			allocpages = minpages;
		}
	}

	/*
	 * Would the last new reservation extend past the end of the object?
	 */
	if (first + maxpages > object->size) {
		/*
		 * Don't allocate the last new reservation if the object is a
		 * vnode or backed by another object that is a vnode. 
		 */
		if (object->type == OBJT_VNODE ||
		    (object->backing_object != NULL &&
		    object->backing_object->type == OBJT_VNODE)) {
			if (maxpages == VM_LEVEL_0_NPAGES)
				return (NULL);
			allocpages = minpages;
		}
		/* Speculate that the object may grow. */
	}

	/*
	 * Allocate and populate the new reservations.  The alignment and
	 * boundary specified for this allocation may be different from the
	 * alignment and boundary specified for the requested pages.  For
	 * instance, the specified index may not be the first page within the
	 * first new reservation.
	 */
	m = vm_phys_alloc_contig(allocpages, low, high, ulmax(alignment,
	    VM_LEVEL_0_SIZE), boundary > VM_LEVEL_0_SIZE ? boundary : 0);
	if (m == NULL)
		return (NULL);
	m_ret = NULL;
	index = VM_RESERV_INDEX(object, pindex);
	do {
		rv = vm_reserv_from_page(m);
		KASSERT(rv->pages == m,
		    ("vm_reserv_alloc_contig: reserv %p's pages is corrupted",
		    rv));
		KASSERT(rv->object == NULL,
		    ("vm_reserv_alloc_contig: reserv %p isn't free", rv));
		LIST_INSERT_HEAD(&object->rvq, rv, objq);
		rv->object = object;
		rv->pindex = first;
		KASSERT(rv->popcnt == 0,
		    ("vm_reserv_alloc_contig: reserv %p's popcnt is corrupted",
		    rv));
		KASSERT(!rv->inpartpopq,
		    ("vm_reserv_alloc_contig: reserv %p's inpartpopq is TRUE",
		    rv));
		n = ulmin(VM_LEVEL_0_NPAGES - index, npages);
		for (i = 0; i < n; i++)
			vm_reserv_populate(rv);
		npages -= n;
		if (m_ret == NULL) {
			m_ret = &rv->pages[index];
			index = 0;
		}
		m += VM_LEVEL_0_NPAGES;
		first += VM_LEVEL_0_NPAGES;
		allocpages -= VM_LEVEL_0_NPAGES;
	} while (allocpages > 0);
	return (m_ret);

	/*
	 * Found a matching reservation.
	 */
found:
	index = VM_RESERV_INDEX(object, pindex);
	/* Does the allocation fit within the reservation? */
	if (index + npages > VM_LEVEL_0_NPAGES)
		return (NULL);
	m = &rv->pages[index];
	pa = VM_PAGE_TO_PHYS(m);
	if (pa < low || pa + size > high || (pa & (alignment - 1)) != 0 ||
	    ((pa ^ (pa + size - 1)) & ~(boundary - 1)) != 0)
		return (NULL);
	/* Handle vm_page_rename(m, new_object, ...). */
	for (i = 0; i < npages; i++)
		if ((rv->pages[index + i].flags & (PG_CACHED | PG_FREE)) == 0)
			return (NULL);
	for (i = 0; i < npages; i++)
		vm_reserv_populate(rv);
	return (m);
}

/*
 * Allocates a page from an existing or newly-created reservation.
 *
 * The page "mpred" must immediately precede the offset "pindex" within the
 * specified object.
 *
 * The object and free page queue must be locked.
 */
vm_page_t
vm_reserv_alloc_page(vm_object_t object, vm_pindex_t pindex, vm_page_t mpred)
{
	vm_page_t m, msucc;
	vm_pindex_t first, leftcap, rightcap;
	vm_reserv_t rv;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	VM_OBJECT_ASSERT_WLOCKED(object);

	/*
	 * Is a reservation fundamentally impossible?
	 */
	if (pindex < VM_RESERV_INDEX(object, pindex) ||
	    pindex >= object->size)
		return (NULL);

	/*
	 * Look for an existing reservation.
	 */
	if (mpred != NULL) {
		KASSERT(mpred->object == object ||
		    (mpred->flags & PG_SLAB) != 0,
		    ("vm_reserv_alloc_page: object doesn't contain mpred"));
		KASSERT(mpred->pindex < pindex,
		    ("vm_reserv_alloc_page: mpred doesn't precede pindex"));
		rv = vm_reserv_from_page(mpred);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
		msucc = TAILQ_NEXT(mpred, listq);
	} else
		msucc = TAILQ_FIRST(&object->memq);
	if (msucc != NULL) {
		KASSERT(msucc->pindex > pindex,
		    ("vm_reserv_alloc_page: msucc doesn't succeed pindex"));
		rv = vm_reserv_from_page(msucc);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
	}

	/*
	 * Could a reservation fit between the first index to the left that
	 * can be used and the first index to the right that cannot be used?
	 */
	first = pindex - VM_RESERV_INDEX(object, pindex);
	if (mpred != NULL) {
		if ((rv = vm_reserv_from_page(mpred))->object != object)
			leftcap = mpred->pindex + 1;
		else
			leftcap = rv->pindex + VM_LEVEL_0_NPAGES;
		if (leftcap > first)
			return (NULL);
	}
	if (msucc != NULL) {
		if ((rv = vm_reserv_from_page(msucc))->object != object)
			rightcap = msucc->pindex;
		else
			rightcap = rv->pindex;
		if (first + VM_LEVEL_0_NPAGES > rightcap)
			return (NULL);
	}

	/*
	 * Would a new reservation extend past the end of the object? 
	 */
	if (first + VM_LEVEL_0_NPAGES > object->size) {
		/*
		 * Don't allocate a new reservation if the object is a vnode or
		 * backed by another object that is a vnode. 
		 */
		if (object->type == OBJT_VNODE ||
		    (object->backing_object != NULL &&
		    object->backing_object->type == OBJT_VNODE))
			return (NULL);
		/* Speculate that the object may grow. */
	}

	/*
	 * Allocate and populate the new reservation.
	 */
	m = vm_phys_alloc_pages(VM_FREEPOOL_DEFAULT, VM_LEVEL_0_ORDER);
	if (m == NULL)
		return (NULL);
	rv = vm_reserv_from_page(m);
	KASSERT(rv->pages == m,
	    ("vm_reserv_alloc_page: reserv %p's pages is corrupted", rv));
	KASSERT(rv->object == NULL,
	    ("vm_reserv_alloc_page: reserv %p isn't free", rv));
	LIST_INSERT_HEAD(&object->rvq, rv, objq);
	rv->object = object;
	rv->pindex = first;
	KASSERT(rv->popcnt == 0,
	    ("vm_reserv_alloc_page: reserv %p's popcnt is corrupted", rv));
	KASSERT(!rv->inpartpopq,
	    ("vm_reserv_alloc_page: reserv %p's inpartpopq is TRUE", rv));
	vm_reserv_populate(rv);
	return (&rv->pages[VM_RESERV_INDEX(object, pindex)]);

	/*
	 * Found a matching reservation.
	 */
found:
	m = &rv->pages[VM_RESERV_INDEX(object, pindex)];
	/* Handle vm_page_rename(m, new_object, ...). */
	if ((m->flags & (PG_CACHED | PG_FREE)) == 0)
		return (NULL);
	vm_reserv_populate(rv);
	return (m);
}

/*
 * Breaks all reservations belonging to the given object.
 */
void
vm_reserv_break_all(vm_object_t object)
{
	vm_reserv_t rv;
	int i;

	mtx_lock(&vm_page_queue_free_mtx);
	while ((rv = LIST_FIRST(&object->rvq)) != NULL) {
		KASSERT(rv->object == object,
		    ("vm_reserv_break_all: reserv %p is corrupted", rv));
		if (rv->inpartpopq) {
			TAILQ_REMOVE(&vm_rvq_partpop, rv, partpopq);
			rv->inpartpopq = FALSE;
		}
		LIST_REMOVE(rv, objq);
		rv->object = NULL;
		for (i = 0; i < VM_LEVEL_0_NPAGES; i++) {
			if ((rv->pages[i].flags & (PG_CACHED | PG_FREE)) != 0)
				vm_phys_free_pages(&rv->pages[i], 0);
			else
				rv->popcnt--;
		}
		KASSERT(rv->popcnt == 0,
		    ("vm_reserv_break_all: reserv %p's popcnt is corrupted",
		    rv));
		vm_reserv_broken++;
	}
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 * Frees the given page if it belongs to a reservation.  Returns TRUE if the
 * page is freed and FALSE otherwise.
 *
 * The free page queue lock must be held.
 */
boolean_t
vm_reserv_free_page(vm_page_t m)
{
	vm_reserv_t rv;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	rv = vm_reserv_from_page(m);
	if (rv->object == NULL)
		return (FALSE);
	if ((m->flags & PG_CACHED) != 0 && m->pool != VM_FREEPOOL_CACHE)
		vm_phys_set_pool(VM_FREEPOOL_CACHE, rv->pages,
		    VM_LEVEL_0_ORDER);
	vm_reserv_depopulate(rv);
	return (TRUE);
}

/*
 * Initializes the reservation management system.  Specifically, initializes
 * the reservation array.
 *
 * Requires that vm_page_array and first_page are initialized!
 */
void
vm_reserv_init(void)
{
	vm_paddr_t paddr;
	int i;

	/*
	 * Initialize the reservation array.  Specifically, initialize the
	 * "pages" field for every element that has an underlying superpage.
	 */
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		paddr = roundup2(phys_avail[i], VM_LEVEL_0_SIZE);
		while (paddr + VM_LEVEL_0_SIZE <= phys_avail[i + 1]) {
			vm_reserv_array[paddr >> VM_LEVEL_0_SHIFT].pages =
			    PHYS_TO_VM_PAGE(paddr);
			paddr += VM_LEVEL_0_SIZE;
		}
	}
}

/*
 * Returns a reservation level if the given page belongs to a fully-populated
 * reservation and -1 otherwise.
 */
int
vm_reserv_level_iffullpop(vm_page_t m)
{
	vm_reserv_t rv;

	rv = vm_reserv_from_page(m);
	return (rv->popcnt == VM_LEVEL_0_NPAGES ? 0 : -1);
}

/*
 * Prepare for the reactivation of a cached page.
 *
 * First, suppose that the given page "m" was allocated individually, i.e., not
 * as part of a reservation, and cached.  Then, suppose a reservation
 * containing "m" is allocated by the same object.  Although "m" and the
 * reservation belong to the same object, "m"'s pindex may not match the
 * reservation's.
 *
 * The free page queue must be locked.
 */
boolean_t
vm_reserv_reactivate_page(vm_page_t m)
{
	vm_reserv_t rv;
	int i, m_index;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	rv = vm_reserv_from_page(m);
	if (rv->object == NULL)
		return (FALSE);
	KASSERT((m->flags & PG_CACHED) != 0,
	    ("vm_reserv_uncache_page: page %p is not cached", m));
	if (m->object == rv->object &&
	    m->pindex - rv->pindex == VM_RESERV_INDEX(m->object, m->pindex))
		vm_reserv_populate(rv);
	else {
		KASSERT(rv->inpartpopq,
		    ("vm_reserv_uncache_page: reserv %p's inpartpopq is FALSE",
		    rv));
		TAILQ_REMOVE(&vm_rvq_partpop, rv, partpopq);
		rv->inpartpopq = FALSE;
		LIST_REMOVE(rv, objq);
		rv->object = NULL;
		/* Don't vm_phys_free_pages(m, 0). */
		m_index = m - rv->pages;
		for (i = 0; i < m_index; i++) {
			if ((rv->pages[i].flags & (PG_CACHED | PG_FREE)) != 0)
				vm_phys_free_pages(&rv->pages[i], 0);
			else
				rv->popcnt--;
		}
		for (i++; i < VM_LEVEL_0_NPAGES; i++) {
			if ((rv->pages[i].flags & (PG_CACHED | PG_FREE)) != 0)
				vm_phys_free_pages(&rv->pages[i], 0);
			else
				rv->popcnt--;
		}
		KASSERT(rv->popcnt == 0,
		    ("vm_reserv_uncache_page: reserv %p's popcnt is corrupted",
		    rv));
		vm_reserv_broken++;
	}
	return (TRUE);
}

/*
 * Breaks the given partially-populated reservation, releasing its cached and
 * free pages to the physical memory allocator.
 *
 * The free page queue lock must be held.
 */
static void
vm_reserv_reclaim(vm_reserv_t rv)
{
	int i;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	KASSERT(rv->inpartpopq,
	    ("vm_reserv_reclaim: reserv %p's inpartpopq is corrupted", rv));
	TAILQ_REMOVE(&vm_rvq_partpop, rv, partpopq);
	rv->inpartpopq = FALSE;
	KASSERT(rv->object != NULL,
	    ("vm_reserv_reclaim: reserv %p is free", rv));
	LIST_REMOVE(rv, objq);
	rv->object = NULL;
	for (i = 0; i < VM_LEVEL_0_NPAGES; i++) {
		if ((rv->pages[i].flags & (PG_CACHED | PG_FREE)) != 0)
			vm_phys_free_pages(&rv->pages[i], 0);
		else
			rv->popcnt--;
	}
	KASSERT(rv->popcnt == 0,
	    ("vm_reserv_reclaim: reserv %p's popcnt is corrupted", rv));
	vm_reserv_reclaimed++;
}

/*
 * Breaks the reservation at the head of the partially-populated reservation
 * queue, releasing its cached and free pages to the physical memory
 * allocator.  Returns TRUE if a reservation is broken and FALSE otherwise.
 *
 * The free page queue lock must be held.
 */
boolean_t
vm_reserv_reclaim_inactive(void)
{
	vm_reserv_t rv;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	if ((rv = TAILQ_FIRST(&vm_rvq_partpop)) != NULL) {
		vm_reserv_reclaim(rv);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Searches the partially-populated reservation queue for the least recently
 * active reservation with unused pages, i.e., cached or free, that satisfy the
 * given request for contiguous physical memory.  If a satisfactory reservation
 * is found, it is broken.  Returns TRUE if a reservation is broken and FALSE
 * otherwise.
 *
 * The free page queue lock must be held.
 */
boolean_t
vm_reserv_reclaim_contig(u_long npages, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary)
{
	vm_paddr_t pa, pa_length, size;
	vm_reserv_t rv;
	int i;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	if (npages > VM_LEVEL_0_NPAGES - 1)
		return (FALSE);
	size = npages << PAGE_SHIFT;
	TAILQ_FOREACH(rv, &vm_rvq_partpop, partpopq) {
		pa = VM_PAGE_TO_PHYS(&rv->pages[VM_LEVEL_0_NPAGES - 1]);
		if (pa + PAGE_SIZE - size < low) {
			/* this entire reservation is too low; go to next */
			continue;
		}
		pa_length = 0;
		for (i = 0; i < VM_LEVEL_0_NPAGES; i++)
			if ((rv->pages[i].flags & (PG_CACHED | PG_FREE)) != 0) {
				pa_length += PAGE_SIZE;
				if (pa_length == PAGE_SIZE) {
					pa = VM_PAGE_TO_PHYS(&rv->pages[i]);
					if (pa + size > high) {
						/* skip to next reservation */
						break;
					} else if (pa < low ||
					    (pa & (alignment - 1)) != 0 ||
					    ((pa ^ (pa + size - 1)) &
					    ~(boundary - 1)) != 0)
						pa_length = 0;
				}
				if (pa_length >= size) {
					vm_reserv_reclaim(rv);
					return (TRUE);
				}
			} else
				pa_length = 0;
	}
	return (FALSE);
}

/*
 * Transfers the reservation underlying the given page to a new object.
 *
 * The object must be locked.
 */
void
vm_reserv_rename(vm_page_t m, vm_object_t new_object, vm_object_t old_object,
    vm_pindex_t old_object_offset)
{
	vm_reserv_t rv;

	VM_OBJECT_ASSERT_WLOCKED(new_object);
	rv = vm_reserv_from_page(m);
	if (rv->object == old_object) {
		mtx_lock(&vm_page_queue_free_mtx);
		if (rv->object == old_object) {
			LIST_REMOVE(rv, objq);
			LIST_INSERT_HEAD(&new_object->rvq, rv, objq);
			rv->object = new_object;
			rv->pindex -= old_object_offset;
		}
		mtx_unlock(&vm_page_queue_free_mtx);
	}
}

/*
 * Allocates the virtual and physical memory required by the reservation
 * management system's data structures, in particular, the reservation array.
 */
vm_paddr_t
vm_reserv_startup(vm_offset_t *vaddr, vm_paddr_t end, vm_paddr_t high_water)
{
	vm_paddr_t new_end;
	size_t size;

	/*
	 * Calculate the size (in bytes) of the reservation array.  Round up
	 * from "high_water" because every small page is mapped to an element
	 * in the reservation array based on its physical address.  Thus, the
	 * number of elements in the reservation array can be greater than the
	 * number of superpages. 
	 */
	size = howmany(high_water, VM_LEVEL_0_SIZE) * sizeof(struct vm_reserv);

	/*
	 * Allocate and map the physical memory for the reservation array.  The
	 * next available virtual address is returned by reference.
	 */
	new_end = end - round_page(size);
	vm_reserv_array = (void *)(uintptr_t)pmap_map(vaddr, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	bzero(vm_reserv_array, size);

	/*
	 * Return the next available physical address.
	 */
	return (new_end);
}

#endif	/* VM_NRESERVLEVEL > 0 */
