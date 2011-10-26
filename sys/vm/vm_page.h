/*-
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
 *	In general, operations on this structure's mutable fields are
 *	synchronized using either one of or a combination of the lock on the
 *	object that the page belongs to (O), the pool lock for the page (P),
 *	or the lock for either the free or paging queues (Q).  If a field is
 *	annotated below with two of these locks, then holding either lock is
 *	sufficient for read access, but both locks are required for write 
 *	access.
 *
 *	In contrast, the synchronization of accesses to the page's dirty field
 *	is machine dependent (M).  In the machine-independent layer, the lock
 *	on the object that the page belongs to must be held in order to
 *	operate on the field.  However, the pmap layer is permitted to set
 *	all bits within the field without holding that lock.  Therefore, if
 *	the underlying architecture does not support atomic read-modify-write
 *	operations on the field's type, then the machine-independent layer
 *	must also hold the page queues lock when performing read-modify-write
 *	operations and the pmap layer must hold the page queues lock when
 *	setting the field.  In the machine-independent layer, the
 *	implementation of read-modify-write operations on the field is
 *	encapsulated in vm_page_clear_dirty_mask().
 */

TAILQ_HEAD(pglist, vm_page);

struct vm_page {
	TAILQ_ENTRY(vm_page) pageq;	/* queue info for FIFO queue or free list (Q) */
	TAILQ_ENTRY(vm_page) listq;	/* pages in same object (O) 	*/
	struct vm_page *left;		/* splay tree link (O)		*/
	struct vm_page *right;		/* splay tree link (O)		*/

	vm_object_t object;		/* which object am I in (O,P)*/
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_paddr_t phys_addr;		/* physical address of page */
	struct md_page md;		/* machine dependant stuff */
	uint8_t	queue;			/* page queue index (P,Q) */
	int8_t segind;
	short hold_count;		/* page hold count (P) */
	uint8_t	order;			/* index of the buddy queue */
	uint8_t pool;
	u_short cow;			/* page cow mapping count (P) */
	u_int wire_count;		/* wired down maps refs (P) */
	uint8_t aflags;			/* access is atomic */
	uint8_t flags;			/* see below, often immutable after alloc */
	u_short oflags;			/* page flags (O) */
	u_char	act_count;		/* page usage count (O) */
	u_char	busy;			/* page busy count (O) */
	/* NOTE that these must support one bit per DEV_BSIZE in a page!!! */
	/* so, on normal X86 kernels, they must be at least 8 bits wide */
	/* In reality, support for 32KB pages is not fully implemented. */
#if PAGE_SIZE == 4096
	u_char	valid;			/* map of valid DEV_BSIZE chunks (O) */
	u_char	dirty;			/* map of dirty DEV_BSIZE chunks (M) */
#elif PAGE_SIZE == 8192
	u_short	valid;			/* map of valid DEV_BSIZE chunks (O) */
	u_short	dirty;			/* map of dirty DEV_BSIZE chunks (M) */
#elif PAGE_SIZE == 16384
	u_int valid;			/* map of valid DEV_BSIZE chunks (O) */
	u_int dirty;			/* map of dirty DEV_BSIZE chunks (M) */
#elif PAGE_SIZE == 32768
	u_long valid;			/* map of valid DEV_BSIZE chunks (O) */
	u_long dirty;			/* map of dirty DEV_BSIZE chunks (M) */
#endif
};

/*
 * Page flags stored in oflags:
 *
 * Access to these page flags is synchronized by the lock on the object
 * containing the page (O).
 *
 * Note: VPO_UNMANAGED (used by OBJT_DEVICE, OBJT_PHYS and OBJT_SG)
 * 	 indicates that the page is not under PV management but
 * 	 otherwise should be treated as a normal page.  Pages not
 * 	 under PV management cannot be paged out via the
 * 	 object/vm_page_t because there is no knowledge of their pte
 * 	 mappings, and such pages are also not on any PQ queue.
 *
 */
