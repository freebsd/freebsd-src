/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007-2011 Alan L. Cox <alc@cs.rice.edu>
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
#include <sys/bitstring.h>
#include <sys/counter.h>
#include <sys/ktr.h>
#include <sys/vmmeter.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pagequeue.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>

/*
 * The reservation system supports the speculative allocation of large physical
 * pages ("superpages").  Speculative allocation enables the fully automatic
 * utilization of superpages by the virtual memory system.  In other words, no
 * programmatic directives are required to use superpages.
 */

#if VM_NRESERVLEVEL > 0

#ifndef VM_LEVEL_0_ORDER_MAX
#define	VM_LEVEL_0_ORDER_MAX	VM_LEVEL_0_ORDER
#endif

/*
 * The number of small pages that are contained in a level 0 reservation
 */
#define	VM_LEVEL_0_NPAGES	(1 << VM_LEVEL_0_ORDER)
#define	VM_LEVEL_0_NPAGES_MAX	(1 << VM_LEVEL_0_ORDER_MAX)

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
 * Number of elapsed ticks before we update the LRU queue position.  Used
 * to reduce contention and churn on the list.
 */
#define	PARTPOPSLOP	1

/*
 * The reservation structure
 *
 * A reservation structure is constructed whenever a large physical page is
 * speculatively allocated to an object.  The reservation provides the small
 * physical pages for the range [pindex, pindex + VM_LEVEL_0_NPAGES) of offsets
 * within that object.  The reservation's "popcnt" tracks the number of these
 * small physical pages that are in use at any given time.  When and if the
 * reservation is not fully utilized, it appears in the queue of partially
 * populated reservations.  The reservation always appears on the containing
 * object's list of reservations.
 *
 * A partially populated reservation can be broken and reclaimed at any time.
 *
 * c - constant after boot
 * d - vm_reserv_domain_lock
 * o - vm_reserv_object_lock
 * r - vm_reserv_lock
 * s - vm_reserv_domain_scan_lock
 */
struct vm_reserv {
	struct mtx	lock;			/* reservation lock. */
	TAILQ_ENTRY(vm_reserv) partpopq;	/* (d, r) per-domain queue. */
	LIST_ENTRY(vm_reserv) objq;		/* (o, r) object queue */
	vm_object_t	object;			/* (o, r) containing object */
	vm_pindex_t	pindex;			/* (o, r) offset in object */
	vm_page_t	pages;			/* (c) first page  */
	uint16_t	popcnt;			/* (r) # of pages in use */
	uint8_t		domain;			/* (c) NUMA domain. */
	char		inpartpopq;		/* (d, r) */
	int		lasttick;		/* (r) last pop update tick. */
	bitstr_t	bit_decl(popmap, VM_LEVEL_0_NPAGES_MAX);
						/* (r) bit vector, used pages */
};

TAILQ_HEAD(vm_reserv_queue, vm_reserv);

#define	vm_reserv_lockptr(rv)		(&(rv)->lock)
#define	vm_reserv_assert_locked(rv)					\
	    mtx_assert(vm_reserv_lockptr(rv), MA_OWNED)
#define	vm_reserv_lock(rv)		mtx_lock(vm_reserv_lockptr(rv))
#define	vm_reserv_trylock(rv)		mtx_trylock(vm_reserv_lockptr(rv))
#define	vm_reserv_unlock(rv)		mtx_unlock(vm_reserv_lockptr(rv))

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
 * The per-domain partially populated reservation queues
 *
 * These queues enable the fast recovery of an unused free small page from a
 * partially populated reservation.  The reservation at the head of a queue
 * is the least recently changed, partially populated reservation.
 *
 * Access to this queue is synchronized by the per-domain reservation lock.
 * Threads reclaiming free pages from the queue must hold the per-domain scan
 * lock.
 */
struct vm_reserv_domain {
	struct mtx 		lock;
	struct vm_reserv_queue	partpop;	/* (d) */
	struct vm_reserv	marker;		/* (d, s) scan marker/lock */
} __aligned(CACHE_LINE_SIZE);

static struct vm_reserv_domain vm_rvd[MAXMEMDOM];

#define	vm_reserv_domain_lockptr(d)	(&vm_rvd[(d)].lock)
#define	vm_reserv_domain_assert_locked(d)	\
	mtx_assert(vm_reserv_domain_lockptr(d), MA_OWNED)
#define	vm_reserv_domain_lock(d)	mtx_lock(vm_reserv_domain_lockptr(d))
#define	vm_reserv_domain_unlock(d)	mtx_unlock(vm_reserv_domain_lockptr(d))

#define	vm_reserv_domain_scan_lock(d)	mtx_lock(&vm_rvd[(d)].marker.lock)
#define	vm_reserv_domain_scan_unlock(d)	mtx_unlock(&vm_rvd[(d)].marker.lock)

