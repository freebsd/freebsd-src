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
 *	@(#)vm_pageout.c	8.7 (Berkeley) 6/19/95
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
 *	The proverbial page-out daemon.
 */

#include <sys/param.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#ifndef VM_PAGE_FREE_MIN
#define VM_PAGE_FREE_MIN	(cnt.v_free_count / 20)
#endif

#ifndef VM_PAGE_FREE_TARGET
#define VM_PAGE_FREE_TARGET	((cnt.v_free_min * 4) / 3)
#endif

int	vm_page_free_min_min = 16 * 1024;
int	vm_page_free_min_max = 256 * 1024;

int	vm_pages_needed;	/* Event on which pageout daemon sleeps */

int	vm_page_max_wired = 0;	/* XXX max # of wired pages system-wide */

#ifdef CLUSTERED_PAGEOUT
#define MAXPOCLUSTER		(MAXPHYS/NBPG)	/* XXX */
int doclustered_pageout = 1;
#endif

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
void
vm_pageout_scan()
{
	register vm_page_t	m, next;
	register int		page_shortage;
	register int		s;
	register int		pages_freed;
	int			free;
	vm_object_t		object;

	/*
	 *	Only continue when we want more pages to be "free"
	 */

	cnt.v_rev++;

	s = splimp();
	simple_lock(&vm_page_queue_free_lock);
	free = cnt.v_free_count;
	simple_unlock(&vm_page_queue_free_lock);
	splx(s);

	if (free < cnt.v_free_target) {
		swapout_threads();

		/*
		 *	Be sure the pmap system is updated so
		 *	we can scan the inactive queue.
		 */

		pmap_update();
	}

	/*
	 *	Acquire the resident page system lock,
	 *	as we may be changing what's resident quite a bit.
	 */
	vm_page_lock_queues();

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 */

	pages_freed = 0;
	for (m = vm_page_queue_inactive.tqh_first; m != NULL; m = next) {
		s = splimp();
		simple_lock(&vm_page_queue_free_lock);
		free = cnt.v_free_count;
		simple_unlock(&vm_page_queue_free_lock);
		splx(s);
		if (free >= cnt.v_free_target)
			break;

		cnt.v_scan++;
		next = m->pageq.tqe_next;

		/*
		 * If the page has been referenced, move it back to the
		 * active queue.
		 */
		if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			vm_page_activate(m);
			cnt.v_reactivated++;
			continue;
		}

		/*
		 * If the page is clean, free it up.
		 */
		if (m->flags & PG_CLEAN) {
			object = m->object;
			if (vm_object_lock_try(object)) {
				pmap_page_protect(VM_PAGE_TO_PHYS(m),
						  VM_PROT_NONE);
				vm_page_free(m);
				pages_freed++;
				cnt.v_dfree++;
				vm_object_unlock(object);
			}
			continue;
		}

		/*
		 * If the page is dirty but already being washed, skip it.
		 */
		if ((m->flags & PG_LAUNDRY) == 0)
			continue;

		/*
		 * Otherwise the page is dirty and still in the laundry,
		 * so we start the cleaning operation and remove it from
		 * the laundry.
		 */
		object = m->object;
		if (!vm_object_lock_try(object))
			continue;
		cnt.v_pageouts++;
#ifdef CLUSTERED_PAGEOUT
		if (object->pager &&
		    vm_pager_cancluster(object->pager, PG_CLUSTERPUT))
			vm_pageout_cluster(m, object);
		else
#endif
		vm_pageout_page(m, object);
		thread_wakeup(object);
		vm_object_unlock(object);
		/*
		 * Former next page may no longer even be on the inactive
		 * queue (due to potential blocking in the pager with the
		 * queues unlocked).  If it isn't, we just start over.
		 */
		if (next && (next->flags & PG_INACTIVE) == 0)
			next = vm_page_queue_inactive.tqh_first;
	}
	
	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

	page_shortage = cnt.v_inactive_target - cnt.v_inactive_count;
	if (page_shortage <= 0 && pages_freed == 0)
		page_shortage = 1;

	while (page_shortage > 0) {
		/*
		 *	Move some more pages from active to inactive.
		 */

		if ((m = vm_page_queue_active.tqh_first) == NULL)
			break;
		vm_page_deactivate(m);
		page_shortage--;
	}

	vm_page_unlock_queues();
}

/*
 * Called with object and page queues locked.
 * If reactivate is TRUE, a pager error causes the page to be
 * put back on the active queue, ow it is left on the inactive queue.
 */