#define	VPO_BUSY	0x0001	/* page is in transit */
#define	VPO_WANTED	0x0002	/* someone is waiting for page */
#define	VPO_UNMANAGED	0x0004		/* No PV management for page */
#define	VPO_SWAPINPROG	0x0200	/* swap I/O in progress on page */
#define	VPO_NOSYNC	0x0400	/* do not collect for syncer */

#define	PQ_NONE		255
#define	PQ_INACTIVE	0
#define	PQ_ACTIVE	1
#define	PQ_HOLD		2
#define	PQ_COUNT	3

struct vpgqueues {
	struct pglist pl;
	int	*cnt;
};

extern struct vpgqueues vm_page_queues[PQ_COUNT];

struct vpglocks {
	struct mtx	data;
	char		pad[CACHE_LINE_SIZE - sizeof(struct mtx)];
} __aligned(CACHE_LINE_SIZE);

extern struct vpglocks vm_page_queue_free_lock;
extern struct vpglocks pa_lock[];

#if defined(__arm__)
#define	PDRSHIFT	PDR_SHIFT
#elif !defined(PDRSHIFT)
#define PDRSHIFT	21
#endif

#define	pa_index(pa)	((pa) >> PDRSHIFT)
#define	PA_LOCKPTR(pa)	&pa_lock[pa_index((pa)) % PA_LOCK_COUNT].data
#define	PA_LOCKOBJPTR(pa)	((struct lock_object *)PA_LOCKPTR((pa)))
#define	PA_LOCK(pa)	mtx_lock(PA_LOCKPTR(pa))
#define	PA_TRYLOCK(pa)	mtx_trylock(PA_LOCKPTR(pa))
#define	PA_UNLOCK(pa)	mtx_unlock(PA_LOCKPTR(pa))
#define	PA_UNLOCK_COND(pa) 			\
	do {		   			\
		if ((pa) != 0) {		\
			PA_UNLOCK((pa));	\
			(pa) = 0;		\
		}				\
	} while (0)

#define	PA_LOCK_ASSERT(pa, a)	mtx_assert(PA_LOCKPTR(pa), (a))

#define	vm_page_lockptr(m)	(PA_LOCKPTR(VM_PAGE_TO_PHYS((m))))
#define	vm_page_lock(m)		mtx_lock(vm_page_lockptr((m)))
#define	vm_page_unlock(m)	mtx_unlock(vm_page_lockptr((m)))
#define	vm_page_trylock(m)	mtx_trylock(vm_page_lockptr((m)))
#define	vm_page_lock_assert(m, a)	mtx_assert(vm_page_lockptr((m)), (a))

#define	vm_page_queue_free_mtx	vm_page_queue_free_lock.data
/*
 * These are the flags defined for vm_page.
 *
 * aflags are updated by atomic accesses. Use the vm_page_aflag_set()
 * and vm_page_aflag_clear() functions to set and clear the flags.
 *
 * PGA_REFERENCED may be cleared only if the object containing the page is
 * locked.
 *
 * PGA_WRITEABLE is set exclusively on managed pages by pmap_enter().  When it
 * does so, the page must be VPO_BUSY.
 */
#define	PGA_WRITEABLE	0x01		/* page may be mapped writeable */
#define	PGA_REFERENCED	0x02		/* page has been referenced */

/*
 * Page flags.  If changed at any other time than page allocation or
 * freeing, the modification must be protected by the vm_page lock.
 */
#define	PG_CACHED	0x01		/* page is cached */
#define	PG_FREE		0x02		/* page is free */
#define	PG_FICTITIOUS	0x04		/* physical page doesn't exist (O) */
#define	PG_ZERO		0x08		/* page is zeroed */
#define	PG_MARKER	0x10		/* special queue marker page */
#define	PG_SLAB		0x20		/* object pointer is actually a slab */
#define	PG_WINATCFLS	0x40		/* flush dirty page on inactive q */

/*
 * Misc constants.
 */
#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_INIT		5
#define ACT_MAX			64

#ifdef _KERNEL

#include <vm/vm_param.h>

