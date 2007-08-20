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

#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/_mutex.h>

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 */

typedef u_char vm_flags_t;
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
	struct vm_map_entry *left;	/* left child in binary search tree */
	struct vm_map_entry *right;	/* right child in binary search tree */
	vm_offset_t start;		/* start address */
	vm_offset_t end;		/* end address */
	vm_offset_t avail_ssize;	/* amt can grow if this is a stack */
	vm_size_t adj_free;		/* amount of adjacent free space */
	vm_size_t max_free;		/* max free space in subtree */
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
#define MAP_ENTRY_NEEDS_WAKEUP		0x0200	/* waiters in transition */
#define MAP_ENTRY_NOCOREDUMP		0x0400	/* don't include in a core */

#define	MAP_ENTRY_GROWS_DOWN		0x1000	/* Top-down stacks */
#define	MAP_ENTRY_GROWS_UP		0x2000	/* Bottom-up stacks */

#ifdef	_KERNEL
static __inline u_char
vm_map_entry_behavior(vm_map_entry_t entry)
{
	return (entry->eflags & MAP_ENTRY_BEHAV_MASK);
}

static __inline int
vm_map_entry_user_wired_count(vm_map_entry_t entry)
{
	if (entry->eflags & MAP_ENTRY_USER_WIRED)
		return (1);
	return (0);
}

static __inline int
vm_map_entry_system_wired_count(vm_map_entry_t entry)
{
	return (entry->wired_count - vm_map_entry_user_wired_count(entry));
}
#endif	/* _KERNEL */

/*
 *	A map is a set of map entries.  These map entries are
 *	organized both as a binary search tree and as a doubly-linked
 *	list.  Both structures are ordered based upon the start and
 *	end addresses contained within each map entry.  Sleator and
 *	Tarjan's top-down splay algorithm is employed to control
 *	height imbalance in the binary search tree.
 *
 *	Note: the lock structure cannot be the first element of vm_map
 *	because this can result in a running lockup between two or more
 *	system processes trying to kmem_alloc_wait() due to kmem_alloc_wait()
 *	and free tsleep/waking up 'map' and the underlying lockmgr also
 *	sleeping and waking up on 'map'.  The lockup occurs when the map fills
 *	up.  The 'exec' map, for example.
 *
 * List of locks
 *	(c)	const until freed
 */
struct vm_map {
	struct vm_map_entry header;	/* List of entries */
	struct sx lock;			/* Lock for map data */
	struct mtx system_mtx;
	int nentries;			/* Number of entries */
	vm_size_t size;			/* virtual size */
	u_int timestamp;		/* Version number */
	u_char needs_wakeup;
	u_char system_map;		/* Am I a system map? */
	vm_flags_t flags;		/* flags for this vm_map */
	vm_map_entry_t root;		/* Root of a binary search tree */
	pmap_t pmap;			/* (c) Physical map */
#define	min_offset	header.start	/* (c) */
#define	max_offset	header.end	/* (c) */
};

/*
 * vm_flags_t values
 */
#define MAP_WIREFUTURE		0x01	/* wire all future pages */

#ifdef	_KERNEL
static __inline vm_offset_t
vm_map_max(vm_map_t map)
{
	return (map->max_offset);
}

static __inline vm_offset_t
vm_map_min(vm_map_t map)
{
	return (map->min_offset);
}

static __inline pmap_t
vm_map_pmap(vm_map_t map)
{
	return (map->pmap);
}

static __inline void
vm_map_modflags(vm_map_t map, vm_flags_t set, vm_flags_t clear)
{
	map->flags = (map->flags | set) & ~clear;
}
#endif	/* _KERNEL */

/*
 * Shareable process virtual address space.
 *
 * List of locks
 *	(c)	const until freed
 */
struct vmspace {
	struct vm_map vm_map;	/* VM address map */
	struct pmap vm_pmap;	/* private physical map */
	struct shmmap_state *vm_shm;	/* SYS5 shared memory private data XXX */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t vm_taddr;	/* (c) user virtual address of text */
	caddr_t vm_daddr;	/* (c) user virtual address of data */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
	int	vm_refcnt;	/* number of references */
};

#ifdef	_KERNEL
static __inline pmap_t
vmspace_pmap(struct vmspace *vmspace)
{
	return &vmspace->vm_pmap;
}
#endif	/* _KERNEL */

#ifdef	_KERNEL
/*
 *	Macros:		vm_map_lock, etc.
 *	Function:
 *		Perform locking on the data portion of a map.  Note that
 *		these macros mimic procedure calls returning void.  The
 *		semicolon is supplied by the user of these macros, not
 *		by the macros themselves.  The macros can safely be used
 *		as unbraced elements in a higher level statement.
 */

void _vm_map_lock(vm_map_t map, const char *file, int line);
void _vm_map_unlock(vm_map_t map, const char *file, int line);
void _vm_map_lock_read(vm_map_t map, const char *file, int line);
void _vm_map_unlock_read(vm_map_t map, const char *file, int line);
int _vm_map_trylock(vm_map_t map, const char *file, int line);
int _vm_map_trylock_read(vm_map_t map, const char *file, int line);
int _vm_map_lock_upgrade(vm_map_t map, const char *file, int line);
void _vm_map_lock_downgrade(vm_map_t map, const char *file, int line);
int vm_map_unlock_and_wait(vm_map_t map, boolean_t user_wait);
void vm_map_wakeup(vm_map_t map);

