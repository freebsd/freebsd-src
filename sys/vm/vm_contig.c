/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 */

/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

static int
vm_contig_launder_page(vm_page_t m, vm_page_t *next, int tries)
{
	vm_object_t object;
	vm_page_t m_tmp;
	struct vnode *vp;
	struct mount *mp;
	int vfslocked;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (!vm_pageout_page_lock(m, next) || m->hold_count != 0) {
		vm_page_unlock(m);
		return (EAGAIN);
	}
	object = m->object;
	if (!VM_OBJECT_TRYLOCK(object) &&
	    (!vm_pageout_fallback_object_lock(m, next) || m->hold_count != 0)) {
		vm_page_unlock(m);
		VM_OBJECT_UNLOCK(object);
		return (EAGAIN);
	}
	if ((m->oflags & VPO_BUSY) != 0 || m->busy != 0) {
		if (tries == 0) {
			vm_page_unlock(m);
			VM_OBJECT_UNLOCK(object);
			return (EAGAIN);
		}
		vm_page_sleep(m, "vpctw0");
		VM_OBJECT_UNLOCK(object);
		vm_page_lock_queues();
		return (EBUSY);
	}
	vm_page_test_dirty(m);
	if (m->dirty == 0)
		pmap_remove_all(m);
	if (m->dirty != 0) {
		vm_page_unlock(m);
		if (tries == 0 || (object->flags & OBJ_DEAD) != 0) {
			VM_OBJECT_UNLOCK(object);
			return (EAGAIN);
		}
		if (object->type == OBJT_VNODE) {
			vm_page_unlock_queues();
			vp = object->handle;
			vm_object_reference_locked(object);
			VM_OBJECT_UNLOCK(object);
			(void) vn_start_write(vp, &mp, V_WAIT);
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			VM_OBJECT_LOCK(object);
			vm_object_page_clean(object, 0, 0, OBJPC_SYNC);
			VM_OBJECT_UNLOCK(object);
			VOP_UNLOCK(vp, 0);
			VFS_UNLOCK_GIANT(vfslocked);
			vm_object_deallocate(object);
			vn_finished_write(mp);
			vm_page_lock_queues();
			return (0);
		} else if (object->type == OBJT_SWAP ||
			   object->type == OBJT_DEFAULT) {
			vm_page_unlock_queues();
			m_tmp = m;
			vm_pageout_flush(&m_tmp, 1, VM_PAGER_PUT_SYNC, 0,
			    NULL, NULL);
			VM_OBJECT_UNLOCK(object);
			vm_page_lock_queues();
			return (0);
		}
	} else {
		vm_page_cache(m);
		vm_page_unlock(m);
	}
	VM_OBJECT_UNLOCK(object);
	return (EAGAIN);
}

static int
vm_contig_launder(int queue, int tries, vm_paddr_t low, vm_paddr_t high)
{
	vm_page_t m, next;
	vm_paddr_t pa;
	int error;

	TAILQ_FOREACH_SAFE(m, &vm_page_queues[queue].pl, pageq, next) {
		KASSERT(m->queue == queue,
		    ("vm_contig_launder: page %p's queue is not %d", m, queue));
		if ((m->flags & PG_MARKER) != 0)
			continue;
		pa = VM_PAGE_TO_PHYS(m);
		if (pa < low || pa + PAGE_SIZE > high)
			continue;
		error = vm_contig_launder_page(m, &next, tries);
		if (error == 0)
			return (TRUE);
		if (error == EBUSY)
			return (FALSE);
	}
	return (FALSE);
}

/*
 * Increase the number of cached pages.  The specified value, "tries",
 * determines which categories of pages are cached:
 *
 *  0: All clean, inactive pages within the specified physical address range
 *     are cached.  Will not sleep.
 *  1: The vm_lowmem handlers are called.  All inactive pages within
 *     the specified physical address range are cached.  May sleep.
 *  2: The vm_lowmem handlers are called.  All inactive and active pages
 *     within the specified physical address range are cached.  May sleep.
 */
void
vm_contig_grow_cache(int tries, vm_paddr_t low, vm_paddr_t high)
{
	int actl, actmax, inactl, inactmax;

	if (tries > 0) {
		/*
		 * Decrease registered cache sizes.  The vm_lowmem handlers
		 * may acquire locks and/or sleep, so they can only be invoked
		 * when "tries" is greater than zero.
		 */
		EVENTHANDLER_INVOKE(vm_lowmem, 0);

		/*
		 * We do this explicitly after the caches have been drained
		 * above.
		 */
		uma_reclaim();
	}
	vm_page_lock_queues();
	inactl = 0;
	inactmax = cnt.v_inactive_count;
	actl = 0;
	actmax = tries < 2 ? 0 : cnt.v_active_count;
again:
	if (inactl < inactmax && vm_contig_launder(PQ_INACTIVE, tries, low,
	    high)) {
		inactl++;
		goto again;
	}
	if (actl < actmax && vm_contig_launder(PQ_ACTIVE, tries, low, high)) {
		actl++;
		goto again;
	}
	vm_page_unlock_queues();
}
