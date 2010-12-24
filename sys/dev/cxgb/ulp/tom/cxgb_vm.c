/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
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
__FBSDID("$FreeBSD$");

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
#include <sys/syslog.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <ulp/tom/cxgb_vm.h>

/*
 * This routine takes a user's map, array of pages, number of pages, and flags
 * and then does the following:
 *  - validate that the user has access to those pages (flags indicates read
 *	or write) - if not fail
 *  - validate that count is enough to hold range number of pages - if not fail
 *  - fault in any non-resident pages
 *  - if the user is doing a read force a write fault for any COWed pages
 *  - if the user is doing a read mark all pages as dirty
 *  - hold all pages
 */
int
vm_fault_hold_user_pages(vm_map_t map, vm_offset_t addr, vm_page_t *mp,
    int count, vm_prot_t prot)
{
	vm_offset_t end, va;
	int faults, rv;
	pmap_t pmap;
	vm_page_t m, *pages;
	
	pmap = vm_map_pmap(map);
	pages = mp;
	addr &= ~PAGE_MASK;
	/*
	 * Check that virtual address range is legal
	 * This check is somewhat bogus as on some architectures kernel
	 * and user do not share VA - however, it appears that all FreeBSD
	 * architectures define it
	 */
	end = addr + (count * PAGE_SIZE);
	if (end > VM_MAXUSER_ADDRESS) {
		log(LOG_WARNING, "bad address passed to vm_fault_hold_user_pages");
		return (EFAULT);
	}

	/*
	 * First optimistically assume that all pages are resident 
	 * (and R/W if for write) if so just mark pages as held (and 
	 * dirty if for write) and return
	 */
	for (pages = mp, faults = 0, va = addr; va < end;
	     va += PAGE_SIZE, pages++) {
		/*
		 * it would be really nice if we had an unlocked
		 * version of this so we were only acquiring the 
		 * pmap lock 1 time as opposed to potentially
		 * many dozens of times
		 */
		*pages = m = pmap_extract_and_hold(pmap, va, prot);
		if (m == NULL) {
			faults++;
			continue;
		}
		/*
		 * Preemptively mark dirty - the pages
		 * will never have the modified bit set if
		 * they are only changed via DMA
		 */
		if (prot & VM_PROT_WRITE) {
			vm_page_lock_queues();
			vm_page_dirty(m);
			vm_page_unlock_queues();
		}
		
	}
	
	if (faults == 0)
		return (0);
	
	/*
	 * Pages either have insufficient permissions or are not present
	 * trigger a fault where neccessary
	 */
	for (pages = mp, va = addr; va < end; va += PAGE_SIZE, pages++) {
		if (*pages == NULL && (rv = vm_fault_hold(map, va, prot,
		    VM_FAULT_NORMAL, pages)) != KERN_SUCCESS)
			goto error;
	}
	return (0);
error:	
	log(LOG_WARNING,
	    "vm_fault bad return rv=%d va=0x%zx\n", rv, va);
	for (pages = mp, va = addr; va < end; va += PAGE_SIZE, pages++)
		if (*pages) {
			vm_page_lock(*pages);
			vm_page_unhold(*pages);
			vm_page_unlock(*pages);
			*pages = NULL;
		}
	return (EFAULT);
}
