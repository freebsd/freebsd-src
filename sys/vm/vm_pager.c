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
 * $Id: vm_pager.c,v 1.10 1994/12/23 04:56:51 davidg Exp $
 */

/*
 *	Paging space routine stubs.  Emulates a matchmaker-like interface
 *	for builtin pagers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ucred.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

extern struct pagerops swappagerops;
extern struct pagerops vnodepagerops;
extern struct pagerops devicepagerops;

struct pagerops *pagertab[] = {
	&swappagerops,		/* PG_SWAP */
	&vnodepagerops,		/* PG_VNODE */
	&devicepagerops,	/* PG_DEV */
};
int npagers = sizeof(pagertab) / sizeof(pagertab[0]);

struct pagerops *dfltpagerops = NULL;	/* default pager */

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
boolean_t pager_map_wanted;
vm_offset_t pager_sva, pager_eva;
int bswneeded;
vm_offset_t swapbkva;		/* swap buffers kva */

void
vm_pager_init()
{
	struct pagerops **pgops;

	/*
	 * Initialize known pagers
	 */
	for (pgops = pagertab; pgops < &pagertab[npagers]; pgops++)
		if (pgops)
			(*(*pgops)->pgo_init) ();
	if (dfltpagerops == NULL)
		panic("no default pager");
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
	bp->b_actf = NULL;

	swapbkva = kmem_alloc_pageable(pager_map, nswbuf * MAXPHYS);
	if (!swapbkva)
		panic("Not enough pager_map VM space for physical buffers");
}

/*
 * Allocate an instance of a pager of the given type.
 * Size, protection and offset parameters are passed in for pagers that
 * need to perform page-level validation (e.g. the device pager).
 */
vm_pager_t
vm_pager_allocate(type, handle, size, prot, off)
	int type;
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t off;
{
	struct pagerops *ops;

	ops = (type == PG_DFLT) ? dfltpagerops : pagertab[type];
	if (ops)
		return ((*ops->pgo_alloc) (handle, size, prot, off));
	return (NULL);
}

void
vm_pager_deallocate(pager)
	vm_pager_t pager;
{
	if (pager == NULL)
		panic("vm_pager_deallocate: null pager");

	(*pager->pg_ops->pgo_dealloc) (pager);
}


int
vm_pager_get_pages(pager, m, count, reqpage, sync)
	vm_pager_t pager;
	vm_page_t *m;
	int count;
	int reqpage;
	boolean_t sync;
{
	int i;

	if (pager == NULL) {
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				PAGE_WAKEUP(m[i]);
				vm_page_free(m[i]);
			}
		}
		vm_page_zero_fill(m[reqpage]);
		return VM_PAGER_OK;
	}
	if (pager->pg_ops->pgo_getpages == 0) {
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				PAGE_WAKEUP(m[i]);
				vm_page_free(m[i]);
			}
		}
		return (VM_PAGER_GET(pager, m[reqpage], sync));
	} else {
		return (VM_PAGER_GET_MULTI(pager, m, count, reqpage, sync));
	}
}

int
vm_pager_put_pages(pager, m, count, sync, rtvals)
	vm_pager_t pager;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	int i;

	if (pager->pg_ops->pgo_putpages)
		return (VM_PAGER_PUT_MULTI(pager, m, count, sync, rtvals));
	else {
		for (i = 0; i < count; i++) {
			rtvals[i] = VM_PAGER_PUT(pager, m[i], sync);
		}
		return rtvals[0];
	}
}

boolean_t
vm_pager_has_page(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	if (pager == NULL)
		panic("vm_pager_has_page: null pager");
	return ((*pager->pg_ops->pgo_haspage) (pager, offset));
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
		if (pgops)
			(*(*pgops)->pgo_putpage) (NULL, NULL, 0);
}

#if 0
void
vm_pager_cluster(pager, offset, loff, hoff)
	vm_pager_t pager;
	vm_offset_t offset;
	vm_offset_t *loff;
	vm_offset_t *hoff;
{
	if (pager == NULL)
		panic("vm_pager_cluster: null pager");
	return ((*pager->pg_ops->pgo_cluster) (pager, offset, loff, hoff));
}
#endif

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

vm_page_t
vm_pager_atop(kva)
	vm_offset_t kva;
{
	vm_offset_t pa;

	pa = pmap_kextract(kva);
	if (pa == 0)
		panic("vm_pager_atop");
	return (PHYS_TO_VM_PAGE(pa));
}

vm_pager_t
vm_pager_lookup(pglist, handle)
	register struct pagerlst *pglist;
	caddr_t handle;
{
	register vm_pager_t pager;

	for (pager = pglist->tqh_first; pager; pager = pager->pg_list.tqe_next)
		if (pager->pg_handle == handle)
			return (pager);
	return (NULL);
}

/*
 * This routine gains a reference to the object.
 * Explicit deallocation is necessary.
 */
int
pager_cache(object, should_cache)
	vm_object_t object;
	boolean_t should_cache;
{
	if (object == NULL)
		return (KERN_INVALID_ARGUMENT);

	vm_object_cache_lock();
	vm_object_lock(object);
	if (should_cache)
		object->flags |= OBJ_CANPERSIST;
	else
		object->flags &= ~OBJ_CANPERSIST;
	vm_object_unlock(object);
	vm_object_cache_unlock();

	vm_object_deallocate(object);

	return (KERN_SUCCESS);
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
	while ((bp = bswlist.tqh_first) == NULL) {
		bswneeded = 1;
		tsleep((caddr_t) & bswneeded, PVM, "wswbuf", 0);
	}
	TAILQ_REMOVE(&bswlist, bp, b_freelist);
	splx(s);

	bzero(bp, sizeof *bp);
	bp->b_rcred = NOCRED;
	bp->b_wcred = NOCRED;
	bp->b_data = (caddr_t) (MAXPHYS * (bp - swbuf)) + swapbkva;
	bp->b_vnbufs.le_next = NOLIST;
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
	if ((bp = bswlist.tqh_first) == NULL) {
		splx(s);
		return NULL;
	}
	TAILQ_REMOVE(&bswlist, bp, b_freelist);
	splx(s);

	bzero(bp, sizeof *bp);
	bp->b_rcred = NOCRED;
	bp->b_wcred = NOCRED;
	bp->b_data = (caddr_t) (MAXPHYS * (bp - swbuf)) + swapbkva;
	bp->b_vnbufs.le_next = NOLIST;
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
		wakeup((caddr_t) bp);

	TAILQ_INSERT_HEAD(&bswlist, bp, b_freelist);

	if (bswneeded) {
		bswneeded = 0;
		wakeup((caddr_t) & bswlist);
	}
	splx(s);
}