/*
 * Each pageable resident page falls into one of five lists:
 *
 *	free
 *		Available for allocation now.
 *
 *	cache
 *		Almost available for allocation. Still associated with
 *		an object, but clean and immediately freeable.
 *
 *	hold
 *		Will become free after a pending I/O operation
 *		completes.
 *
 * The following lists are LRU sorted:
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
 */

struct vnode;
extern int vm_page_zero_count;

extern vm_page_t vm_page_array;		/* First resident page in table */
extern int vm_page_array_size;		/* number of vm_page_t's */
extern long first_page;			/* first physical page number */

#define	VM_PAGE_IS_FREE(m)	(((m)->flags & PG_FREE) != 0)

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

vm_page_t vm_phys_paddr_to_vm_page(vm_paddr_t pa);

static __inline vm_page_t PHYS_TO_VM_PAGE(vm_paddr_t pa);

static __inline vm_page_t
PHYS_TO_VM_PAGE(vm_paddr_t pa)
{
#ifdef VM_PHYSSEG_SPARSE
	return (vm_phys_paddr_to_vm_page(pa));
#elif defined(VM_PHYSSEG_DENSE)
	return (&vm_page_array[atop(pa) - first_page]);
#else
#error "Either VM_PHYSSEG_DENSE or VM_PHYSSEG_SPARSE must be defined."
#endif
}

extern struct vpglocks vm_page_queue_lock;

#define	vm_page_queue_mtx	vm_page_queue_lock.data
#define vm_page_lock_queues()   mtx_lock(&vm_page_queue_mtx)
#define vm_page_unlock_queues() mtx_unlock(&vm_page_queue_mtx)

#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xffu
#elif PAGE_SIZE == 8192
#define VM_PAGE_BITS_ALL 0xffffu
#elif PAGE_SIZE == 16384
#define VM_PAGE_BITS_ALL 0xffffffffu
#elif PAGE_SIZE == 32768
#define VM_PAGE_BITS_ALL 0xfffffffffffffffflu
#endif

/* page allocation classes: */
#define VM_ALLOC_NORMAL		0
#define VM_ALLOC_INTERRUPT	1
#define VM_ALLOC_SYSTEM		2
#define	VM_ALLOC_CLASS_MASK	3
/* page allocation flags: */
#define	VM_ALLOC_WIRED		0x0020	/* non pageable */
#define	VM_ALLOC_ZERO		0x0040	/* Try to obtain a zeroed page */
#define	VM_ALLOC_RETRY		0x0080	/* Mandatory with vm_page_grab() */
#define	VM_ALLOC_NOOBJ		0x0100	/* No associated object */
#define	VM_ALLOC_NOBUSY		0x0200	/* Do not busy the page */
#define	VM_ALLOC_IFCACHED	0x0400	/* Fail if the page is not cached */
#define	VM_ALLOC_IFNOTCACHED	0x0800	/* Fail if the page is cached */
#define	VM_ALLOC_IGN_SBUSY	0x1000	/* vm_page_grab() only */

#define	VM_ALLOC_COUNT_SHIFT	16
#define	VM_ALLOC_COUNT(count)	((count) << VM_ALLOC_COUNT_SHIFT)

void vm_page_aflag_set(vm_page_t m, uint8_t bits);
void vm_page_aflag_clear(vm_page_t m, uint8_t bits);
void vm_page_busy(vm_page_t m);
void vm_page_flash(vm_page_t m);
void vm_page_io_start(vm_page_t m);
void vm_page_io_finish(vm_page_t m);
void vm_page_hold(vm_page_t mem);
void vm_page_unhold(vm_page_t mem);
void vm_page_free(vm_page_t m);
void vm_page_free_zero(vm_page_t m);
void vm_page_dirty(vm_page_t m);
void vm_page_wakeup(vm_page_t m);

void vm_pageq_remove(vm_page_t m);

