/*--
 * Copyright (c) 1997, Duke University
 * All rights reserved.
 *
 * Author:
 *         Andrew Gallatin <gallatin@cs.duke.edu>  
 *            
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Duke University may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY DUKE UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DUKE UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITSOR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 */

/*
 * This is a set of routines for enabling and disabling copy on write
 * protection for data written into sockets.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/sf_buf.h>
#include <sys/socketvar.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>


struct netsend_cow_stats {
	int attempted;
	int fail_not_mapped;
	int fail_sf_buf;
	int success;
	int iodone;
};

static struct netsend_cow_stats socow_stats;

static void socow_iodone(void *addr, void *args);

static void
socow_iodone(void *addr, void *args)
{	
	struct sf_buf *sf;
	vm_page_t pp;

	sf = args;
	pp = sf_buf_page(sf);
	sf_buf_free(sf);
	/* remove COW mapping  */
	vm_page_lock(pp);
	vm_page_cowclear(pp);
	vm_page_unwire(pp, 0);
	/*
	 * Check for the object going away on us. This can
	 * happen since we don't hold a reference to it.
	 * If so, we're responsible for freeing the page.
	 */
	if (pp->wire_count == 0 && pp->object == NULL)
		vm_page_free(pp);
	vm_page_unlock(pp);
	socow_stats.iodone++;
}

int
socow_setup(struct mbuf *m0, struct uio *uio)
{
	struct sf_buf *sf;
	vm_page_t pp;
	struct iovec *iov;
	struct vmspace *vmspace;
	struct vm_map *map;
	vm_offset_t offset, uva;
	vm_size_t len;

	socow_stats.attempted++;
	vmspace = curproc->p_vmspace;
	map = &vmspace->vm_map;
	uva = (vm_offset_t) uio->uio_iov->iov_base;
	offset = uva & PAGE_MASK;
	len = PAGE_SIZE - offset;

	/*
	 * Verify that access to the given address is allowed from user-space.
	 */
	if (vm_fault_quick_hold_pages(map, uva, len, VM_PROT_READ, &pp, 1) <
	    0) {
		socow_stats.fail_not_mapped++;
		return(0);
	}

	/* 
	 * set up COW
	 */
	vm_page_lock(pp);
	if (vm_page_cowsetup(pp) != 0) {
		vm_page_unhold(pp);
		vm_page_unlock(pp);
		return (0);
	}

	/*
	 * wire the page for I/O
	 */
	vm_page_wire(pp);
	vm_page_unhold(pp);
	vm_page_unlock(pp);
	/*
	 * Allocate an sf buf
	 */
	sf = sf_buf_alloc(pp, SFB_CATCH);
	if (sf == NULL) {
		vm_page_lock(pp);
		vm_page_cowclear(pp);
		vm_page_unwire(pp, 0);
		/*
		 * Check for the object going away on us. This can
		 * happen since we don't hold a reference to it.
		 * If so, we're responsible for freeing the page.
		 */
		if (pp->wire_count == 0 && pp->object == NULL)
			vm_page_free(pp);
		vm_page_unlock(pp);
		socow_stats.fail_sf_buf++;
		return(0);
	}
	/* 
	 * attach to mbuf
	 */
	MEXTADD(m0, sf_buf_kva(sf), PAGE_SIZE, socow_iodone,
	    (void*)sf_buf_kva(sf), sf, M_RDONLY, EXT_SFBUF);
	m0->m_len = len;
	m0->m_data = (caddr_t)sf_buf_kva(sf) + offset;
	socow_stats.success++;

	iov = uio->uio_iov;
	iov->iov_base = (char *)iov->iov_base + m0->m_len;
	iov->iov_len -= m0->m_len;
	uio->uio_resid -= m0->m_len;
	uio->uio_offset += m0->m_len;
	if (iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	return(m0->m_len);
}