#define	vm_map_lock(map)	_vm_map_lock(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_unlock(map)	_vm_map_unlock(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_lock_read(map)	_vm_map_lock_read(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_unlock_read(map)	_vm_map_unlock_read(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_trylock(map)	_vm_map_trylock(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_trylock_read(map)	\
			_vm_map_trylock_read(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_lock_upgrade(map)	\
			_vm_map_lock_upgrade(map, LOCK_FILE, LOCK_LINE)
#define	vm_map_lock_downgrade(map)	\
			_vm_map_lock_downgrade(map, LOCK_FILE, LOCK_LINE)

long vmspace_resident_count(struct vmspace *vmspace);
long vmspace_wired_count(struct vmspace *vmspace);
#endif	/* _KERNEL */


/* XXX: number of kernel maps and entries to statically allocate */
#define MAX_KMAP	10
#define	MAX_KMAPENT	128

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
#define	MAP_STACK_GROWS_DOWN	0x1000
#define	MAP_STACK_GROWS_UP	0x2000

/*
 * vm_fault option flags
 */
#define VM_FAULT_NORMAL 0		/* Nothing special */
#define VM_FAULT_CHANGE_WIRING 1	/* Change the wiring as appropriate */
#define VM_FAULT_USER_WIRE 2		/* Likewise, but for user purposes */
#define VM_FAULT_WIRE_MASK (VM_FAULT_CHANGE_WIRING|VM_FAULT_USER_WIRE)
#define VM_FAULT_DIRTY 8		/* Dirty the page */

/*
 * vm_map_wire and vm_map_unwire option flags
 */
#define VM_MAP_WIRE_SYSTEM	0	/* wiring in a kernel map */
#define VM_MAP_WIRE_USER	1	/* wiring in a user map */

#define VM_MAP_WIRE_NOHOLES	0	/* region must not have holes */
#define VM_MAP_WIRE_HOLESOK	2	/* region may have holes */

#ifdef _KERNEL
boolean_t vm_map_check_protection (vm_map_t, vm_offset_t, vm_offset_t, vm_prot_t);
vm_map_t vm_map_create(pmap_t, vm_offset_t, vm_offset_t);
int vm_map_delete (vm_map_t, vm_offset_t, vm_offset_t);
int vm_map_find (vm_map_t, vm_object_t, vm_ooffset_t, vm_offset_t *, vm_size_t, boolean_t, vm_prot_t, vm_prot_t, int);
int vm_map_fixed (vm_map_t, vm_object_t, vm_ooffset_t, vm_offset_t *, vm_size_t, vm_prot_t, vm_prot_t, int);
int vm_map_findspace (vm_map_t, vm_offset_t, vm_size_t, vm_offset_t *);
int vm_map_inherit (vm_map_t, vm_offset_t, vm_offset_t, vm_inherit_t);
void vm_map_init (struct vm_map *, vm_offset_t, vm_offset_t);
int vm_map_insert (vm_map_t, vm_object_t, vm_ooffset_t, vm_offset_t, vm_offset_t, vm_prot_t, vm_prot_t, int);
int vm_map_lookup (vm_map_t *, vm_offset_t, vm_prot_t, vm_map_entry_t *, vm_object_t *,
    vm_pindex_t *, vm_prot_t *, boolean_t *);
int vm_map_lookup_locked(vm_map_t *, vm_offset_t, vm_prot_t, vm_map_entry_t *, vm_object_t *,
    vm_pindex_t *, vm_prot_t *, boolean_t *);
void vm_map_lookup_done (vm_map_t, vm_map_entry_t);
boolean_t vm_map_lookup_entry (vm_map_t, vm_offset_t, vm_map_entry_t *);
void vm_map_pmap_enter(vm_map_t map, vm_offset_t addr, vm_prot_t prot,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size, int flags);
int vm_map_protect (vm_map_t, vm_offset_t, vm_offset_t, vm_prot_t, boolean_t);
int vm_map_remove (vm_map_t, vm_offset_t, vm_offset_t);
void vm_map_startup (void);
int vm_map_submap (vm_map_t, vm_offset_t, vm_offset_t, vm_map_t);
int vm_map_sync(vm_map_t, vm_offset_t, vm_offset_t, boolean_t, boolean_t);
int vm_map_madvise (vm_map_t, vm_offset_t, vm_offset_t, int);
void vm_map_simplify_entry (vm_map_t, vm_map_entry_t);
void vm_init2 (void);
int vm_map_stack (vm_map_t, vm_offset_t, vm_size_t, vm_prot_t, vm_prot_t, int);
int vm_map_growstack (struct proc *p, vm_offset_t addr);
int vm_map_unwire(vm_map_t map, vm_offset_t start, vm_offset_t end,
    int flags);
int vm_map_wire(vm_map_t map, vm_offset_t start, vm_offset_t end,
    int flags);
int vmspace_swap_count (struct vmspace *vmspace);
#endif				/* _KERNEL */
#endif				/* _VM_MAP_ */
