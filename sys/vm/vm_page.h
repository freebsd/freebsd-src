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
 * $Id: vm_page.h,v 1.56 1999/02/15 06:52:14 dillon Exp $
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
#include <machine/atomic.h>

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
	struct vm_page	*hnext;		/* hash table link (O,P)	*/
	TAILQ_ENTRY(vm_page) listq;	/* pages in same object (O) 	*/

	vm_object_t object;		/* which object am I in (O,P)*/
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_offset_t phys_addr;		/* physical address of page */
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
};

/*
 * note SWAPBLK_NONE is a flag, basically the high bit.
 */

#define SWAPBLK_MASK	((daddr_t)((u_daddr_t)-1 >> 1))		/* mask */
#define SWAPBLK_NONE	((daddr_t)((u_daddr_t)SWAPBLK_MASK + 1))/* flag */

#if !defined(KLD_MODULE)

/*
 * Page coloring parameters
 */
/* Each of PQ_FREE, and PQ_CACHE have PQ_HASH_SIZE entries */

/* Define one of the following */
#if defined(PQ_HUGECACHE)
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME3 17	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 256	/* A number of colors opt for 1M cache */
#endif

/* Define one of the following */
#if defined(PQ_LARGECACHE)
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME3 17	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 128	/* A number of colors opt for 512K cache */
#endif


/*
 * Use 'options PQ_NOOPT' to disable page coloring
 */
#if defined(PQ_NOOPT)
#define PQ_PRIME1 1
#define PQ_PRIME2 1
#define PQ_PRIME3 1
#define PQ_L2_SIZE 1
#endif

#if defined(PQ_NORMALCACHE)
#define PQ_PRIME1 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 3	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME3 11	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 16	/* A reasonable number of colors (opt for 64K cache) */
#endif

#if defined(PQ_MEDIUMCACHE) || !defined(PQ_L2_SIZE)
#define PQ_PRIME1 13	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 7	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME3 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 64	/* A number of colors opt for 256K cache */
#endif

#define PQ_L2_MASK (PQ_L2_SIZE - 1)

#define PQ_NONE 0
#define PQ_FREE	1
/* #define PQ_ZERO (1 + PQ_L2_SIZE) */
#define PQ_INACTIVE (1 + 1*PQ_L2_SIZE)
#define PQ_ACTIVE (2 + 1*PQ_L2_SIZE)
#define PQ_CACHE (3 + 1*PQ_L2_SIZE)
#define PQ_COUNT (3 + 2*PQ_L2_SIZE)

extern struct vpgqueues {
	struct pglist *pl;
	int	*cnt;
	int	*lcnt;
} vm_page_queues[PQ_COUNT];

#endif

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_FILLED and PG_DIRTY are added for the filesystems.
 */
#define	PG_BUSY		0x0001		/* page is in transit (O) */
#define	PG_WANTED	0x0002		/* someone is waiting for page (O) */
#define	PG_FICTITIOUS	0x0008		/* physical page doesn't exist (O) */
#define	PG_WRITEABLE	0x0010		/* page is mapped writeable */
#define PG_MAPPED	0x0020		/* page is mapped */
#define	PG_ZERO		0x0040		/* page is zeroed */
#define PG_REFERENCED	0x0080		/* page has been referenced */
#define PG_CLEANCHK	0x0100		/* page will be checked for cleaning */
#define PG_SWAPINPROG	0x0200		/* swap I/O in progress on page	     */

/*
 * Misc constants.
 */

#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_INIT		5
#define ACT_MAX			64
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

#if !defined(KLD_MODULE)

extern struct pglist vm_page_queue_free[PQ_L2_SIZE];/* memory free queue */
extern struct pglist vm_page_queue_active;	/* active memory queue */
extern struct pglist vm_page_queue_inactive;	/* inactive memory queue */
extern struct pglist vm_page_queue_cache[PQ_L2_SIZE];/* cache memory queue */

#endif

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

static __inline void
vm_page_flag_set(vm_page_t m, unsigned int bits)
{
	atomic_set_short(&(m)->flags, bits);
}

static __inline void
vm_page_flag_clear(vm_page_t m, unsigned int bits)
{
	atomic_clear_short(&(m)->flags, bits);
}

