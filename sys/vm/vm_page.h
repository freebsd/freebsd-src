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
 *	from: @(#)vm_page.h	8.2 (Berkeley) 12/13/93
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
 * $Id: vm_page.h,v 1.22 1995/11/20 12:19:32 phk Exp $
 */

/*
 *	Resident memory system definitions.
 */

#ifndef	_VM_PAGE_
#define	_VM_PAGE_

#include <vm/pmap.h>
/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P).
 */

TAILQ_HEAD(pglist, vm_page);

struct vm_page {
	TAILQ_ENTRY(vm_page) pageq;	/* queue info for FIFO queue or free list (P) */
	TAILQ_ENTRY(vm_page) hashq;	/* hash table links (O) */
	TAILQ_ENTRY(vm_page) listq;	/* pages in same object (O) */

	vm_object_t object;		/* which object am I in (O,P) */
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_offset_t phys_addr;		/* physical address of page */

	u_short wire_count;		/* wired down maps refs (P) */
	u_short flags;			/* see below */
	short hold_count;		/* page hold count */
	u_short act_count;		/* page usage count */
	u_short bmapped;		/* number of buffers mapped */
	u_short busy;			/* page busy count */
	u_short valid;			/* map of valid DEV_BSIZE chunks */
	u_short dirty;			/* map of dirty DEV_BSIZE chunks */
};

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_FILLED and PG_DIRTY are added for the filesystems.
 */
#define	PG_INACTIVE	0x0001		/* page is in inactive list (P) */
#define	PG_ACTIVE	0x0002		/* page is in active list (P) */
#define	PG_BUSY		0x0010		/* page is in transit (O) */
#define	PG_WANTED	0x0020		/* someone is waiting for page (O) */
#define	PG_TABLED	0x0040		/* page is in VP table (O) */
#define	PG_FICTITIOUS	0x0100		/* physical page doesn't exist (O) */
#define	PG_WRITEABLE	0x0200		/* page is mapped writeable */
#define PG_MAPPED	0x0400		/* page is mapped */
#define	PG_ZERO		0x0800		/* page is zeroed */
#define PG_REFERENCED	0x1000		/* page has been referenced */
#define	PG_CACHE	0x4000		/* On VMIO cache */
#define	PG_FREE		0x8000		/* page is in free list */

/*
 * Misc constants.
 */

#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_MAX			100
#define PFCLUSTER_BEHIND	3
#define PFCLUSTER_AHEAD		3

#ifdef KERNEL
/*
 * Each pageable resident page falls into one of four lists:
 *
 *	free
 *		Available for allocation now.
 *
 * The following are all LRU sorted:
 *
 *	cache
 *		Almost available for allocation. Still in an
 *		object, but clean and immediately freeable at
 *		non-interrupt times.
 *
 *	inactive
 *		Low activity, candidates for reclaimation.
 *		This is the list of pages that should be
 *		paged out next.
 *
 *	active
 *		Pages that are "active" i.e. they have been
 *		recently referenced.
 *
 *	zero
 *		Pages that are really free and have been pre-zeroed
 *
 */

extern struct pglist vm_page_queue_free;	/* memory free queue */
extern struct pglist vm_page_queue_zero;	/* zeroed memory free queue */
extern struct pglist vm_page_queue_active;	/* active memory queue */
extern struct pglist vm_page_queue_inactive;	/* inactive memory queue */
extern struct pglist vm_page_queue_cache;	/* cache memory queue */

extern int vm_page_zero_count;

extern vm_page_t vm_page_array;		/* First resident page in table */
extern long first_page;			/* first physical page number */

 /* ... represented in vm_page_array */
extern long last_page;			/* last physical page number */

 /* ... represented in vm_page_array */
 /* [INCLUSIVE] */
extern vm_offset_t first_phys_addr;	/* physical address for first_page */
extern vm_offset_t last_phys_addr;	/* physical address for last_page */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#define IS_VM_PHYSADDR(pa) \
		((pa) >= first_phys_addr && (pa) <= last_phys_addr)

#define PHYS_TO_VM_PAGE(pa) \
		(&vm_page_array[atop(pa) - first_page ])

/*
 *	Functions implemented as macros
 */

#define PAGE_ASSERT_WAIT(m, interruptible)	{ \
				(m)->flags |= PG_WANTED; \
				assert_wait((int) (m), (interruptible)); \
			}

#define PAGE_WAKEUP(m)	{ \
				(m)->flags &= ~PG_BUSY; \
				if ((m)->flags & PG_WANTED) { \
					(m)->flags &= ~PG_WANTED; \
					wakeup((caddr_t) (m)); \
				} \
			}

#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xff
#endif

#if PAGE_SIZE == 8192
#define VM_PAGE_BITS_ALL 0xffff
#endif

#define VM_ALLOC_NORMAL 0
#define VM_ALLOC_INTERRUPT 1
#define VM_ALLOC_SYSTEM 2
#define	VM_ALLOC_ZERO	0x80

void vm_page_activate __P((vm_page_t));
vm_page_t vm_page_alloc __P((vm_object_t, vm_pindex_t, int));
void vm_page_cache __P((register vm_page_t));
void vm_page_copy __P((vm_page_t, vm_page_t));
void vm_page_deactivate __P((vm_page_t));
void vm_page_free __P((vm_page_t));
void vm_page_insert __P((vm_page_t, vm_object_t, vm_pindex_t));
vm_page_t vm_page_lookup __P((vm_object_t, vm_pindex_t));
void vm_page_remove __P((vm_page_t));
void vm_page_rename __P((vm_page_t, vm_object_t, vm_pindex_t));
vm_offset_t vm_page_startup __P((vm_offset_t, vm_offset_t, vm_offset_t));
void vm_page_unwire __P((vm_page_t));
void vm_page_wire __P((vm_page_t));
boolean_t vm_page_zero_fill __P((vm_page_t));
void vm_page_set_validclean __P((vm_page_t, int, int));
void vm_page_set_invalid __P((vm_page_t, int, int));
int vm_page_is_valid __P((vm_page_t, int, int));
void vm_page_test_dirty __P((vm_page_t));
int vm_page_bits __P((int, int));


/*
 * Keep page from being freed by the page daemon
 * much of the same effect as wiring, except much lower
 * overhead and should be used only for *very* temporary
 * holding ("wiring").
 */
static __inline void
vm_page_hold(vm_page_t mem)
{
	mem->hold_count++;
}

#ifdef DIAGNOSTIC
#include <sys/systm.h>		/* make GCC shut up */
#endif

static __inline void
vm_page_unhold(vm_page_t mem)
{
#ifdef DIAGNOSTIC
	if (--mem->hold_count < 0)
		panic("vm_page_unhold: hold count < 0!!!");
#else
	--mem->hold_count;
#endif
}

static __inline void
vm_page_protect(vm_page_t mem, int prot)
{
	if (prot == VM_PROT_NONE) {
		if (mem->flags & (PG_WRITEABLE|PG_MAPPED)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(mem), prot);
			mem->flags &= ~(PG_WRITEABLE|PG_MAPPED);
		}
	} else if ((prot == VM_PROT_READ) && (mem->flags & PG_WRITEABLE)) {
		pmap_page_protect(VM_PAGE_TO_PHYS(mem), prot);
		mem->flags &= ~PG_WRITEABLE;
	}
}


#endif				/* KERNEL */
#endif				/* !_VM_PAGE_ */
