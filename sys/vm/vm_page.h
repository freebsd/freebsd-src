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
 * $FreeBSD$
 */

/*
 *	Resident memory system definitions.
 */

#ifndef	_VM_PAGE_
#define	_VM_PAGE_

#if !defined(KLD_MODULE)
#include "opt_vmpage.h"
#endif

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
 *
 *	The 'valid' and 'dirty' fields are distinct.  A page may have dirty
 *	bits set without having associated valid bits set.  This is used by
 *	NFS to implement piecemeal writes.
 */

TAILQ_HEAD(pglist, vm_page);

struct vm_page {
	TAILQ_ENTRY(vm_page) pageq;	/* queue info for FIFO queue or free list (P) */
	TAILQ_ENTRY(vm_page) listq;	/* pages in same object (O) 	*/
	struct vm_page *left;		/* splay tree link (O)		*/
	struct vm_page *right;		/* splay tree link (O)		*/

	vm_object_t object;		/* which object am I in (O,P)*/
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_offset_t phys_addr;		/* physical address of page */
	struct md_page md;		/* machine dependant stuff */
	u_short	queue;			/* page queue index */
	u_short	flags,			/* see below */
		pc;			/* page color */
	u_short wire_count;		/* wired down maps refs (P) */
	short hold_count;		/* page hold count */
	u_char	act_count;		/* page usage count */
	u_char	busy;			/* page busy count */
	/* NOTE that these must support one bit per DEV_BSIZE in a page!!! */
	/* so, on normal X86 kernels, they must be at least 8 bits wide */
#if PAGE_SIZE == 4096
	u_char	valid;			/* map of valid DEV_BSIZE chunks */
	u_char	dirty;			/* map of dirty DEV_BSIZE chunks */
#elif PAGE_SIZE == 8192
	u_short	valid;			/* map of valid DEV_BSIZE chunks */
	u_short	dirty;			/* map of dirty DEV_BSIZE chunks */
#endif
	u_int cow;			/* page cow mapping count */
};

/*
 * note: currently use SWAPBLK_NONE as an absolute value rather then 
 * a flag bit.
 */

#define SWAPBLK_MASK	((daddr_t)((u_daddr_t)-1 >> 1))		/* mask */
#define SWAPBLK_NONE	((daddr_t)((u_daddr_t)SWAPBLK_MASK + 1))/* flag */

#if !defined(KLD_MODULE)
/*
 * Page coloring parameters
 */
/* Each of PQ_FREE, and PQ_CACHE have PQ_HASH_SIZE entries */

/* Backward compatibility for existing PQ_*CACHE config options. */
#if !defined(PQ_CACHESIZE)
#if defined(PQ_HUGECACHE)
#define PQ_CACHESIZE 1024
#elif defined(PQ_LARGECACHE)
#define PQ_CACHESIZE 512
#elif defined(PQ_MEDIUMCACHE)
#define PQ_CACHESIZE 256
#elif defined(PQ_NORMALCACHE)
#define PQ_CACHESIZE 64
#elif defined(PQ_NOOPT)
#define PQ_CACHESIZE 0
#else
#define PQ_CACHESIZE 128
#endif
#endif			/* !defined(PQ_CACHESIZE) */

#if PQ_CACHESIZE >= 1024
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 256	/* A number of colors opt for 1M cache */

#elif PQ_CACHESIZE >= 512
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 128	/* A number of colors opt for 512K cache */

#elif PQ_CACHESIZE >= 256
#define PQ_PRIME1 13	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 7	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 64	/* A number of colors opt for 256K cache */

#elif PQ_CACHESIZE >= 128
#define PQ_PRIME1 9	/* Produces a good PQ_L2_SIZE/3 + PQ_PRIME1 */
#define PQ_PRIME2 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 32	/* A number of colors opt for 128k cache */

#elif PQ_CACHESIZE >= 64
#define PQ_PRIME1 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 3	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 16	/* A reasonable number of colors (opt for 64K cache) */

#else
#define PQ_PRIME1 1	/* Disable page coloring. */
#define PQ_PRIME2 1
#define PQ_L2_SIZE 1

#endif

#define PQ_L2_MASK (PQ_L2_SIZE - 1)

#define PQ_NONE 0
#define PQ_FREE	1
#define PQ_INACTIVE (1 + 1*PQ_L2_SIZE)
#define PQ_ACTIVE (2 + 1*PQ_L2_SIZE)
#define PQ_CACHE (3 + 1*PQ_L2_SIZE)
#define PQ_HOLD  (3 + 2*PQ_L2_SIZE)
#define PQ_COUNT (4 + 2*PQ_L2_SIZE)

struct vpgqueues {
	struct pglist pl;
	int	*cnt;
	int	lcnt;
};

extern struct vpgqueues vm_page_queues[PQ_COUNT];
extern struct mtx vm_page_queue_free_mtx;

#endif			/* !defined(KLD_MODULE) */

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_FILLED and PG_DIRTY are added for the filesystems.
 *
 * Note: PG_UNMANAGED (used by OBJT_PHYS) indicates that the page is
 * 	 not under PV management but otherwise should be treated as a
 *	 normal page.  Pages not under PV management cannot be paged out
 *	 via the object/vm_page_t because there is no knowledge of their
 *	 pte mappings, nor can they be removed from their objects via 
 *	 the object, and such pages are also not on any PQ queue.
 */