void vm_page_activate (vm_page_t);
vm_page_t vm_page_alloc (vm_object_t, vm_pindex_t, int);
vm_page_t vm_page_alloc_freelist(int, int);
struct vnode *vm_page_alloc_init(vm_page_t);
vm_page_t vm_page_grab (vm_object_t, vm_pindex_t, int);
void vm_page_cache(vm_page_t);
void vm_page_cache_free(vm_object_t, vm_pindex_t, vm_pindex_t);
void vm_page_cache_remove(vm_page_t);
void vm_page_cache_transfer(vm_object_t, vm_pindex_t, vm_object_t);
int vm_page_try_to_cache (vm_page_t);
int vm_page_try_to_free (vm_page_t);
void vm_page_dontneed(vm_page_t);
void vm_page_deactivate (vm_page_t);
vm_page_t vm_page_find_least(vm_object_t, vm_pindex_t);
vm_page_t vm_page_getfake(vm_paddr_t paddr, vm_memattr_t memattr);
void vm_page_insert (vm_page_t, vm_object_t, vm_pindex_t);
vm_page_t vm_page_lookup (vm_object_t, vm_pindex_t);
vm_page_t vm_page_next(vm_page_t m);
int vm_page_pa_tryrelock(pmap_t, vm_paddr_t, vm_paddr_t *);
vm_page_t vm_page_prev(vm_page_t m);
void vm_page_putfake(vm_page_t m);
void vm_page_reference(vm_page_t m);
void vm_page_remove (vm_page_t);
void vm_page_rename (vm_page_t, vm_object_t, vm_pindex_t);
void vm_page_requeue(vm_page_t m);
void vm_page_set_valid(vm_page_t m, int base, int size);
void vm_page_sleep(vm_page_t m, const char *msg);
vm_page_t vm_page_splay(vm_pindex_t, vm_page_t);
vm_offset_t vm_page_startup(vm_offset_t vaddr);
void vm_page_unhold_pages(vm_page_t *ma, int count);
void vm_page_unwire (vm_page_t, int);
void vm_page_updatefake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr);
void vm_page_wire (vm_page_t);
void vm_page_set_validclean (vm_page_t, int, int);
void vm_page_clear_dirty (vm_page_t, int, int);
void vm_page_set_invalid (vm_page_t, int, int);
int vm_page_is_valid (vm_page_t, int, int);
void vm_page_test_dirty (vm_page_t);
int vm_page_bits (int, int);
void vm_page_zero_invalid(vm_page_t m, boolean_t setvalid);
void vm_page_free_toq(vm_page_t m);
void vm_page_zero_idle_wakeup(void);
void vm_page_cowfault (vm_page_t);
int vm_page_cowsetup(vm_page_t);
void vm_page_cowclear (vm_page_t);

#ifdef INVARIANTS
void vm_page_object_lock_assert(vm_page_t m);
#define	VM_PAGE_OBJECT_LOCK_ASSERT(m)	vm_page_object_lock_assert(m)
#else
#define	VM_PAGE_OBJECT_LOCK_ASSERT(m)	(void)0
#endif

/*
 *	vm_page_sleep_if_busy:
 *
 *	Sleep and release the page queues lock if VPO_BUSY is set or,
 *	if also_m_busy is TRUE, busy is non-zero.  Returns TRUE if the
 *	thread slept and the page queues lock was released.
 *	Otherwise, retains the page queues lock and returns FALSE.
 *
 *	The object containing the given page must be locked.
 */
static __inline int
vm_page_sleep_if_busy(vm_page_t m, int also_m_busy, const char *msg)
{

	if ((m->oflags & VPO_BUSY) || (also_m_busy && m->busy)) {
		vm_page_sleep(m, msg);
		return (TRUE);
	}
	return (FALSE);
}

/*
 *	vm_page_undirty:
 *
 *	Set page to not be dirty.  Note: does not clear pmap modify bits
 */
static __inline void
vm_page_undirty(vm_page_t m)
{

	VM_PAGE_OBJECT_LOCK_ASSERT(m);
	m->dirty = 0;
}

#endif				/* _KERNEL */
#endif				/* !_VM_PAGE_ */