static SYSCTL_NODE(_vm, OID_AUTO, reserv, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Reservation Info");

static COUNTER_U64_DEFINE_EARLY(vm_reserv_broken);
SYSCTL_COUNTER_U64(_vm_reserv, OID_AUTO, broken, CTLFLAG_RD,
    &vm_reserv_broken, "Cumulative number of broken reservations");

static COUNTER_U64_DEFINE_EARLY(vm_reserv_freed);
SYSCTL_COUNTER_U64(_vm_reserv, OID_AUTO, freed, CTLFLAG_RD,
    &vm_reserv_freed, "Cumulative number of freed reservations");

static int sysctl_vm_reserv_fullpop(SYSCTL_HANDLER_ARGS);

SYSCTL_PROC(_vm_reserv, OID_AUTO, fullpop, CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RD,
    NULL, 0, sysctl_vm_reserv_fullpop, "I", "Current number of full reservations");

static int sysctl_vm_reserv_partpopq(SYSCTL_HANDLER_ARGS);

SYSCTL_OID(_vm_reserv, OID_AUTO, partpopq,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_vm_reserv_partpopq, "A",
    "Partially populated reservation queues");

static COUNTER_U64_DEFINE_EARLY(vm_reserv_reclaimed);
SYSCTL_COUNTER_U64(_vm_reserv, OID_AUTO, reclaimed, CTLFLAG_RD,
    &vm_reserv_reclaimed, "Cumulative number of reclaimed reservations");

/*
 * The object lock pool is used to synchronize the rvq.  We can not use a
 * pool mutex because it is required before malloc works.
 *
 * The "hash" function could be made faster without divide and modulo.
 */
#define	VM_RESERV_OBJ_LOCK_COUNT	MAXCPU

struct mtx_padalign vm_reserv_object_mtx[VM_RESERV_OBJ_LOCK_COUNT];

#define	vm_reserv_object_lock_idx(object)			\
	    (((uintptr_t)object / sizeof(*object)) % VM_RESERV_OBJ_LOCK_COUNT)
#define	vm_reserv_object_lock_ptr(object)			\
	    &vm_reserv_object_mtx[vm_reserv_object_lock_idx((object))]
#define	vm_reserv_object_lock(object)				\
	    mtx_lock(vm_reserv_object_lock_ptr((object)))
#define	vm_reserv_object_unlock(object)				\
	    mtx_unlock(vm_reserv_object_lock_ptr((object)))

static void		vm_reserv_break(vm_reserv_t rv);
static void		vm_reserv_depopulate(vm_reserv_t rv, int index);
static vm_reserv_t	vm_reserv_from_page(vm_page_t m);
static boolean_t	vm_reserv_has_pindex(vm_reserv_t rv,
			    vm_pindex_t pindex);
static void		vm_reserv_populate(vm_reserv_t rv, int index);
static void		vm_reserv_reclaim(vm_reserv_t rv);

/*
 * Returns the current number of full reservations.
 *
 * Since the number of full reservations is computed without acquiring any
 * locks, the returned value is inexact.
 */
static int
sysctl_vm_reserv_fullpop(SYSCTL_HANDLER_ARGS)
{
	vm_paddr_t paddr;
	struct vm_phys_seg *seg;
	vm_reserv_t rv;
	int fullpop, segind;

	fullpop = 0;
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		paddr = roundup2(seg->start, VM_LEVEL_0_SIZE);
#ifdef VM_PHYSSEG_SPARSE
		rv = seg->first_reserv + (paddr >> VM_LEVEL_0_SHIFT) -
		    (seg->start >> VM_LEVEL_0_SHIFT);
#else
		rv = &vm_reserv_array[paddr >> VM_LEVEL_0_SHIFT];
#endif
		while (paddr + VM_LEVEL_0_SIZE > paddr && paddr +
		    VM_LEVEL_0_SIZE <= seg->end) {
			fullpop += rv->popcnt == VM_LEVEL_0_NPAGES;
			paddr += VM_LEVEL_0_SIZE;
			rv++;
		}
	}
	return (sysctl_handle_int(oidp, &fullpop, 0, req));
}

/*
 * Describes the current state of the partially populated reservation queue.
 */
