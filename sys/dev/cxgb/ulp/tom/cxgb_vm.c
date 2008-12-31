/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_vm.c,v 1.1.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <dev/cxgb/ulp/tom/cxgb_vm.h>

#define TRACE_ENTER printf("%s:%s entered", __FUNCTION__, __FILE__)
#define TRACE_EXIT printf("%s:%s:%d exited", __FUNCTION__, __FILE__, __LINE__)

/*
 * This routine takes a user address range and does the following:
 *  - validate that the user has access to those pages (flags indicates read or write) - if not fail
 *  - validate that count is enough to hold range number of pages - if not fail
 *  - fault in any non-resident pages
 *  - if the user is doing a read force a write fault for any COWed pages
 *  - if the user is doing a read mark all pages as dirty
 *  - hold all pages
 *  - return number of pages in count
 */
int
vm_fault_hold_user_pages(vm_offset_t addr, vm_page_t *mp, int count, int flags)
{
	vm_offset_t end, va;
	int faults, rv;

	struct thread *td;
	vm_map_t map;
	pmap_t pmap;
	vm_page_t m, *pages;
	vm_prot_t prot;
	

	/*
	 * Check that virtual address range is legal
	 * This check is somewhat bogus as on some architectures kernel
	 * and user do not share VA - however, it appears that all FreeBSD
	 * architectures define it
	 */
	end = addr + (count * PAGE_SIZE);
	if (end > VM_MAXUSER_ADDRESS) {
		printf("bad address passed\n");
		return (EFAULT);
	}

	td = curthread;
	map = &td->td_proc->p_vmspace->vm_map;
	pmap = &td->td_proc->p_vmspace->vm_pmap;
	pages = mp;

	prot = VM_PROT_READ;
	prot |= (flags & VM_HOLD_WRITEABLE) ? VM_PROT_WRITE : 0;
retry:

	/*
	 * First optimistically assume that all pages are resident (and R/W if for write)
	 * if so just mark pages as held (and dirty if for write) and return
	 */
	vm_page_lock_queues();
	for (pages = mp, faults = 0, va = addr; va < end; va += PAGE_SIZE, pages++) {
		/*
		 * Assure that we only hold the page once
		 */
		if (*pages == NULL) {
			/*
			 * page queue mutex is recursable so this is OK
			 * it would be really nice if we had an unlocked version of this so
			 * we were only acquiring the pmap lock 1 time as opposed to potentially
			 * many dozens of times
			 */
			*pages = m = pmap_extract_and_hold(pmap, va, prot);
			if (m == NULL) {
				faults++;
				continue;
			}
			
			if (flags & VM_HOLD_WRITEABLE)
				vm_page_dirty(m);
		}
	}
	vm_page_unlock_queues();
	
	if (faults == 0) {
		return (0);
	}
	
	/*
	 * Pages either have insufficient permissions or are not present
	 * trigger a fault where neccessary
	 * 
	 */
	for (pages = mp, va = addr; va < end; va += PAGE_SIZE, pages++) {
		m = *pages;
		rv = 0;
		if (m)
			continue;
		if (flags & VM_HOLD_WRITEABLE) 
			rv = vm_fault(map, va, VM_PROT_WRITE, VM_FAULT_DIRTY);
		else	
			rv = vm_fault(map, va, VM_PROT_READ, VM_FAULT_NORMAL);
		if (rv) {
			printf("vm_fault bad return rv=%d va=0x%zx\n", rv, va);
			
			goto error;
		} 
	}
	
	goto retry;

error:	
	vm_page_lock_queues();
	for (pages = mp, va = addr; va < end; va += PAGE_SIZE, pages++)
		if (*pages)
			vm_page_unhold(*pages);
	vm_page_unlock_queues();
	return (EFAULT);
}

void
vm_fault_unhold_pages(vm_page_t *mp, int count)
{

	KASSERT(count >= 0, ("negative count %d", count));
	vm_page_lock_queues();
	while (count--) {
		vm_page_unhold(*mp);
		mp++;
	}
	vm_page_unlock_queues();
}