void
vm_pageout_page(m, object)
	vm_page_t m;
	vm_object_t object;
{
	vm_pager_t pager;
	int pageout_status;

	/*
	 * We set the busy bit to cause potential page faults on
	 * this page to block.
	 *
	 * We also set pageout-in-progress to keep the object from
	 * disappearing during pageout.  This guarantees that the
	 * page won't move from the inactive queue.  (However, any
	 * other page on the inactive queue may move!)
	 */
	pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
	m->flags |= PG_BUSY;

	/*
	 * Try to collapse the object before making a pager for it.
	 * We must unlock the page queues first.
	 */
	vm_page_unlock_queues();
	if (object->pager == NULL)
		vm_object_collapse(object);

	object->paging_in_progress++;
	vm_object_unlock(object);

	/*
	 * Do a wakeup here in case the following operations block.
	 */
	thread_wakeup(&cnt.v_free_count);

	/*
	 * If there is no pager for the page, use the default pager.
	 * If there is no place to put the page at the moment,
	 * leave it in the laundry and hope that there will be
	 * paging space later.
	 */
	if ((pager = object->pager) == NULL) {
		pager = vm_pager_allocate(PG_DFLT, (caddr_t)0, object->size,
					  VM_PROT_ALL, (vm_offset_t)0);
		if (pager != NULL)
			vm_object_setpager(object, pager, 0, FALSE);
	}
	pageout_status = pager ? vm_pager_put(pager, m, FALSE) : VM_PAGER_FAIL;
	vm_object_lock(object);
	vm_page_lock_queues();

	switch (pageout_status) {
	case VM_PAGER_OK:
	case VM_PAGER_PEND:
		cnt.v_pgpgout++;
		m->flags &= ~PG_LAUNDRY;
		break;
	case VM_PAGER_BAD:
		/*
		 * Page outside of range of object.  Right now we
		 * essentially lose the changes by pretending it
		 * worked.
		 *
		 * XXX dubious, what should we do?
		 */
		m->flags &= ~PG_LAUNDRY;
		m->flags |= PG_CLEAN;
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
		break;
	case VM_PAGER_AGAIN:
	{
		extern int lbolt;

		/*
		 * FAIL on a write is interpreted to mean a resource
		 * shortage, so we put pause for awhile and try again.
		 * XXX could get stuck here.
		 */
		vm_page_unlock_queues();
		vm_object_unlock(object);
		(void) tsleep((caddr_t)&lbolt, PZERO|PCATCH, "pageout", 0);
		vm_object_lock(object);
		vm_page_lock_queues();
		break;
	}
	case VM_PAGER_FAIL:
	case VM_PAGER_ERROR:
		/*
		 * If page couldn't be paged out, then reactivate
		 * the page so it doesn't clog the inactive list.
		 * (We will try paging out it again later).
		 */
		vm_page_activate(m);
		cnt.v_reactivated++;
		break;
	}

	pmap_clear_reference(VM_PAGE_TO_PHYS(m));

	/*
	 * If the operation is still going, leave the page busy
	 * to block all other accesses.  Also, leave the paging
	 * in progress indicator set so that we don't attempt an
	 * object collapse.
	 */
	if (pageout_status != VM_PAGER_PEND) {
		m->flags &= ~PG_BUSY;
		PAGE_WAKEUP(m);
		object->paging_in_progress--;
	}
}

#ifdef CLUSTERED_PAGEOUT
#define PAGEOUTABLE(p) \
	((((p)->flags & (PG_INACTIVE|PG_CLEAN|PG_LAUNDRY)) == \
	  (PG_INACTIVE|PG_LAUNDRY)) && !pmap_is_referenced(VM_PAGE_TO_PHYS(p)))

/*
 * Attempt to pageout as many contiguous (to ``m'') dirty pages as possible
 * from ``object''.  Using information returned from the pager, we assemble
 * a sorted list of contiguous dirty pages and feed them to the pager in one
 * chunk.  Called with paging queues and object locked.  Also, object must
 * already have a pager.
 */