#if 0
static __inline void
vm_page_assert_wait(vm_page_t m, int interruptible)
{
	vm_page_flag_set(m, PG_WANTED);
	assert_wait((int) m, interruptible);
}
#endif

static __inline void
vm_page_busy(vm_page_t m)
{
	KASSERT((m->flags & PG_BUSY) == 0, ("vm_page_busy: page already busy!!!"));
	vm_page_flag_set(m, PG_BUSY);
}

/*
 *	vm_page_flash:
 *
 *	wakeup anyone waiting for the page.
 */

static __inline void
vm_page_flash(vm_page_t m)
{
	if (m->flags & PG_WANTED) {
		vm_page_flag_clear(m, PG_WANTED);
		wakeup(m);
	}
}

/*
 *	vm_page_wakeup:
 *
 *	clear the PG_BUSY flag and wakeup anyone waiting for the
 *	page.
 *
 */

static __inline void
vm_page_wakeup(vm_page_t m)
{
	KASSERT(m->flags & PG_BUSY, ("vm_page_wakeup: page not busy!!!"));
	vm_page_flag_clear(m, PG_BUSY);
	vm_page_flash(m);
}

/*
 *
 *
 */

static __inline void
vm_page_io_start(vm_page_t m)
{
	atomic_add_char(&(m)->busy, 1);
}

static __inline void
vm_page_io_finish(vm_page_t m)
{
	atomic_subtract_char(&m->busy, 1);
	if (m->busy == 0)
		vm_page_flash(m);
}


#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xff
#endif

#if PAGE_SIZE == 8192
#define VM_PAGE_BITS_ALL 0xffff
#endif

#define VM_ALLOC_NORMAL		0
#define VM_ALLOC_INTERRUPT	1
#define VM_ALLOC_SYSTEM		2
#define	VM_ALLOC_ZERO		3
#define	VM_ALLOC_RETRY		0x80

void vm_page_activate __P((vm_page_t));
vm_page_t vm_page_alloc __P((vm_object_t, vm_pindex_t, int));
vm_page_t vm_page_grab __P((vm_object_t, vm_pindex_t, int));
void vm_page_cache __P((register vm_page_t));
static __inline void vm_page_copy __P((vm_page_t, vm_page_t));
static __inline void vm_page_free __P((vm_page_t));
static __inline void vm_page_free_zero __P((vm_page_t));
void vm_page_destroy __P((vm_page_t));
void vm_page_deactivate __P((vm_page_t));
void vm_page_insert __P((vm_page_t, vm_object_t, vm_pindex_t));
vm_page_t vm_page_lookup __P((vm_object_t, vm_pindex_t));
void vm_page_remove __P((vm_page_t));
void vm_page_rename __P((vm_page_t, vm_object_t, vm_pindex_t));
vm_offset_t vm_page_startup __P((vm_offset_t, vm_offset_t, vm_offset_t));
void vm_page_unwire __P((vm_page_t, int));
void vm_page_wire __P((vm_page_t));
void vm_page_unqueue __P((vm_page_t));
void vm_page_unqueue_nowakeup __P((vm_page_t));
void vm_page_set_validclean __P((vm_page_t, int, int));
void vm_page_set_invalid __P((vm_page_t, int, int));
static __inline boolean_t vm_page_zero_fill __P((vm_page_t));
int vm_page_is_valid __P((vm_page_t, int, int));
void vm_page_test_dirty __P((vm_page_t));
int vm_page_bits __P((int, int));
vm_page_t _vm_page_list_find __P((int, int));
int vm_page_queue_index __P((vm_offset_t, int));
#if 0
int vm_page_sleep(vm_page_t m, char *msg, char *busy);
int vm_page_asleep(vm_page_t m, char *msg, char *busy);
#endif
void vm_page_free_toq(vm_page_t m);

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

static __inline void
vm_page_unhold(vm_page_t mem)
{
	--mem->hold_count;
	KASSERT(mem->hold_count >= 0, ("vm_page_unhold: hold count < 0!!!"));
}

/*
 * 	vm_page_protect:
 *
 *	Reduce the protection of a page.  This routine never
 *	raises the protection and therefore can be safely
 *	called if the page is already at VM_PROT_NONE ( it
 *	will be a NOP effectively ).
 */