static int
sysctl_vm_reserv_partpopq(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	vm_reserv_t rv;
	int counter, error, domain, level, unused_pages;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	sbuf_printf(&sbuf, "\nDOMAIN    LEVEL     SIZE  NUMBER\n\n");
	for (domain = 0; domain < vm_ndomains; domain++) {
		for (level = -1; level <= VM_NRESERVLEVEL - 2; level++) {
			counter = 0;
			unused_pages = 0;
			vm_reserv_domain_lock(domain);
			TAILQ_FOREACH(rv, &vm_rvd[domain].partpop, partpopq) {
				if (rv == &vm_rvd[domain].marker)
					continue;
				counter++;
				unused_pages += VM_LEVEL_0_NPAGES - rv->popcnt;
			}
			vm_reserv_domain_unlock(domain);
			sbuf_printf(&sbuf, "%6d, %7d, %6dK, %6d\n",
			    domain, level,
			    unused_pages * ((int)PAGE_SIZE / 1024), counter);
		}
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

/*
 * Remove a reservation from the object's objq.
 */
static void
vm_reserv_remove(vm_reserv_t rv)
{
	vm_object_t object;

	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	KASSERT(rv->object != NULL,
	    ("vm_reserv_remove: reserv %p is free", rv));
	KASSERT(!rv->inpartpopq,
	    ("vm_reserv_remove: reserv %p's inpartpopq is TRUE", rv));
	object = rv->object;
	vm_reserv_object_lock(object);
	LIST_REMOVE(rv, objq);
	rv->object = NULL;
	vm_reserv_object_unlock(object);
}

/*
 * Insert a new reservation into the object's objq.
 */
static void
vm_reserv_insert(vm_reserv_t rv, vm_object_t object, vm_pindex_t pindex)
{

	vm_reserv_assert_locked(rv);
	CTR6(KTR_VM,
	    "%s: rv %p(%p) object %p new %p popcnt %d",
	    __FUNCTION__, rv, rv->pages, rv->object, object,
	   rv->popcnt);
	KASSERT(rv->object == NULL,
	    ("vm_reserv_insert: reserv %p isn't free", rv));
	KASSERT(rv->popcnt == 0,
	    ("vm_reserv_insert: reserv %p's popcnt is corrupted", rv));
	KASSERT(!rv->inpartpopq,
	    ("vm_reserv_insert: reserv %p's inpartpopq is TRUE", rv));
	KASSERT(bit_ntest(rv->popmap, 0, VM_LEVEL_0_NPAGES - 1, 0),
	    ("vm_reserv_insert: reserv %p's popmap is corrupted", rv));
	vm_reserv_object_lock(object);
	rv->pindex = pindex;
	rv->object = object;
	rv->lasttick = ticks;
	LIST_INSERT_HEAD(&object->rvq, rv, objq);
	vm_reserv_object_unlock(object);
}

/*
 * Reduces the given reservation's population count.  If the population count
 * becomes zero, the reservation is destroyed.  Additionally, moves the
 * reservation to the tail of the partially populated reservation queue if the
 * population count is non-zero.
 */
static void
vm_reserv_depopulate(vm_reserv_t rv, int index)
{
	struct vm_domain *vmd;

	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	KASSERT(rv->object != NULL,
	    ("vm_reserv_depopulate: reserv %p is free", rv));
	KASSERT(bit_test(rv->popmap, index),
	    ("vm_reserv_depopulate: reserv %p's popmap[%d] is clear", rv,
	    index));
	KASSERT(rv->popcnt > 0,
	    ("vm_reserv_depopulate: reserv %p's popcnt is corrupted", rv));
	KASSERT(rv->domain < vm_ndomains,
	    ("vm_reserv_depopulate: reserv %p's domain is corrupted %d",
	    rv, rv->domain));
	if (rv->popcnt == VM_LEVEL_0_NPAGES) {
		KASSERT(rv->pages->psind == 1,
		    ("vm_reserv_depopulate: reserv %p is already demoted",
		    rv));
		rv->pages->psind = 0;
	}
	bit_clear(rv->popmap, index);
	rv->popcnt--;
	if ((unsigned)(ticks - rv->lasttick) >= PARTPOPSLOP ||
	    rv->popcnt == 0) {
		vm_reserv_domain_lock(rv->domain);
		if (rv->inpartpopq) {
			TAILQ_REMOVE(&vm_rvd[rv->domain].partpop, rv, partpopq);
			rv->inpartpopq = FALSE;
		}
		if (rv->popcnt != 0) {
			rv->inpartpopq = TRUE;
			TAILQ_INSERT_TAIL(&vm_rvd[rv->domain].partpop, rv,
			    partpopq);
		}
		vm_reserv_domain_unlock(rv->domain);
		rv->lasttick = ticks;
	}
	vmd = VM_DOMAIN(rv->domain);
	if (rv->popcnt == 0) {
		vm_reserv_remove(rv);
		vm_domain_free_lock(vmd);
		vm_phys_free_pages(rv->pages, VM_LEVEL_0_ORDER);
		vm_domain_free_unlock(vmd);
		counter_u64_add(vm_reserv_freed, 1);
	}
	vm_domain_freecnt_inc(vmd, 1);
}

/*
 * Returns the reservation to which the given page might belong.
 */
static __inline vm_reserv_t
vm_reserv_from_page(vm_page_t m)
{
#ifdef VM_PHYSSEG_SPARSE
	struct vm_phys_seg *seg;

	seg = &vm_phys_segs[m->segind];
	return (seg->first_reserv + (VM_PAGE_TO_PHYS(m) >> VM_LEVEL_0_SHIFT) -
	    (seg->start >> VM_LEVEL_0_SHIFT));
#else
	return (&vm_reserv_array[VM_PAGE_TO_PHYS(m) >> VM_LEVEL_0_SHIFT]);
#endif
}

/*
 * Returns an existing reservation or NULL and initialized successor pointer.
 */
static vm_reserv_t
vm_reserv_from_object(vm_object_t object, vm_pindex_t pindex,
    vm_page_t mpred, vm_page_t *msuccp)
{
	vm_reserv_t rv;
	vm_page_t msucc;

	msucc = NULL;
	if (mpred != NULL) {
		KASSERT(mpred->object == object,
		    ("vm_reserv_from_object: object doesn't contain mpred"));
		KASSERT(mpred->pindex < pindex,
		    ("vm_reserv_from_object: mpred doesn't precede pindex"));
		rv = vm_reserv_from_page(mpred);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
		msucc = TAILQ_NEXT(mpred, listq);
	} else
		msucc = TAILQ_FIRST(&object->memq);
	if (msucc != NULL) {
		KASSERT(msucc->pindex > pindex,
		    ("vm_reserv_from_object: msucc doesn't succeed pindex"));
		rv = vm_reserv_from_page(msucc);
		if (rv->object == object && vm_reserv_has_pindex(rv, pindex))
			goto found;
	}
	rv = NULL;

found:
	*msuccp = msucc;

	return (rv);
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
 * to the tail of the partially populated reservation queue.
 */
static void
vm_reserv_populate(vm_reserv_t rv, int index)
{

	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	KASSERT(rv->object != NULL,
	    ("vm_reserv_populate: reserv %p is free", rv));
	KASSERT(!bit_test(rv->popmap, index),
	    ("vm_reserv_populate: reserv %p's popmap[%d] is set", rv,
	    index));
	KASSERT(rv->popcnt < VM_LEVEL_0_NPAGES,
	    ("vm_reserv_populate: reserv %p is already full", rv));
	KASSERT(rv->pages->psind == 0,
	    ("vm_reserv_populate: reserv %p is already promoted", rv));
	KASSERT(rv->domain < vm_ndomains,
	    ("vm_reserv_populate: reserv %p's domain is corrupted %d",
	    rv, rv->domain));
	bit_set(rv->popmap, index);
	rv->popcnt++;
	if ((unsigned)(ticks - rv->lasttick) < PARTPOPSLOP &&
	    rv->inpartpopq && rv->popcnt != VM_LEVEL_0_NPAGES)
		return;
	rv->lasttick = ticks;
	vm_reserv_domain_lock(rv->domain);
	if (rv->inpartpopq) {
		TAILQ_REMOVE(&vm_rvd[rv->domain].partpop, rv, partpopq);
		rv->inpartpopq = FALSE;
	}
	if (rv->popcnt < VM_LEVEL_0_NPAGES) {
		rv->inpartpopq = TRUE;
		TAILQ_INSERT_TAIL(&vm_rvd[rv->domain].partpop, rv, partpopq);
	} else {
		KASSERT(rv->pages->psind == 0,
		    ("vm_reserv_populate: reserv %p is already promoted",
		    rv));
		rv->pages->psind = 1;
	}
	vm_reserv_domain_unlock(rv->domain);
}

/*
 * Allocates a contiguous set of physical pages of the given size "npages"
 * from existing or newly created reservations.  All of the physical pages
 * must be at or above the given physical address "low" and below the given
 * physical address "high".  The given value "alignment" determines the
 * alignment of the first physical page in the set.  If the given value
 * "boundary" is non-zero, then the set of physical pages cannot cross any
 * physical address boundary that is a multiple of that value.  Both
 * "alignment" and "boundary" must be a power of two.
 *
 * The page "mpred" must immediately precede the offset "pindex" within the
 * specified object.
 *
 * The object must be locked.
 */
vm_page_t
vm_reserv_alloc_contig(vm_object_t object, vm_pindex_t pindex, int domain,
    int req, vm_page_t mpred, u_long npages, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary)
{
	struct vm_domain *vmd;
	vm_paddr_t pa, size;
	vm_page_t m, m_ret, msucc;
	vm_pindex_t first, leftcap, rightcap;
	vm_reserv_t rv;
	u_long allocpages, maxpages, minpages;
	int i, index, n;

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
	size = npages << PAGE_SHIFT;
	if (!vm_addr_ok(pa, size, alignment, boundary))
		return (NULL);

	/*
	 * Look for an existing reservation.
	 */
	rv = vm_reserv_from_object(object, pindex, mpred, &msucc);
	if (rv != NULL) {
		KASSERT(object != kernel_object || rv->domain == domain,
		    ("vm_reserv_alloc_contig: domain mismatch"));
		index = VM_RESERV_INDEX(object, pindex);
		/* Does the allocation fit within the reservation? */
		if (index + npages > VM_LEVEL_0_NPAGES)
			return (NULL);
		domain = rv->domain;
		vmd = VM_DOMAIN(domain);
		vm_reserv_lock(rv);
		/* Handle reclaim race. */
		if (rv->object != object)
			goto out;
		m = &rv->pages[index];
		pa = VM_PAGE_TO_PHYS(m);
		if (pa < low || pa + size > high ||
		    !vm_addr_ok(pa, size, alignment, boundary))
			goto out;
		/* Handle vm_page_rename(m, new_object, ...). */
		if (!bit_ntest(rv->popmap, index, index + npages - 1, 0))
			goto out;
		if (!vm_domain_allocate(vmd, req, npages))
			goto out;
		for (i = 0; i < npages; i++)
			vm_reserv_populate(rv, index + i);
		vm_reserv_unlock(rv);
		return (m);
out:
		vm_reserv_unlock(rv);
		return (NULL);
	}

	/*
	 * Could at least one reservation fit between the first index to the
	 * left that can be used ("leftcap") and the first index to the right
	 * that cannot be used ("rightcap")?
	 *
	 * We must synchronize with the reserv object lock to protect the
	 * pindex/object of the resulting reservations against rename while
	 * we are inspecting.
	 */
	first = pindex - VM_RESERV_INDEX(object, pindex);
	minpages = VM_RESERV_INDEX(object, pindex) + npages;
	maxpages = roundup2(minpages, VM_LEVEL_0_NPAGES);
	allocpages = maxpages;
	vm_reserv_object_lock(object);
	if (mpred != NULL) {
		if ((rv = vm_reserv_from_page(mpred))->object != object)
			leftcap = mpred->pindex + 1;
		else
			leftcap = rv->pindex + VM_LEVEL_0_NPAGES;
		if (leftcap > first) {
			vm_reserv_object_unlock(object);
			return (NULL);
		}
	}
	if (msucc != NULL) {
		if ((rv = vm_reserv_from_page(msucc))->object != object)
			rightcap = msucc->pindex;
		else
			rightcap = rv->pindex;
		if (first + maxpages > rightcap) {
			if (maxpages == VM_LEVEL_0_NPAGES) {
				vm_reserv_object_unlock(object);
				return (NULL);
			}

			/*
			 * At least one reservation will fit between "leftcap"
			 * and "rightcap".  However, a reservation for the
			 * last of the requested pages will not fit.  Reduce
			 * the size of the upcoming allocation accordingly.
			 */
			allocpages = minpages;
		}
	}
	vm_reserv_object_unlock(object);

	/*
	 * Would the last new reservation extend past the end of the object?
	 *
	 * If the object is unlikely to grow don't allocate a reservation for
	 * the tail.
	 */
	if ((object->flags & OBJ_ANON) == 0 &&
	    first + maxpages > object->size) {
		if (maxpages == VM_LEVEL_0_NPAGES)
			return (NULL);
		allocpages = minpages;
	}

	/*
	 * Allocate the physical pages.  The alignment and boundary specified
	 * for this allocation may be different from the alignment and
	 * boundary specified for the requested pages.  For instance, the
	 * specified index may not be the first page within the first new
	 * reservation.
	 */
	m = NULL;
	vmd = VM_DOMAIN(domain);
	if (vm_domain_allocate(vmd, req, npages)) {
		vm_domain_free_lock(vmd);
		m = vm_phys_alloc_contig(domain, allocpages, low, high,
		    ulmax(alignment, VM_LEVEL_0_SIZE),
		    boundary > VM_LEVEL_0_SIZE ? boundary : 0);
		vm_domain_free_unlock(vmd);
		if (m == NULL) {
			vm_domain_freecnt_inc(vmd, npages);
			return (NULL);
		}
	} else
		return (NULL);
	KASSERT(vm_page_domain(m) == domain,
	    ("vm_reserv_alloc_contig: Page domain does not match requested."));

	/*
	 * The allocated physical pages always begin at a reservation
	 * boundary, but they do not always end at a reservation boundary.
	 * Initialize every reservation that is completely covered by the
	 * allocated physical pages.
	 */
	m_ret = NULL;
	index = VM_RESERV_INDEX(object, pindex);
	do {
		rv = vm_reserv_from_page(m);
		KASSERT(rv->pages == m,
		    ("vm_reserv_alloc_contig: reserv %p's pages is corrupted",
		    rv));
		vm_reserv_lock(rv);
		vm_reserv_insert(rv, object, first);
		n = ulmin(VM_LEVEL_0_NPAGES - index, npages);
		for (i = 0; i < n; i++)
			vm_reserv_populate(rv, index + i);
		npages -= n;
		if (m_ret == NULL) {
			m_ret = &rv->pages[index];
			index = 0;
		}
		vm_reserv_unlock(rv);
		m += VM_LEVEL_0_NPAGES;
		first += VM_LEVEL_0_NPAGES;
		allocpages -= VM_LEVEL_0_NPAGES;
	} while (allocpages >= VM_LEVEL_0_NPAGES);
	return (m_ret);
}

/*
 * Allocate a physical page from an existing or newly created reservation.
 *
 * The page "mpred" must immediately precede the offset "pindex" within the
 * specified object.
 *
 * The object must be locked.
 */
vm_page_t
vm_reserv_alloc_page(vm_object_t object, vm_pindex_t pindex, int domain,
    int req, vm_page_t mpred)
{
	struct vm_domain *vmd;
	vm_page_t m, msucc;
	vm_pindex_t first, leftcap, rightcap;
	vm_reserv_t rv;
	int index;

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
	rv = vm_reserv_from_object(object, pindex, mpred, &msucc);
	if (rv != NULL) {
		KASSERT(object != kernel_object || rv->domain == domain,
		    ("vm_reserv_alloc_page: domain mismatch"));
		domain = rv->domain;
		vmd = VM_DOMAIN(domain);
		index = VM_RESERV_INDEX(object, pindex);
		m = &rv->pages[index];
		vm_reserv_lock(rv);
		/* Handle reclaim race. */
		if (rv->object != object ||
		    /* Handle vm_page_rename(m, new_object, ...). */
		    bit_test(rv->popmap, index)) {
			m = NULL;
			goto out;
		}
		if (vm_domain_allocate(vmd, req, 1) == 0)
			m = NULL;
		else
			vm_reserv_populate(rv, index);
out:
		vm_reserv_unlock(rv);
		return (m);
	}

	/*
	 * Could a reservation fit between the first index to the left that
	 * can be used and the first index to the right that cannot be used?
	 *
	 * We must synchronize with the reserv object lock to protect the
	 * pindex/object of the resulting reservations against rename while
	 * we are inspecting.
	 */
	first = pindex - VM_RESERV_INDEX(object, pindex);
	vm_reserv_object_lock(object);
	if (mpred != NULL) {
		if ((rv = vm_reserv_from_page(mpred))->object != object)
			leftcap = mpred->pindex + 1;
		else
			leftcap = rv->pindex + VM_LEVEL_0_NPAGES;
		if (leftcap > first) {
			vm_reserv_object_unlock(object);
			return (NULL);
		}
	}
	if (msucc != NULL) {
		if ((rv = vm_reserv_from_page(msucc))->object != object)
			rightcap = msucc->pindex;
		else
			rightcap = rv->pindex;
		if (first + VM_LEVEL_0_NPAGES > rightcap) {
			vm_reserv_object_unlock(object);
			return (NULL);
		}
	}
	vm_reserv_object_unlock(object);

	/*
	 * Would the last new reservation extend past the end of the object?
	 *
	 * If the object is unlikely to grow don't allocate a reservation for
	 * the tail.
	 */
	if ((object->flags & OBJ_ANON) == 0 &&
	    first + VM_LEVEL_0_NPAGES > object->size)
		return (NULL);

	/*
	 * Allocate and populate the new reservation.
	 */
	m = NULL;
	vmd = VM_DOMAIN(domain);
	if (vm_domain_allocate(vmd, req, 1)) {
		vm_domain_free_lock(vmd);
		m = vm_phys_alloc_pages(domain, VM_FREEPOOL_DEFAULT,
		    VM_LEVEL_0_ORDER);
		vm_domain_free_unlock(vmd);
		if (m == NULL) {
			vm_domain_freecnt_inc(vmd, 1);
			return (NULL);
		}
	} else
		return (NULL);
	rv = vm_reserv_from_page(m);
	vm_reserv_lock(rv);
	KASSERT(rv->pages == m,
	    ("vm_reserv_alloc_page: reserv %p's pages is corrupted", rv));
	vm_reserv_insert(rv, object, first);
	index = VM_RESERV_INDEX(object, pindex);
	vm_reserv_populate(rv, index);
	vm_reserv_unlock(rv);

	return (&rv->pages[index]);
}

/*
 * Breaks the given reservation.  All free pages in the reservation
 * are returned to the physical memory allocator.  The reservation's
 * population count and map are reset to their initial state.
 *
 * The given reservation must not be in the partially populated reservation
 * queue.
 */
static void
vm_reserv_break(vm_reserv_t rv)
{
	int hi, lo, pos;

	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	vm_reserv_remove(rv);
	rv->pages->psind = 0;
	hi = lo = -1;
	pos = 0;
	for (;;) {
		bit_ff_at(rv->popmap, pos, VM_LEVEL_0_NPAGES, lo != hi, &pos);
		if (lo == hi) {
			if (pos == -1)
				break;
			lo = pos;
			continue;
		}
		if (pos == -1)
			pos = VM_LEVEL_0_NPAGES;
		hi = pos;
		vm_domain_free_lock(VM_DOMAIN(rv->domain));
		vm_phys_enqueue_contig(&rv->pages[lo], hi - lo);
		vm_domain_free_unlock(VM_DOMAIN(rv->domain));
		lo = hi;
	}
	bit_nclear(rv->popmap, 0, VM_LEVEL_0_NPAGES - 1);
	rv->popcnt = 0;
	counter_u64_add(vm_reserv_broken, 1);
}

/*
 * Breaks all reservations belonging to the given object.
 */
void
vm_reserv_break_all(vm_object_t object)
{
	vm_reserv_t rv;

	/*
	 * This access of object->rvq is unsynchronized so that the
	 * object rvq lock can nest after the domain_free lock.  We
	 * must check for races in the results.  However, the object
	 * lock prevents new additions, so we are guaranteed that when
	 * it returns NULL the object is properly empty.
	 */
	while ((rv = LIST_FIRST(&object->rvq)) != NULL) {
		vm_reserv_lock(rv);
		/* Reclaim race. */
		if (rv->object != object) {
			vm_reserv_unlock(rv);
			continue;
		}
		vm_reserv_domain_lock(rv->domain);
		if (rv->inpartpopq) {
			TAILQ_REMOVE(&vm_rvd[rv->domain].partpop, rv, partpopq);
			rv->inpartpopq = FALSE;
		}
		vm_reserv_domain_unlock(rv->domain);
		vm_reserv_break(rv);
		vm_reserv_unlock(rv);
	}
}

/*
 * Frees the given page if it belongs to a reservation.  Returns TRUE if the
 * page is freed and FALSE otherwise.
 */
boolean_t
vm_reserv_free_page(vm_page_t m)
{
	vm_reserv_t rv;
	boolean_t ret;

	rv = vm_reserv_from_page(m);
	if (rv->object == NULL)
		return (FALSE);
	vm_reserv_lock(rv);
	/* Re-validate after lock. */
	if (rv->object != NULL) {
		vm_reserv_depopulate(rv, m - rv->pages);
		ret = TRUE;
	} else
		ret = FALSE;
	vm_reserv_unlock(rv);

	return (ret);
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
	struct vm_phys_seg *seg;
	struct vm_reserv *rv;
	struct vm_reserv_domain *rvd;
#ifdef VM_PHYSSEG_SPARSE
	vm_pindex_t used;
#endif
	int i, segind;

	/*
	 * Initialize the reservation array.  Specifically, initialize the
	 * "pages" field for every element that has an underlying superpage.
	 */
#ifdef VM_PHYSSEG_SPARSE
	used = 0;
#endif
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
#ifdef VM_PHYSSEG_SPARSE
		seg->first_reserv = &vm_reserv_array[used];
		used += howmany(seg->end, VM_LEVEL_0_SIZE) -
		    seg->start / VM_LEVEL_0_SIZE;
#else
		seg->first_reserv =
		    &vm_reserv_array[seg->start >> VM_LEVEL_0_SHIFT];
#endif
		paddr = roundup2(seg->start, VM_LEVEL_0_SIZE);
		rv = seg->first_reserv + (paddr >> VM_LEVEL_0_SHIFT) -
		    (seg->start >> VM_LEVEL_0_SHIFT);
		while (paddr + VM_LEVEL_0_SIZE > paddr && paddr +
		    VM_LEVEL_0_SIZE <= seg->end) {
			rv->pages = PHYS_TO_VM_PAGE(paddr);
			rv->domain = seg->domain;
			mtx_init(&rv->lock, "vm reserv", NULL, MTX_DEF);
			paddr += VM_LEVEL_0_SIZE;
			rv++;
		}
	}
	for (i = 0; i < MAXMEMDOM; i++) {
		rvd = &vm_rvd[i];
		mtx_init(&rvd->lock, "vm reserv domain", NULL, MTX_DEF);
		TAILQ_INIT(&rvd->partpop);
		mtx_init(&rvd->marker.lock, "vm reserv marker", NULL, MTX_DEF);

		/*
		 * Fully populated reservations should never be present in the
		 * partially populated reservation queues.
		 */
		rvd->marker.popcnt = VM_LEVEL_0_NPAGES;
		bit_nset(rvd->marker.popmap, 0, VM_LEVEL_0_NPAGES - 1);
	}

	for (i = 0; i < VM_RESERV_OBJ_LOCK_COUNT; i++)
		mtx_init(&vm_reserv_object_mtx[i], "resv obj lock", NULL,
		    MTX_DEF);
}

/*
 * Returns true if the given page belongs to a reservation and that page is
 * free.  Otherwise, returns false.
 */
bool
vm_reserv_is_page_free(vm_page_t m)
{
	vm_reserv_t rv;

	rv = vm_reserv_from_page(m);
	if (rv->object == NULL)
		return (false);
	return (!bit_test(rv->popmap, m - rv->pages));
}

/*
 * Returns true if the given page is part of a block of npages, starting at a
 * multiple of npages, that are all allocated.  Otherwise, returns false.
 */
bool
vm_reserv_is_populated(vm_page_t m, int npages)
{
	vm_reserv_t rv;
	int index;

	KASSERT(npages <= VM_LEVEL_0_NPAGES,
	    ("%s: npages %d exceeds VM_LEVEL_0_NPAGES", __func__, npages));
	KASSERT(powerof2(npages),
	    ("%s: npages %d is not a power of 2", __func__, npages));
	rv = vm_reserv_from_page(m);
	if (rv->object == NULL)
		return (false);
	index = rounddown2(m - rv->pages, npages);
	return (bit_ntest(rv->popmap, index, index + npages - 1, 1));
}

/*
 * If the given page belongs to a reservation, returns the level of that
 * reservation.  Otherwise, returns -1.
 */
int
vm_reserv_level(vm_page_t m)
{
	vm_reserv_t rv;

	rv = vm_reserv_from_page(m);
	return (rv->object != NULL ? 0 : -1);
}

/*
 * Returns a reservation level if the given page belongs to a fully populated
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
 * Remove a partially populated reservation from the queue.
 */
static void
vm_reserv_dequeue(vm_reserv_t rv)
{

	vm_reserv_domain_assert_locked(rv->domain);
	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	KASSERT(rv->inpartpopq,
	    ("vm_reserv_reclaim: reserv %p's inpartpopq is FALSE", rv));

	TAILQ_REMOVE(&vm_rvd[rv->domain].partpop, rv, partpopq);
	rv->inpartpopq = FALSE;
}

/*
 * Breaks the given partially populated reservation, releasing its free pages
 * to the physical memory allocator.
 */
static void
vm_reserv_reclaim(vm_reserv_t rv)
{

	vm_reserv_assert_locked(rv);
	CTR5(KTR_VM, "%s: rv %p object %p popcnt %d inpartpop %d",
	    __FUNCTION__, rv, rv->object, rv->popcnt, rv->inpartpopq);
	if (rv->inpartpopq) {
		vm_reserv_domain_lock(rv->domain);
		vm_reserv_dequeue(rv);
		vm_reserv_domain_unlock(rv->domain);
	}
	vm_reserv_break(rv);
	counter_u64_add(vm_reserv_reclaimed, 1);
}

/*
 * Breaks a reservation near the head of the partially populated reservation
 * queue, releasing its free pages to the physical memory allocator.  Returns
 * TRUE if a reservation is broken and FALSE otherwise.
 */
bool
vm_reserv_reclaim_inactive(int domain)
{
	vm_reserv_t rv;

	vm_reserv_domain_lock(domain);
	TAILQ_FOREACH(rv, &vm_rvd[domain].partpop, partpopq) {
		/*
		 * A locked reservation is likely being updated or reclaimed,
		 * so just skip ahead.
		 */
		if (rv != &vm_rvd[domain].marker && vm_reserv_trylock(rv)) {
			vm_reserv_dequeue(rv);
			break;
		}
	}
	vm_reserv_domain_unlock(domain);
	if (rv != NULL) {
		vm_reserv_reclaim(rv);
		vm_reserv_unlock(rv);
		return (true);
	}
	return (false);
}

/*
 * Determine whether this reservation has free pages that satisfy the given
 * request for contiguous physical memory.  Start searching from the lower
 * bound, defined by lo, and stop at the upper bound, hi.  Return the index
 * of the first satisfactory free page, or -1 if none is found.
 */
static int
vm_reserv_find_contig(vm_reserv_t rv, int npages, int lo,
    int hi, int ppn_align, int ppn_bound)
{

	vm_reserv_assert_locked(rv);
	KASSERT(npages <= VM_LEVEL_0_NPAGES - 1,
	    ("%s: Too many pages", __func__));
	KASSERT(ppn_bound <= VM_LEVEL_0_NPAGES,
	    ("%s: Too big a boundary for reservation size", __func__));
	KASSERT(npages <= ppn_bound,
	    ("%s: Too many pages for given boundary", __func__));
	KASSERT(ppn_align != 0 && powerof2(ppn_align),
	    ("ppn_align is not a positive power of 2"));
	KASSERT(ppn_bound != 0 && powerof2(ppn_bound),
	    ("ppn_bound is not a positive power of 2"));
	while (bit_ffc_area_at(rv->popmap, lo, hi, npages, &lo), lo != -1) {
		if (lo < roundup2(lo, ppn_align)) {
			/* Skip to next aligned page. */
			lo = roundup2(lo, ppn_align);
		} else if (roundup2(lo + 1, ppn_bound) >= lo + npages)
			return (lo);
		if (roundup2(lo + 1, ppn_bound) < lo + npages) {
			/* Skip to next boundary-matching page. */
			lo = roundup2(lo + 1, ppn_bound);
		}
	}
	return (-1);
}

/*
 * Searches the partially populated reservation queue for the least recently
 * changed reservation with free pages that satisfy the given request for
 * contiguous physical memory.  If a satisfactory reservation is found, it is
 * broken.  Returns true if a reservation is broken and false otherwise.
 */
vm_page_t
vm_reserv_reclaim_contig(int domain, u_long npages, vm_paddr_t low,
    vm_paddr_t high, u_long alignment, vm_paddr_t boundary)
{
	struct vm_reserv_queue *queue;
	vm_paddr_t pa, size;
	vm_page_t m_ret;
	vm_reserv_t marker, rv, rvn;
	int hi, lo, posn, ppn_align, ppn_bound;

	KASSERT(npages > 0, ("npages is 0"));
	KASSERT(powerof2(alignment), ("alignment is not a power of 2"));
	KASSERT(powerof2(boundary), ("boundary is not a power of 2"));
	if (npages > VM_LEVEL_0_NPAGES - 1)
		return (false);
	size = npages << PAGE_SHIFT;
	/* 
	 * Ensure that a free range starting at a boundary-multiple
	 * doesn't include a boundary-multiple within it.  Otherwise,
	 * no boundary-constrained allocation is possible.
	 */
	if (!vm_addr_bound_ok(0, size, boundary))
		return (NULL);
	marker = &vm_rvd[domain].marker;
	queue = &vm_rvd[domain].partpop;
	/*
	 * Compute shifted alignment, boundary values for page-based
	 * calculations.  Constrain to range [1, VM_LEVEL_0_NPAGES] to
	 * avoid overflow.
	 */
	ppn_align = (int)(ulmin(ulmax(PAGE_SIZE, alignment),
	    VM_LEVEL_0_SIZE) >> PAGE_SHIFT);
	ppn_bound = boundary == 0 ? VM_LEVEL_0_NPAGES :
	    (int)(MIN(MAX(PAGE_SIZE, boundary),
            VM_LEVEL_0_SIZE) >> PAGE_SHIFT);

	vm_reserv_domain_scan_lock(domain);
	vm_reserv_domain_lock(domain);
	TAILQ_FOREACH_SAFE(rv, queue, partpopq, rvn) {
		pa = VM_PAGE_TO_PHYS(&rv->pages[0]);
		if (pa + VM_LEVEL_0_SIZE - size < low) {
			/* This entire reservation is too low; go to next. */
			continue;
		}
		if (pa + size > high) {
			/* This entire reservation is too high; go to next. */
			continue;
		}
		if (!vm_addr_align_ok(pa, alignment)) {
			/* This entire reservation is unaligned; go to next. */
			continue;
		}

		if (vm_reserv_trylock(rv) == 0) {
			TAILQ_INSERT_AFTER(queue, rv, marker, partpopq);
			vm_reserv_domain_unlock(domain);
			vm_reserv_lock(rv);
			if (TAILQ_PREV(marker, vm_reserv_queue, partpopq) !=
			    rv) {
				vm_reserv_unlock(rv);
				vm_reserv_domain_lock(domain);
				rvn = TAILQ_NEXT(marker, partpopq);
				TAILQ_REMOVE(queue, marker, partpopq);
				continue;
			}
			vm_reserv_domain_lock(domain);
			TAILQ_REMOVE(queue, marker, partpopq);
		}
		vm_reserv_domain_unlock(domain);
		lo = (pa >= low) ? 0 :
		    (int)((low + PAGE_MASK - pa) >> PAGE_SHIFT);
		hi = (pa + VM_LEVEL_0_SIZE <= high) ? VM_LEVEL_0_NPAGES :
		    (int)((high - pa) >> PAGE_SHIFT);
		posn = vm_reserv_find_contig(rv, (int)npages, lo, hi,
		    ppn_align, ppn_bound);
		if (posn >= 0) {
			vm_reserv_domain_scan_unlock(domain);
			/* Allocate requested space */
			rv->popcnt += npages;
			bit_nset(rv->popmap, posn, posn + npages - 1);
			vm_reserv_reclaim(rv);
			vm_reserv_unlock(rv);
			m_ret = &rv->pages[posn];
			pa = VM_PAGE_TO_PHYS(m_ret);
			KASSERT(vm_addr_ok(pa, size, alignment, boundary),
			    ("%s: adjusted address not aligned/bounded to "
			     "%lx/%jx",
			     __func__, alignment, (uintmax_t)boundary));
			return (m_ret);
		}
		vm_reserv_domain_lock(domain);
		rvn = TAILQ_NEXT(rv, partpopq);
		vm_reserv_unlock(rv);
	}
	vm_reserv_domain_unlock(domain);
	vm_reserv_domain_scan_unlock(domain);
	return (NULL);
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
		vm_reserv_lock(rv);
		CTR6(KTR_VM,
		    "%s: rv %p object %p new %p popcnt %d inpartpop %d",
		    __FUNCTION__, rv, rv->object, new_object, rv->popcnt,
		    rv->inpartpopq);
		if (rv->object == old_object) {
			vm_reserv_object_lock(old_object);
			rv->object = NULL;
			LIST_REMOVE(rv, objq);
			vm_reserv_object_unlock(old_object);
			vm_reserv_object_lock(new_object);
			rv->object = new_object;
			rv->pindex -= old_object_offset;
			LIST_INSERT_HEAD(&new_object->rvq, rv, objq);
			vm_reserv_object_unlock(new_object);
		}
		vm_reserv_unlock(rv);
	}
}

/*
 * Returns the size (in bytes) of a reservation of the specified level.
 */
int
vm_reserv_size(int level)
{

	switch (level) {
	case 0:
		return (VM_LEVEL_0_SIZE);
	case -1:
		return (PAGE_SIZE);
	default:
		return (0);
	}
}

/*
 * Allocates the virtual and physical memory required by the reservation
 * management system's data structures, in particular, the reservation array.
 */
vm_paddr_t
vm_reserv_startup(vm_offset_t *vaddr, vm_paddr_t end)
{
	vm_paddr_t new_end;
	vm_pindex_t count;
	size_t size;
	int i;

	count = 0;
	for (i = 0; i < vm_phys_nsegs; i++) {
#ifdef VM_PHYSSEG_SPARSE
		count += howmany(vm_phys_segs[i].end, VM_LEVEL_0_SIZE) -
		    vm_phys_segs[i].start / VM_LEVEL_0_SIZE;
#else
		count = MAX(count,
		    howmany(vm_phys_segs[i].end, VM_LEVEL_0_SIZE));
#endif
	}

	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
#ifdef VM_PHYSSEG_SPARSE
		count += howmany(phys_avail[i + 1], VM_LEVEL_0_SIZE) -
		    phys_avail[i] / VM_LEVEL_0_SIZE;
#else
		count = MAX(count,
		    howmany(phys_avail[i + 1], VM_LEVEL_0_SIZE));
#endif
	}

	/*
	 * Calculate the size (in bytes) of the reservation array.  Rounding up
	 * for partial superpages at boundaries, as every small page is mapped
	 * to an element in the reservation array based on its physical address.
	 * Thus, the number of elements in the reservation array can be greater
	 * than the number of superpages.
	 */
	size = count * sizeof(struct vm_reserv);

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

/*
 * Returns the superpage containing the given page.
 */
vm_page_t
vm_reserv_to_superpage(vm_page_t m)
{
	vm_reserv_t rv;

	VM_OBJECT_ASSERT_LOCKED(m->object);
	rv = vm_reserv_from_page(m);
	if (rv->object == m->object && rv->popcnt == VM_LEVEL_0_NPAGES)
		m = rv->pages;
	else
		m = NULL;

	return (m);
}

#endif	/* VM_NRESERVLEVEL > 0 */
