
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
 * $Id: vm_pager.h,v 1.4 1994/10/09 01:52:17 phk Exp $
 */

/*
 * Pager routine interface definition.
 * For BSD we use a cleaner version of the internal pager interface.
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

TAILQ_HEAD(pagerlst, pager_struct);

struct pager_struct {
	TAILQ_ENTRY(pager_struct) pg_list;	/* links for list management */
	caddr_t pg_handle;		/* ext. handle (vp, dev, fp) */
	int pg_type;			/* type of pager */
	struct pagerops *pg_ops;	/* pager operations */
	void *pg_data;			/* private pager data */
};

/* pager types */
#define PG_DFLT		-1
#define	PG_SWAP		0
#define	PG_VNODE	1
#define PG_DEVICE	2

/* flags */
#define PG_CLUSTERGET	1
#define PG_CLUSTERPUT	2

struct pagerops {
	void (*pgo_init) __P((void));		/* Initialize pager. */
	vm_pager_t(*pgo_alloc) __P((caddr_t, vm_size_t, vm_prot_t, vm_offset_t));	/* Allocate pager. */
	void (*pgo_dealloc) __P((vm_pager_t));	/* Disassociate. */
	int (*pgo_getpage) __P((vm_pager_t, vm_page_t, boolean_t));
	int (*pgo_getpages) __P((vm_pager_t, vm_page_t *, int, int, boolean_t));	/* Get (read) page. */
	int (*pgo_putpage) __P((vm_pager_t, vm_page_t, boolean_t));
	int (*pgo_putpages) __P((vm_pager_t, vm_page_t *, int, boolean_t, int *)); /* Put (write) page. */
	boolean_t(*pgo_haspage) __P((vm_pager_t, vm_offset_t)); /* Does pager have page? */
};

#define	VM_PAGER_ALLOC(h, s, p, o)		(*(pg)->pg_ops->pgo_alloc)(h, s, p, o)
#define	VM_PAGER_DEALLOC(pg)		(*(pg)->pg_ops->pgo_dealloc)(pg)
#define	VM_PAGER_GET(pg, m, s)		(*(pg)->pg_ops->pgo_getpage)(pg, m, s)
#define	VM_PAGER_GET_MULTI(pg, m, c, r, s)	(*(pg)->pg_ops->pgo_getpages)(pg, m, c, r, s)
#define	VM_PAGER_PUT(pg, m, s)		(*(pg)->pg_ops->pgo_putpage)(pg, m, s)
#define	VM_PAGER_PUT_MULTI(pg, m, c, s, rtval)		(*(pg)->pg_ops->pgo_putpages)(pg, m, c, s, rtval)
#define	VM_PAGER_HASPAGE(pg, o)		(*(pg)->pg_ops->pgo_haspage)(pg, o)

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

#ifdef KERNEL
extern struct pagerops *dfltpagerops;

vm_pager_t vm_pager_allocate __P((int, caddr_t, vm_size_t, vm_prot_t, vm_offset_t));
vm_page_t vm_pager_atop __P((vm_offset_t));
void vm_pager_deallocate __P((vm_pager_t));
int vm_pager_get_pages __P((vm_pager_t, vm_page_t *, int, int, boolean_t));
boolean_t vm_pager_has_page __P((vm_pager_t, vm_offset_t));
void vm_pager_init __P((void));
vm_pager_t vm_pager_lookup __P((struct pagerlst *, caddr_t));
vm_offset_t vm_pager_map_pages __P((vm_page_t *, int, boolean_t));
vm_offset_t vm_pager_map_page __P((vm_page_t));
int vm_pager_put_pages __P((vm_pager_t, vm_page_t *, int, boolean_t, int *));
void vm_pager_sync __P((void));
void vm_pager_unmap_pages __P((vm_offset_t, int));
void vm_pager_unmap_page __P((vm_offset_t));

/*
 * XXX compat with old interface
 */
#define vm_pager_get(p, m, s) \
({ \
	vm_page_t ml[1]; \
	ml[0] = (m); \
	vm_pager_get_pages(p, ml, 1, 0, s); \
})

#define vm_pager_put(p, m, s) \
({ \
	int rtval; \
	vm_page_t ml[1]; \
	ml[0] = (m); \
	vm_pager_put_pages(p, ml, 1, s, &rtval); \
	rtval; \
})
#endif

#endif				/* _VM_PAGER_ */
