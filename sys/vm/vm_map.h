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
 *	@(#)vm_map.h	8.9 (Berkeley) 5/17/95
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
 *	Virtual memory map module definitions.
 */

#ifndef	_VM_MAP_
#define	_VM_MAP_

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 */

typedef u_int vm_eflags_t;

/*
 *	Objects which live in maps may be either VM objects, or
 *	another map (called a "sharing map") which denotes read-write
 *	sharing with other maps.
 */

union vm_map_object {
	struct vm_object *vm_object;	/* object object */
	struct vm_map *sub_map;		/* belongs to another map */
};

/*
 *	Address map entries consist of start and end addresses,
 *	a VM object (or sharing map) and offset into that object,
 *	and user-exported inheritance and protection information.
 *	Also included is control information for virtual copy operations.
 */
struct vm_map_entry {
	struct vm_map_entry *prev;	/* previous entry */
	struct vm_map_entry *next;	/* next entry */
	vm_offset_t start;		/* start address */
	vm_offset_t end;		/* end address */
	vm_offset_t avail_ssize;	/* amt can grow if this is a stack */
	union vm_map_object object;	/* object I point to */
	vm_ooffset_t offset;		/* offset into object */
	vm_eflags_t eflags;		/* map entry flags */
	/* Only in task maps: */
	vm_prot_t protection;		/* protection code */
	vm_prot_t max_protection;	/* maximum protection */
	vm_inherit_t inheritance;	/* inheritance */
	int wired_count;		/* can be paged if = 0 */
	vm_pindex_t lastr;		/* last read */
};

#define MAP_ENTRY_NOSYNC		0x0001
#define MAP_ENTRY_IS_SUB_MAP		0x0002
#define MAP_ENTRY_COW			0x0004
#define MAP_ENTRY_NEEDS_COPY		0x0008
#define MAP_ENTRY_NOFAULT		0x0010
#define MAP_ENTRY_USER_WIRED		0x0020

#define MAP_ENTRY_BEHAV_NORMAL		0x0000	/* default behavior */
#define MAP_ENTRY_BEHAV_SEQUENTIAL	0x0040	/* expect sequential access */
#define MAP_ENTRY_BEHAV_RANDOM		0x0080	/* expect random access */
#define MAP_ENTRY_BEHAV_RESERVED	0x00C0	/* future use */

#define MAP_ENTRY_BEHAV_MASK		0x00C0

#define MAP_ENTRY_IN_TRANSITION		0x0100	/* entry being changed */
#define MAP_ENTRY_NEEDS_WAKEUP		0x0200	/* waiter's in transition */
#define MAP_ENTRY_NOCOREDUMP		0x0400	/* don't include in a core */

/*
 * flags for vm_map_[un]clip_range()
 */
#define MAP_CLIP_NO_HOLES		0x0001

static __inline u_char   
vm_map_entry_behavior(struct vm_map_entry *entry)
{                  
	return entry->eflags & MAP_ENTRY_BEHAV_MASK;
}

static __inline void
vm_map_entry_set_behavior(struct vm_map_entry *entry, u_char behavior)
{              
	entry->eflags = (entry->eflags & ~MAP_ENTRY_BEHAV_MASK) |
		(behavior & MAP_ENTRY_BEHAV_MASK);
}                       

/*
 *	Maps are doubly-linked lists of map entries, kept sorted
 *	by address.  A single hint is provided to start
 *	searches again from the last successful search,
 *	insertion, or removal.
 *
 *	Note: the lock structure cannot be the first element of vm_map
 *	because this can result in a running lockup between two or more
 *	system processes trying to kmem_alloc_wait() due to kmem_alloc_wait()
 *	and free tsleep/waking up 'map' and the underlying lockmgr also
 *	sleeping and waking up on 'map'.  The lockup occurs when the map fills
 *	up.  The 'exec' map, for example.
 */
struct vm_map {
	struct vm_map_entry header;	/* List of entries */
	struct lock lock;		/* Lock for map data */
	int nentries;			/* Number of entries */
	vm_size_t size;			/* virtual size */
	u_char system_map;		/* Am I a system map? */
	u_char infork;			/* Am I in fork processing? */
	vm_map_entry_t hint;		/* hint for quick lookups */
	unsigned int timestamp;		/* Version number */
	vm_map_entry_t first_free;	/* First free space hint */
	struct pmap *pmap;		/* Physical map */
#define	min_offset		header.start
#define max_offset		header.end
};

/* 
 * Shareable process virtual address space.
 * May eventually be merged with vm_map.
 * Several fields are temporary (text, data stuff).
 */
struct vmspace {
	struct vm_map vm_map;	/* VM address map */
	struct pmap vm_pmap;	/* private physical map */
	int vm_refcnt;		/* number of references */
	caddr_t vm_shm;		/* SYS5 shared memory private data XXX */
/* we copy between vm_startcopy and vm_endcopy on fork */
#define vm_startcopy vm_rssize
	segsz_t vm_rssize;	/* current resident set size in pages */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t vm_taddr;	/* user virtual address of text XXX */
	caddr_t vm_daddr;	/* user virtual address of data XXX */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
	caddr_t vm_minsaddr;	/* user VA at max stack growth */
#define	vm_endcopy vm_exitingcnt
	int	vm_exitingcnt;	/* several procsses zombied in exit1 */
};

/*
 *	Macros:		vm_map_lock, etc.
 *	Function:
 *		Perform locking on the data portion of a map.  Note that
 *		these macros mimic procedure calls returning void.  The
 *		semicolon is supplied by the user of these macros, not
 *		by the macros themselves.  The macros can safely be used
 *		as unbraced elements in a higher level statement.
 */

#define	vm_map_lock_drain_interlock(map) \
	do { \
		lockmgr(&(map)->lock, LK_DRAIN|LK_INTERLOCK, \
			&(map)->ref_lock, curproc); \
		(map)->timestamp++; \
	} while(0)

#ifdef DIAGNOSTIC
/* #define MAP_LOCK_DIAGNOSTIC 1 */
#ifdef MAP_LOCK_DIAGNOSTIC
#define	vm_map_lock(map) \
	do { \
		printf ("locking map LK_EXCLUSIVE: 0x%x\n", map); \
		if (lockmgr(&(map)->lock, LK_EXCLUSIVE, (void *)0, curproc) != 0) { \
			panic("vm_map_lock: failed to get lock"); \
		} \
		(map)->timestamp++; \
	} while(0)
#else
#define	vm_map_lock(map) \
	do { \
		if (lockmgr(&(map)->lock, LK_EXCLUSIVE, (void *)0, curproc) != 0) { \
			panic("vm_map_lock: failed to get lock"); \
		} \
		(map)->timestamp++; \
	} while(0)
#endif
#else
#define	vm_map_lock(map) \
	do { \
		lockmgr(&(map)->lock, LK_EXCLUSIVE, (void *)0, curproc); \
		(map)->timestamp++; \
	} while(0)
#endif /* DIAGNOSTIC */

#if defined(MAP_LOCK_DIAGNOSTIC)
#define	vm_map_unlock(map) \
	do { \
		printf ("locking map LK_RELEASE: 0x%x\n", map); \
		lockmgr(&(map)->lock, LK_RELEASE, (void *)0, curproc); \
	} while (0)
#define	vm_map_lock_read(map) \
	do { \
		printf ("locking map LK_SHARED: 0x%x\n", map); \
		lockmgr(&(map)->lock, LK_SHARED, (void *)0, curproc); \
	} while (0)
#define	vm_map_unlock_read(map) \
	do { \
		printf ("locking map LK_RELEASE: 0x%x\n", map); \
		lockmgr(&(map)->lock, LK_RELEASE, (void *)0, curproc); \
	} while (0)
#else
#define	vm_map_unlock(map) \
	lockmgr(&(map)->lock, LK_RELEASE, (void *)0, curproc)
#define	vm_map_lock_read(map) \
	lockmgr(&(map)->lock, LK_SHARED, (void *)0, curproc) 
#define	vm_map_unlock_read(map) \
	lockmgr(&(map)->lock, LK_RELEASE, (void *)0, curproc)
#endif

static __inline__ int
_vm_map_lock_upgrade(vm_map_t map, struct proc *p) {
	int error;
#if defined(MAP_LOCK_DIAGNOSTIC)
	printf("locking map LK_EXCLUPGRADE: 0x%x\n", map); 
#endif
	error = lockmgr(&map->lock, LK_EXCLUPGRADE, (void *)0, p);
	if (error == 0)
		map->timestamp++;
	return error;
}

#define vm_map_lock_upgrade(map) _vm_map_lock_upgrade(map, curproc)

#if defined(MAP_LOCK_DIAGNOSTIC)
#define vm_map_lock_downgrade(map) \
	do { \
		printf ("locking map LK_DOWNGRADE: 0x%x\n", map); \
		lockmgr(&(map)->lock, LK_DOWNGRADE, (void *)0, curproc); \
	} while (0)
#else
#define vm_map_lock_downgrade(map) \
	lockmgr(&(map)->lock, LK_DOWNGRADE, (void *)0, curproc)
#endif

#define vm_map_set_recursive(map) \
	do { \
		simple_lock(&(map)->lock.lk_interlock); \
		(map)->lock.lk_flags |= LK_CANRECURSE; \
		simple_unlock(&(map)->lock.lk_interlock); \
	} while(0)
#define vm_map_clear_recursive(map) \
	do { \
		simple_lock(&(map)->lock.lk_interlock); \
		(map)->lock.lk_flags &= ~LK_CANRECURSE; \
		simple_unlock(&(map)->lock.lk_interlock); \
	} while(0)

/*
 *	Functions implemented as macros
 */
#define		vm_map_min(map)		((map)->min_offset)
#define		vm_map_max(map)		((map)->max_offset)
#define		vm_map_pmap(map)	((map)->pmap)

static __inline struct pmap *
vmspace_pmap(struct vmspace *vmspace)
{
	return &vmspace->vm_pmap;
}

static __inline long
vmspace_resident_count(struct vmspace *vmspace)
{
	return pmap_resident_count(vmspace_pmap(vmspace));
}

/* XXX: number of kernel maps and entries to statically allocate */
#define MAX_KMAP	10
#define	MAX_KMAPENT	128
#define	MAX_MAPENT	128

/*
 * Copy-on-write flags for vm_map operations
 */
#define MAP_UNUSED_01		0x0001
#define MAP_COPY_ON_WRITE	0x0002
#define MAP_NOFAULT		0x0004
#define MAP_PREFAULT		0x0008
#define MAP_PREFAULT_PARTIAL	0x0010
#define MAP_DISABLE_SYNCER	0x0020
#define MAP_DISABLE_COREDUMP	0x0100
#define MAP_PREFAULT_MADVISE	0x0200	/* from (user) madvise request */

/*
 * vm_fault option flags
 */
#define VM_FAULT_NORMAL 0		/* Nothing special */
#define VM_FAULT_CHANGE_WIRING 1	/* Change the wiring as appropriate */
#define VM_FAULT_USER_WIRE 2		/* Likewise, but for user purposes */
#define VM_FAULT_WIRE_MASK (VM_FAULT_CHANGE_WIRING|VM_FAULT_USER_WIRE)
#define	VM_FAULT_HOLD 4			/* Hold the page */
#define VM_FAULT_DIRTY 8		/* Dirty the page */

#ifdef _KERNEL
boolean_t vm_map_check_protection __P((vm_map_t, vm_offset_t, vm_offset_t, vm_prot_t));
struct pmap;
vm_map_t vm_map_create __P((struct pmap *, vm_offset_t, vm_offset_t));
int vm_map_delete __P((vm_map_t, vm_offset_t, vm_offset_t));
int vm_map_find __P((vm_map_t, vm_object_t, vm_ooffset_t, vm_offset_t *, vm_size_t, boolean_t, vm_prot_t, vm_prot_t, int));
int vm_map_findspace __P((vm_map_t, vm_offset_t, vm_size_t, vm_offset_t *));
int vm_map_inherit __P((vm_map_t, vm_offset_t, vm_offset_t, vm_inherit_t));
void vm_map_init __P((struct vm_map *, vm_offset_t, vm_offset_t));
int vm_map_insert __P((vm_map_t, vm_object_t, vm_ooffset_t, vm_offset_t, vm_offset_t, vm_prot_t, vm_prot_t, int));
int vm_map_lookup __P((vm_map_t *, vm_offset_t, vm_prot_t, vm_map_entry_t *, vm_object_t *,
    vm_pindex_t *, vm_prot_t *, boolean_t *));
void vm_map_lookup_done __P((vm_map_t, vm_map_entry_t));
boolean_t vm_map_lookup_entry __P((vm_map_t, vm_offset_t, vm_map_entry_t *));
int vm_map_pageable __P((vm_map_t, vm_offset_t, vm_offset_t, boolean_t));
int vm_map_user_pageable __P((vm_map_t, vm_offset_t, vm_offset_t, boolean_t));
int vm_map_clean __P((vm_map_t, vm_offset_t, vm_offset_t, boolean_t, boolean_t));
int vm_map_protect __P((vm_map_t, vm_offset_t, vm_offset_t, vm_prot_t, boolean_t));
int vm_map_remove __P((vm_map_t, vm_offset_t, vm_offset_t));
void vm_map_startup __P((void));
int vm_map_submap __P((vm_map_t, vm_offset_t, vm_offset_t, vm_map_t));
int vm_map_madvise __P((vm_map_t, vm_offset_t, vm_offset_t, int));
void vm_map_simplify_entry __P((vm_map_t, vm_map_entry_t));
void vm_init2 __P((void));
int vm_uiomove __P((vm_map_t, vm_object_t, off_t, int, vm_offset_t, int *));
void vm_freeze_copyopts __P((vm_object_t, vm_pindex_t, vm_pindex_t));
int vm_map_stack __P((vm_map_t, vm_offset_t, vm_size_t, vm_prot_t, vm_prot_t, int));
int vm_map_growstack __P((struct proc *p, vm_offset_t addr));
int vmspace_swap_count __P((struct vmspace *vmspace));

#endif
#endif				/* _VM_MAP_ */