#define	PG_BUSY		0x0001		/* page is in transit (O) */
#define	PG_WANTED	0x0002		/* someone is waiting for page (O) */
#define PG_WINATCFLS	0x0004		/* flush dirty page on inactive q */
#define	PG_FICTITIOUS	0x0008		/* physical page doesn't exist (O) */
#define	PG_WRITEABLE	0x0010		/* page is mapped writeable */
#define	PG_ZERO		0x0040		/* page is zeroed */
#define PG_REFERENCED	0x0080		/* page has been referenced */
#define PG_CLEANCHK	0x0100		/* page will be checked for cleaning */
#define PG_SWAPINPROG	0x0200		/* swap I/O in progress on page	     */
#define PG_NOSYNC	0x0400		/* do not collect for syncer */
#define PG_UNMANAGED	0x0800		/* No PV management for page */
#define PG_MARKER	0x1000		/* special queue marker page */
#define	PG_SLAB		0x2000		/* object pointer is actually a slab */

/*
 * Misc constants.
 */
#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_INIT		5
#define ACT_MAX			64
#define PFCLUSTER_BEHIND	3
#define PFCLUSTER_AHEAD		3

#ifdef _KERNEL
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
 *		Low activity, candidates for reclamation.
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

extern int vm_page_zero_count;

extern vm_page_t vm_page_array;		/* First resident page in table */
extern int vm_page_array_size;		/* number of vm_page_t's */
extern long first_page;			/* first physical page number */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#define PHYS_TO_VM_PAGE(pa) \
		(&vm_page_array[atop(pa) - first_page ])

extern struct mtx vm_page_queue_mtx;
#define vm_page_lock_queues()   mtx_lock(&vm_page_queue_mtx)
#define vm_page_unlock_queues() mtx_unlock(&vm_page_queue_mtx)

#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xff
#endif

#if PAGE_SIZE == 8192
#define VM_PAGE_BITS_ALL 0xffff
#endif

/* page allocation classes: */
#define VM_ALLOC_NORMAL		0
#define VM_ALLOC_INTERRUPT	1
#define VM_ALLOC_SYSTEM		2
#define	VM_ALLOC_CLASS_MASK	3
/* page allocation flags: */
#define	VM_ALLOC_WIRED		0x20
#define	VM_ALLOC_ZERO		0x40
#define	VM_ALLOC_RETRY		0x80	/* vm_page_grab() only */

void vm_page_flag_set(vm_page_t m, unsigned short bits);
void vm_page_flag_clear(vm_page_t m, unsigned short bits);
void vm_page_busy(vm_page_t m);
void vm_page_flash(vm_page_t m);
void vm_page_io_start(vm_page_t m);
void vm_page_io_finish(vm_page_t m);
void vm_page_hold(vm_page_t mem);
void vm_page_unhold(vm_page_t mem);
void vm_page_protect(vm_page_t mem, int prot);
void vm_page_copy(vm_page_t src_m, vm_page_t dest_m);
void vm_page_free(vm_page_t m);
void vm_page_free_zero(vm_page_t m);
int vm_page_sleep_busy(vm_page_t m, int also_m_busy, const char *msg);
int vm_page_sleep_if_busy(vm_page_t m, int also_m_busy, const char *msg);
void vm_page_dirty(vm_page_t m);
void vm_page_undirty(vm_page_t m);
void vm_page_wakeup(vm_page_t m);

void vm_pageq_init(void);
vm_page_t vm_pageq_add_new_page(vm_offset_t pa);
void vm_pageq_enqueue(int queue, vm_page_t m);
void vm_pageq_remove_nowakeup(vm_page_t m);
void vm_pageq_remove(vm_page_t m);
vm_page_t vm_pageq_find(int basequeue, int index, boolean_t prefer_zero);
void vm_pageq_requeue(vm_page_t m);

void vm_page_activate (vm_page_t);
vm_page_t vm_page_alloc (vm_object_t, vm_pindex_t, int);
vm_page_t vm_page_grab (vm_object_t, vm_pindex_t, int);
void vm_page_cache (register vm_page_t);
int vm_page_try_to_cache (vm_page_t);
int vm_page_try_to_free (vm_page_t);
void vm_page_dontneed (register vm_page_t);
void vm_page_deactivate (vm_page_t);
void vm_page_insert (vm_page_t, vm_object_t, vm_pindex_t);
vm_page_t vm_page_lookup (vm_object_t, vm_pindex_t);
void vm_page_remove (vm_page_t);
void vm_page_rename (vm_page_t, vm_object_t, vm_pindex_t);
vm_offset_t vm_page_startup (vm_offset_t, vm_offset_t, vm_offset_t);
void vm_page_unmanage (vm_page_t);
void vm_page_unwire (vm_page_t, int);
void vm_page_wire (vm_page_t);
void vm_page_set_validclean (vm_page_t, int, int);
void vm_page_set_dirty (vm_page_t, int, int);
void vm_page_clear_dirty (vm_page_t, int, int);
void vm_page_set_invalid (vm_page_t, int, int);
int vm_page_is_valid (vm_page_t, int, int);
void vm_page_test_dirty (vm_page_t);
int vm_page_bits (int, int);
void vm_page_zero_invalid(vm_page_t m, boolean_t setvalid);
void vm_page_free_toq(vm_page_t m);
void vm_page_zero_idle_wakeup(void);
void vm_page_cowfault (vm_page_t);
void vm_page_cowsetup (vm_page_t);
void vm_page_cowclear (vm_page_t);

#endif				/* _KERNEL */
#endif				/* !_VM_PAGE_ */