void
vm_pageout_cluster(m, object)
	vm_page_t m;
	vm_object_t object;
{
	vm_offset_t offset, loff, hoff;
	vm_page_t plist[MAXPOCLUSTER], *plistp, p;
	int postatus, ix, count;

	/*
	 * Determine the range of pages that can be part of a cluster
	 * for this object/offset.  If it is only our single page, just
	 * do it normally.
	 */
	vm_pager_cluster(object->pager, m->offset, &loff, &hoff);
	if (hoff - loff == PAGE_SIZE) {
		vm_pageout_page(m, object);
		return;
	}

	plistp = plist;

	/*
	 * Target page is always part of the cluster.
	 */
	pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
	m->flags |= PG_BUSY;
	plistp[atop(m->offset - loff)] = m;
	count = 1;

	/*
	 * Backup from the given page til we find one not fulfilling
	 * the pageout criteria or we hit the lower bound for the
	 * cluster.  For each page determined to be part of the
	 * cluster, unmap it and busy it out so it won't change.
	 */
	ix = atop(m->offset - loff);
	offset = m->offset;
	while (offset > loff && count < MAXPOCLUSTER-1) {
		p = vm_page_lookup(object, offset - PAGE_SIZE);
		if (p == NULL || !PAGEOUTABLE(p))
			break;
		pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
		p->flags |= PG_BUSY;
		plistp[--ix] = p;
		offset -= PAGE_SIZE;
		count++;
	}
	plistp += atop(offset - loff);
	loff = offset;

	/*
	 * Now do the same moving forward from the target.
	 */
	ix = atop(m->offset - loff) + 1;
	offset = m->offset + PAGE_SIZE;
	while (offset < hoff && count < MAXPOCLUSTER) {
		p = vm_page_lookup(object, offset);
		if (p == NULL || !PAGEOUTABLE(p))
			break;
		pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
		p->flags |= PG_BUSY;
		plistp[ix++] = p;
		offset += PAGE_SIZE;
		count++;
	}
	hoff = offset;

	/*
	 * Pageout the page.
	 * Unlock everything and do a wakeup prior to the pager call
	 * in case it blocks.
	 */
	vm_page_unlock_queues();
	object->paging_in_progress++;
	vm_object_unlock(object);
again:
	thread_wakeup(&cnt.v_free_count);
	postatus = vm_pager_put_pages(object->pager, plistp, count, FALSE);
	/*
	 * XXX rethink this
	 */
	if (postatus == VM_PAGER_AGAIN) {
		extern int lbolt;

		(void) tsleep((caddr_t)&lbolt, PZERO|PCATCH, "pageout", 0);
		goto again;
	} else if (postatus == VM_PAGER_BAD)
		panic("vm_pageout_cluster: VM_PAGER_BAD");
	vm_object_lock(object);
	vm_page_lock_queues();

	/*
	 * Loop through the affected pages, reflecting the outcome of
	 * the operation.
	 */
	for (ix = 0; ix < count; ix++) {
		p = *plistp++;
		switch (postatus) {
		case VM_PAGER_OK:
		case VM_PAGER_PEND:
			cnt.v_pgpgout++;
			p->flags &= ~PG_LAUNDRY;
			break;
		case VM_PAGER_FAIL:
		case VM_PAGER_ERROR:
			/*
			 * Pageout failed, reactivate the target page so it
			 * doesn't clog the inactive list.  Other pages are
			 * left as they are.
			 */
			if (p == m) {
				vm_page_activate(p);
				cnt.v_reactivated++;
			}
			break;
		}
		pmap_clear_reference(VM_PAGE_TO_PHYS(p));
		/*
		 * If the operation is still going, leave the page busy
		 * to block all other accesses.
		 */
		if (postatus != VM_PAGER_PEND) {
			p->flags &= ~PG_BUSY;
			PAGE_WAKEUP(p);

		}
	}
	/*
	 * If the operation is still going, leave the paging in progress
	 * indicator set so that we don't attempt an object collapse.
	 */
	if (postatus != VM_PAGER_PEND)
		object->paging_in_progress--;

}
#endif

/*
 *	vm_pageout is the high level pageout daemon.
 */

void
vm_pageout()
{
	(void) spl0();

	/*
	 *	Initialize some paging parameters.
	 */

	if (cnt.v_free_min == 0) {
		cnt.v_free_min = VM_PAGE_FREE_MIN;
		vm_page_free_min_min /= cnt.v_page_size;
		vm_page_free_min_max /= cnt.v_page_size;
		if (cnt.v_free_min < vm_page_free_min_min)
			cnt.v_free_min = vm_page_free_min_min;
		if (cnt.v_free_min > vm_page_free_min_max)
			cnt.v_free_min = vm_page_free_min_max;
	}

	if (cnt.v_free_target == 0)
		cnt.v_free_target = VM_PAGE_FREE_TARGET;

	if (cnt.v_free_target <= cnt.v_free_min)
		cnt.v_free_target = cnt.v_free_min + 1;

	/* XXX does not really belong here */
	if (vm_page_max_wired == 0)
		vm_page_max_wired = cnt.v_free_count / 3;

	/*
	 *	The pageout daemon is never done, so loop
	 *	forever.
	 */

	simple_lock(&vm_pages_needed_lock);
	while (TRUE) {
		thread_sleep(&vm_pages_needed, &vm_pages_needed_lock, FALSE);
		/*
		 * Compute the inactive target for this scan.
		 * We need to keep a reasonable amount of memory in the
		 * inactive list to better simulate LRU behavior.
		 */
		cnt.v_inactive_target =
			(cnt.v_active_count + cnt.v_inactive_count) / 3;
		if (cnt.v_inactive_target <= cnt.v_free_target)
			cnt.v_inactive_target = cnt.v_free_target + 1;

		/*
		 * Only make a scan if we are likely to do something.
		 * Otherwise we might have been awakened by a pager
		 * to clean up async pageouts.
		 */
		if (cnt.v_free_count < cnt.v_free_target ||
		    cnt.v_inactive_count < cnt.v_inactive_target)
			vm_pageout_scan();
		vm_pager_sync();
		simple_lock(&vm_pages_needed_lock);
		thread_wakeup(&cnt.v_free_count);
	}
}
