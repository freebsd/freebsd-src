/*-
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/vm/vm_pageq.c,v 1.35 2007/09/25 06:25:06 alc Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_extern.h>

struct vpgqueues vm_page_queues[PQ_MAXCOUNT];

void
vm_pageq_init(void)
{
	int i;

	vm_page_queues[PQ_INACTIVE].cnt = &cnt.v_inactive_count;
	vm_page_queues[PQ_ACTIVE].cnt = &cnt.v_active_count;
	vm_page_queues[PQ_HOLD].cnt = &cnt.v_active_count;

	for (i = 0; i < PQ_COUNT; i++) {
		TAILQ_INIT(&vm_page_queues[i].pl);
	}
}

void
vm_pageq_requeue(vm_page_t m)
{
	int queue = VM_PAGE_GETQUEUE(m);
	struct vpgqueues *vpq;

	if (queue != PQ_NONE) {
		vpq = &vm_page_queues[queue];
		TAILQ_REMOVE(&vpq->pl, m, pageq);
		TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
	}
}

/*
 *	vm_pageq_enqueue:
 */
void
vm_pageq_enqueue(int queue, vm_page_t m)
{
	struct vpgqueues *vpq;

	vpq = &vm_page_queues[queue];
	VM_PAGE_SETQUEUE2(m, queue);
	TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
	++*vpq->cnt;
}

/*
 * vm_pageq_remove:
 *
 *	Remove a page from its queue.
 *
 *	The queue containing the given page must be locked.
 *	This routine may not block.
 */
void
vm_pageq_remove(vm_page_t m)
{
	int queue = VM_PAGE_GETQUEUE(m);
	struct vpgqueues *pq;

	if (queue != PQ_NONE) {
		VM_PAGE_SETQUEUE2(m, PQ_NONE);
		pq = &vm_page_queues[queue];
		TAILQ_REMOVE(&pq->pl, m, pageq);
		(*pq->cnt)--;
	}
}