static __inline void
vm_page_protect(vm_page_t mem, int prot)
{
	if (prot == VM_PROT_NONE) {
		if (mem->flags & (PG_WRITEABLE|PG_MAPPED)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(mem), VM_PROT_NONE);
			vm_page_flag_clear(mem, PG_WRITEABLE|PG_MAPPED);
		}
	} else if ((prot == VM_PROT_READ) && (mem->flags & PG_WRITEABLE)) {
		pmap_page_protect(VM_PAGE_TO_PHYS(mem), VM_PROT_READ);
		vm_page_flag_clear(mem, PG_WRITEABLE);
	}
}

/*
 *	vm_page_zero_fill:
 *
 *	Zero-fill the specified page.
 *	Written as a standard pagein routine, to
 *	be used by the zero-fill object.
 */
static __inline boolean_t
vm_page_zero_fill(m)
	vm_page_t m;
{
	pmap_zero_page(VM_PAGE_TO_PHYS(m));
	return (TRUE);
}

/*
 *	vm_page_copy:
 *
 *	Copy one page to another
 */
static __inline void
vm_page_copy(src_m, dest_m)
	vm_page_t src_m;
	vm_page_t dest_m;
{
	pmap_copy_page(VM_PAGE_TO_PHYS(src_m), VM_PAGE_TO_PHYS(dest_m));
	dest_m->valid = VM_PAGE_BITS_ALL;
}

/*
 *	vm_page_free:
 *
 *	Free a page
 *
 *	The clearing of PG_ZERO is a temporary safety until the code can be
 *	reviewed to determine that PG_ZERO is being properly cleared on
 *	write faults or maps.  PG_ZERO was previously cleared in 
 *	vm_page_alloc().
 */
static __inline void
vm_page_free(m)
	vm_page_t m;
{
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_free_toq(m);
}

/*
 *	vm_page_free_zero:
 *
 *	Free a page to the zerod-pages queue
 */
static __inline void
vm_page_free_zero(m)
	vm_page_t m;
{
	vm_page_flag_set(m, PG_ZERO);
	vm_page_free_toq(m);
}

/*
 *	vm_page_sleep_busy:
 *
 *	Wait until page is no longer PG_BUSY or (if also_m_busy is TRUE)
 *	m->busy is zero.  Returns TRUE if it had to sleep ( including if 
 *	it almost had to sleep and made temporary spl*() mods), FALSE 
 *	otherwise.
 *
 *	This routine assumes that interrupts can only remove the busy
 *	status from a page, not set the busy status or change it from
 *	PG_BUSY to m->busy or vise versa (which would create a timing
 *	window).
 *
 *	Note that being an inline, this code will be well optimized.
 */

static __inline int
vm_page_sleep_busy(vm_page_t m, int also_m_busy, const char *msg)
{
	if ((m->flags & PG_BUSY) || (also_m_busy && m->busy))  {
		int s = splvm();
		if ((m->flags & PG_BUSY) || (also_m_busy && m->busy)) {
			/*
			 * Page is busy. Wait and retry.
			 */
			vm_page_flag_set(m, PG_WANTED | PG_REFERENCED);
			tsleep(m, PVM, msg, 0);
		}
		splx(s);
		return(TRUE);
		/* not reached */
	}
	return(FALSE);
}

/*
 *	vm_page_dirty:
 *
 *	make page all dirty
 */

static __inline void
vm_page_dirty(vm_page_t m)
{
	KASSERT(m->queue - m->pc != PQ_CACHE, ("vm_page_dirty: page in cache!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

#if !defined(KLD_MODULE)

static __inline vm_page_t
vm_page_list_find(int basequeue, int index, boolean_t prefer_zero)
{
	vm_page_t m;

#if PQ_L2_SIZE > 1
	if (prefer_zero) {
		m = TAILQ_LAST(vm_page_queues[basequeue+index].pl, pglist);
	} else {
		m = TAILQ_FIRST(vm_page_queues[basequeue+index].pl);
	}
	if (m == NULL)
		m = _vm_page_list_find(basequeue, index);
#else
	if (prefer_zero) {
		m = TAILQ_LAST(vm_page_queues[basequeue].pl, pglist);
	} else {
		m = TAILQ_FIRST(vm_page_queues[basequeue].pl);
	}
#endif
	return(m);
}

#endif

#endif				/* KERNEL */
#endif				/* !_VM_PAGE_ */
