/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)vm_pager.h	8.4 (Berkeley) 1/12/94
 * $FreeBSD$
 */

/*
 * Pager routine interface definition.
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

#include <sys/queue.h>

TAILQ_HEAD(pagerlst, vm_object);

struct buf;

struct pagerops {
	void (*pgo_init) __P((void));		/* Initialize pager. */
	vm_object_t (*pgo_alloc) __P((void *, vm_ooffset_t, vm_prot_t, vm_ooffset_t));	/* Allocate pager. */
	void (*pgo_dealloc) __P((vm_object_t));	/* Disassociate. */
	int (*pgo_getpages) __P((vm_object_t, vm_page_t *, int, int));	/* Get (read) page. */
	void (*pgo_putpages) __P((vm_object_t, vm_page_t *, int, int, int *)); /* Put (write) page. */
	boolean_t (*pgo_haspage) __P((vm_object_t, vm_pindex_t, int *, int *)); /* Does pager have page? */
	void (*pgo_pageunswapped) __P((vm_page_t));
	void (*pgo_strategy) __P((vm_object_t, struct buf *));
};

/*
 * get/put return values
 * OK	 operation was successful
 * BAD	 specified data was out of the accepted range
 * FAIL	 specified data was in range, but doesn't exist
 * PEND	 operations was initiated but not completed
 * ERROR error while accessing data that is in range and exists
 * AGAIN temporary resource shortage prevented operation from happening
 */
#define	VM_PAGER_OK	0
#define	VM_PAGER_BAD	1
#define	VM_PAGER_FAIL	2
#define	VM_PAGER_PEND	3
#define	VM_PAGER_ERROR	4
#define VM_PAGER_AGAIN	5

#define	VM_PAGER_PUT_SYNC	0x1
#define	VM_PAGER_PUT_INVAL	0x2

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_VMPGDATA);
#endif

extern vm_map_t pager_map;
extern int pager_map_size;
extern struct pagerops *pagertab[];

vm_object_t vm_pager_allocate __P((objtype_t, void *, vm_ooffset_t, vm_prot_t, vm_ooffset_t));
void vm_pager_bufferinit __P((void));
void vm_pager_deallocate __P((vm_object_t));
static __inline int vm_pager_get_pages __P((vm_object_t, vm_page_t *, int, int));
static __inline boolean_t vm_pager_has_page __P((vm_object_t, vm_pindex_t, int *, int *));
void vm_pager_init __P((void));
vm_object_t vm_pager_object_lookup __P((struct pagerlst *, void *));
vm_offset_t vm_pager_map_pages __P((vm_page_t *, int, boolean_t));
vm_offset_t vm_pager_map_page __P((vm_page_t));
void vm_pager_sync __P((void));
void vm_pager_unmap_pages __P((vm_offset_t, int));
void vm_pager_unmap_page __P((vm_offset_t));
void vm_pager_strategy __P((vm_object_t object, struct buf *bp));
struct buf *getchainbuf(struct buf *bp, struct vnode *vp, int flags);
void flushchainbuf(struct buf *nbp);
void waitchainbuf(struct buf *bp, int count, int done);
void autochaindone(struct buf *bp);

/*
 *	vm_page_get_pages:
 *
 *	Retrieve pages from the VM system in order to map them into an object
 *	( or into VM space somewhere ).  If the pagein was successful, we
 *	must fully validate it.
 */

static __inline int
vm_pager_get_pages(
	vm_object_t object,
	vm_page_t *m,
	int count,
	int reqpage
) {
	int r;

	r = (*pagertab[object->type]->pgo_getpages)(object, m, count, reqpage);
	if (r == VM_PAGER_OK && m[reqpage]->valid != VM_PAGE_BITS_ALL) {
		vm_page_zero_invalid(m[reqpage], TRUE);
	}
	return(r);
}

static __inline void
vm_pager_put_pages(
	vm_object_t object,
	vm_page_t *m,
	int count,
	int flags,
	int *rtvals
) {
	(*pagertab[object->type]->pgo_putpages)
	    (object, m, count, flags, rtvals);
}

/*
 *	vm_pager_haspage
 *
 *	Check to see if an object's pager has the requested page.  The
 *	object's pager will also set before and after to give the caller
 *	some idea of the number of pages before and after the requested
 *	page can be I/O'd efficiently.
 *
 *	This routine does not have to be called at any particular spl.
 */

static __inline boolean_t
vm_pager_has_page(
	vm_object_t object,
	vm_pindex_t offset, 
	int *before,
	int *after
) {
        return ((*pagertab[object->type]->pgo_haspage) (object, offset, before, after));
} 

/* 
 *      vm_pager_page_unswapped
 * 
 *      called at splvm() to destroy swap associated with the page.
 * 
 *      This function may not block.
 */
 
static __inline void
vm_pager_page_unswapped(vm_page_t m)
{
	if (pagertab[m->object->type]->pgo_pageunswapped)
		(*pagertab[m->object->type]->pgo_pageunswapped)(m);
}

#endif

#endif				/* _VM_PAGER_ */
