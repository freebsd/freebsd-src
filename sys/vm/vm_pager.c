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
 *	from: @(#)vm_pager.c	8.6 (Berkeley) 1/12/94
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
 * $Id: vm_pager.c,v 1.29 1997/09/01 03:17:28 bde Exp $
 */

/*
 *	Paging space routine stubs.  Emulates a matchmaker-like interface
 *	for builtin pagers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ucred.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

MALLOC_DEFINE(M_VMPGDATA, "VM pgdata", "XXX: VM pager private data");

extern struct pagerops defaultpagerops;
extern struct pagerops swappagerops;
extern struct pagerops vnodepagerops;
extern struct pagerops devicepagerops;

static struct pagerops *pagertab[] = {
	&defaultpagerops,	/* OBJT_DEFAULT */
	&swappagerops,		/* OBJT_SWAP */
	&vnodepagerops,		/* OBJT_VNODE */
	&devicepagerops,	/* OBJT_DEVICE */
};
static int npagers = sizeof(pagertab) / sizeof(pagertab[0]);

/*
 * Kernel address space for mapping pages.
 * Used by pagers where KVAs are needed for IO.
 *
 * XXX needs to be large enough to support the number of pending async
 * cleaning requests (NPENDINGIO == 64) * the maximum swap cluster size
 * (MAXPHYS == 64k) if you want to get the most efficiency.
 */
#define PAGER_MAP_SIZE	(8 * 1024 * 1024)

int pager_map_size = PAGER_MAP_SIZE;
vm_map_t pager_map;
static int bswneeded;
static vm_offset_t swapbkva;		/* swap buffers kva */

void
vm_pager_init()
{
	struct pagerops **pgops;

	/*
	 * Initialize known pagers
	 */
	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (pgops && ((*pgops)->pgo_init != NULL))
			(*(*pgops)->pgo_init) ();
}

void
vm_pager_bufferinit()
{
	struct buf *bp;
	int i;

	bp = swbuf;
	/*
	 * Now set up swap and physical I/O buffer headers.
	 */
	for (i = 0; i < nswbuf - 1; i++, bp++) {
		TAILQ_INSERT_HEAD(&bswlist, bp, b_freelist);
		bp->b_rcred = bp->b_wcred = NOCRED;
		bp->b_vnbufs.le_next = NOLIST;
	}
	bp->b_rcred = bp->b_wcred = NOCRED;
	bp->b_vnbufs.le_next = NOLIST;

	swapbkva = kmem_alloc_pageable(pager_map, nswbuf * MAXPHYS);
	if (!swapbkva)
		panic("Not enough pager_map VM space for physical buffers");
}

/*
 * Allocate an instance of a pager of the given type.
 * Size, protection and offset parameters are passed in for pagers that
 * need to perform page-level validation (e.g. the device pager).
 */
vm_object_t
vm_pager_allocate(objtype_t type, void *handle, vm_size_t size, vm_prot_t prot,
		  vm_ooffset_t off)
{
	struct pagerops *ops;

	ops = pagertab[type];
	if (ops)
		return ((*ops->pgo_alloc) (handle, size, prot, off));
	return (NULL);
}

void
vm_pager_deallocate(object)
	vm_object_t object;
{
	(*pagertab[object->type]->pgo_dealloc) (object);
}


int
vm_pager_get_pages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	return ((*pagertab[object->type]->pgo_getpages)(object, m, count, reqpage));
}

int
vm_pager_put_pages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	return ((*pagertab[object->type]->pgo_putpages)(object, m, count, sync, rtvals));
}

boolean_t
vm_pager_has_page(object, offset, before, after)
	vm_object_t object;
	vm_pindex_t offset;
	int *before;
	int *after;
{
	return ((*pagertab[object->type]->pgo_haspage) (object, offset, before, after));
}

/*
 * Called by pageout daemon before going back to sleep.
 * Gives pagers a chance to clean up any completed async pageing operations.
 */
void
vm_pager_sync()
{
	struct pagerops **pgops;

	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (pgops && ((*pgops)->pgo_sync != NULL))
			(*(*pgops)->pgo_sync) ();
}

vm_offset_t
vm_pager_map_page(m)
	vm_page_t m;
{
	vm_offset_t kva;

	kva = kmem_alloc_wait(pager_map, PAGE_SIZE);
	pmap_kenter(kva, VM_PAGE_TO_PHYS(m));
	return (kva);
}

void
vm_pager_unmap_page(kva)
	vm_offset_t kva;
{
	pmap_kremove(kva);
	kmem_free_wakeup(pager_map, kva, PAGE_SIZE);
}

vm_object_t
vm_pager_object_lookup(pg_list, handle)
	register struct pagerlst *pg_list;
	void *handle;
{
	register vm_object_t object;

	for (object = TAILQ_FIRST(pg_list); object != NULL; object = TAILQ_NEXT(object,pager_object_list))
		if (object->handle == handle)
			return (object);
	return (NULL);
}

/*
 * This routine loses a reference to the object -
 * thus a reference must be gained before calling.
 */
int
pager_cache(object, should_cache)
	vm_object_t object;
	boolean_t should_cache;
{
	if (object == NULL)
		return (KERN_INVALID_ARGUMENT);

	if (should_cache)
		object->flags |= OBJ_CANPERSIST;
	else
		object->flags &= ~OBJ_CANPERSIST;

	vm_object_deallocate(object);

	return (KERN_SUCCESS);
}

/*
 * initialize a physical buffer
 */

static void
initpbuf(struct buf *bp) {
	bzero(bp, sizeof *bp);
	bp->b_rcred = NOCRED;
	bp->b_wcred = NOCRED;
	bp->b_qindex = QUEUE_NONE;
	bp->b_data = (caddr_t) (MAXPHYS * (bp - swbuf)) + swapbkva;
	bp->b_kvabase = bp->b_data;
	bp->b_kvasize = MAXPHYS;
	bp->b_vnbufs.le_next = NOLIST;
}

/*
 * allocate a physical buffer
 */
struct buf *
getpbuf()
{
	int s;
	struct buf *bp;

	s = splbio();
	/* get a bp from the swap buffer header pool */
	while ((bp = TAILQ_FIRST(&bswlist)) == NULL) {
		bswneeded = 1;
		tsleep(&bswneeded, PVM, "wswbuf", 0);
	}
	TAILQ_REMOVE(&bswlist, bp, b_freelist);
	splx(s);

	initpbuf(bp);
	return bp;
}

/*
 * allocate a physical buffer, if one is available
 */
struct buf *
trypbuf()
{
	int s;
	struct buf *bp;

	s = splbio();
	if ((bp = TAILQ_FIRST(&bswlist)) == NULL) {
		splx(s);
		return NULL;
	}
	TAILQ_REMOVE(&bswlist, bp, b_freelist);
	splx(s);

	initpbuf(bp);

	return bp;
}

/*
 * release a physical buffer
 */
void
relpbuf(bp)
	struct buf *bp;
{
	int s;

	s = splbio();

	if (bp->b_rcred != NOCRED) {
		crfree(bp->b_rcred);
		bp->b_rcred = NOCRED;
	}
	if (bp->b_wcred != NOCRED) {
		crfree(bp->b_wcred);
		bp->b_wcred = NOCRED;
	}
	if (bp->b_vp)
		pbrelvp(bp);

	if (bp->b_flags & B_WANTED)
		wakeup(bp);

	TAILQ_INSERT_HEAD(&bswlist, bp, b_freelist);

	if (bswneeded) {
		bswneeded = 0;
		wakeup(&bswneeded);
	}
	splx(s);
}
