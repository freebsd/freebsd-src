/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vm_pager.h	7.2 (Berkeley) 4/20/91
 *	$Id: vm_pager.h,v 1.6 1994/01/31 04:21:50 davidg Exp $
 */

/*
 * Pager routine interface definition.
 * For BSD we use a cleaner version of the internal pager interface.
 */

#ifndef	_VM_PAGER_
#define	_VM_PAGER_

struct	pager_struct {
	queue_head_t	pg_list;	/* links for list management */
	caddr_t		pg_handle;	/* external handle (vp, dev, fp) */
	int		pg_type;	/* type of pager */
	struct pagerops	*pg_ops;	/* pager operations */
	caddr_t		pg_data;	/* private pager data */
};
typedef	struct pager_struct *vm_pager_t;

/* pager types */
#define PG_DFLT		-1
#define	PG_SWAP		0
#define	PG_VNODE	1
#define PG_DEVICE	2

struct	pagerops {
	void		(*pgo_init)();		/* initialize pager */
	vm_pager_t	(*pgo_alloc)(caddr_t, vm_size_t, vm_prot_t, vm_offset_t);	/* allocate pager */
	void		(*pgo_dealloc)();	/* disassociate */
	int		(*pgo_getpage)();	/* get (read) page */
	int		(*pgo_getmulti)();	/* get (read) multiple pages */
	int		(*pgo_putpage)();	/* put (write) page */
	boolean_t  	(*pgo_haspage)();	/* does pager have page? */
};

/*
 * get/put return values
 * OK	operation was successful
 * BAD	specified data was out of the accepted range
 * FAIL	specified data was in range, but doesn't exist
 * PEND	operations was initiated but not completed
 * TRYAGAIN operation will be successful in the future
 */
#define	VM_PAGER_OK	0
#define	VM_PAGER_BAD	1
#define	VM_PAGER_FAIL	2
#define	VM_PAGER_PEND	3
#define	VM_PAGER_TRYAGAIN 4

#define	VM_PAGER_ALLOC(h, s, p, o)		(*(pg)->pg_ops->pgo_alloc)(h, s, p, o)
#define	VM_PAGER_DEALLOC(pg)		(*(pg)->pg_ops->pgo_dealloc)(pg)
#define	VM_PAGER_GET(pg, m, s)		(*(pg)->pg_ops->pgo_getpage)(pg, m, s)
#define	VM_PAGER_GET_MULTI(pg, m, c, r, s)	(*(pg)->pg_ops->pgo_getmulti)(pg, m, c, r, s)
#define	VM_PAGER_PUT(pg, m, s)		(*(pg)->pg_ops->pgo_putpage)(pg, m, s)
#define	VM_PAGER_HASPAGE(pg, o)		(*(pg)->pg_ops->pgo_haspage)(pg, o)

#ifdef KERNEL
extern void vm_pager_init(void);
extern vm_pager_t vm_pager_allocate(int, caddr_t, vm_size_t, vm_prot_t, vm_offset_t);
extern void vm_pager_deallocate(vm_pager_t);
struct vm_page;
extern int vm_pager_get(vm_pager_t, struct vm_page *, boolean_t);
extern int vm_pager_put(vm_pager_t, struct vm_page *, boolean_t);
extern boolean_t vm_pager_has_page(vm_pager_t, vm_offset_t);
extern void vm_pager_sync(void);
extern vm_offset_t vm_pager_map_page(struct vm_page *);
extern void vm_pager_unmap_page(vm_offset_t);
extern vm_pager_t vm_pager_lookup(queue_head_t *, caddr_t);

extern struct pagerops *dfltpagerops;
#endif

#endif	/* _VM_PAGER_ */
