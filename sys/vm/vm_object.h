/* 
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vm_object.h	7.3 (Berkeley) 4/21/91
 *	$Id: vm_object.h,v 1.7 1994/03/14 21:54:27 davidg Exp $
 */

/*
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
 */

/*
 *	Virtual memory object module definitions.
 */

#ifndef	_VM_OBJECT_
#define	_VM_OBJECT_

#include <vm/vm_pager.h>

/*
 *	Types defined:
 *
 *	vm_object_t		Virtual memory object.
 */

struct vm_object {
	queue_chain_t		memq;		/* Resident memory */
	queue_chain_t		object_list;	/* list of all objects */
	simple_lock_data_t	Lock;		/* Synchronization */
	int			LockHolder;
	int			ref_count;	/* How many refs?? */
	vm_size_t		size;		/* Object size */
	int			resident_page_count;
						/* number of resident pages */
	struct vm_object	*copy;		/* Object that holds copies of
						   my changed pages */
	vm_pager_t		pager;		/* Where to get data */
	boolean_t		pager_ready;	/* Have pager fields been filled? */
	vm_offset_t		paging_offset;	/* Offset into paging space */
	struct vm_object	*shadow;	/* My shadow */
	vm_offset_t		shadow_offset;	/* Offset in shadow */
	unsigned int
				paging_in_progress:16,
						/* Paging (in or out) - don't
						   collapse or destroy */
	/* boolean_t */		can_persist:1,	/* allow to persist */
	/* boolean_t */		internal:1,	/* internally created object */
				read_only:1;	/* entire obj is read only */
	queue_chain_t		cached_list;	/* for persistence */
};

typedef struct vm_object	*vm_object_t;

struct vm_object_hash_entry {
	queue_chain_t		hash_links;	/* hash chain links */
	vm_object_t		object;		/* object we represent */
};

typedef struct vm_object_hash_entry	*vm_object_hash_entry_t;

#ifdef	KERNEL
extern queue_head_t	vm_object_cached_list; /* list of objects persisting */
extern int		vm_object_cached; /* size of cached list */
extern simple_lock_data_t	vm_cache_lock;	/* lock for object cache */

extern queue_head_t	vm_object_list;		/* list of allocated objects */
extern long		vm_object_count;	/* count of all objects */
extern simple_lock_data_t	vm_object_list_lock;
					/* lock for object list and count */

extern vm_object_t	kernel_object;		/* the single kernel object */
extern vm_object_t	kmem_object;

#define	vm_object_cache_lock()		simple_lock(&vm_cache_lock)
#define	vm_object_cache_unlock()	simple_unlock(&vm_cache_lock)
#endif /* KERNEL */

/*
 *	Declare procedures that operate on VM objects.
 */

void		vm_object_init ();
void		vm_object_terminate();
vm_object_t	vm_object_allocate();
void		vm_object_reference();
void		vm_object_deallocate();
void		vm_object_pmap_copy();
void		vm_object_pmap_remove();
void		vm_object_page_remove();
void		vm_object_shadow();
void		vm_object_copy();
void		vm_object_collapse();
vm_object_t	vm_object_lookup();
void		vm_object_enter();
void		vm_object_setpager();
#define		vm_object_cache(pager)		pager_cache(vm_object_lookup(pager),TRUE)
#define		vm_object_uncache(pager)	pager_cache(vm_object_lookup(pager),FALSE)

void		vm_object_cache_clear();
void		vm_object_print();

#if	VM_OBJECT_DEBUG
#define	vm_object_lock_init(object)	{ simple_lock_init(&(object)->Lock); (object)->LockHolder = 0; }
#define	vm_object_lock(object)		{ simple_lock(&(object)->Lock); (object)->LockHolder = (int) current_thread(); }
#define	vm_object_unlock(object)	{ (object)->LockHolder = 0; simple_unlock(&(object)->Lock); }
#define	vm_object_lock_try(object)	(simple_lock_try(&(object)->Lock) ? (((object)->LockHolder = (int) current_thread()) , TRUE) : FALSE)
#define	vm_object_sleep(event, object, interruptible) \
					{ (object)->LockHolder = 0; thread_sleep((int)(event), &(object)->Lock, (interruptible)); }
#else  /* VM_OBJECT_DEBUG */
#define	vm_object_lock_init(object)	simple_lock_init(&(object)->Lock)
#define	vm_object_lock(object)		simple_lock(&(object)->Lock)
#define	vm_object_unlock(object)	simple_unlock(&(object)->Lock)
#define	vm_object_lock_try(object)	simple_lock_try(&(object)->Lock)
#define	vm_object_sleep(event, object, interruptible) \
					thread_sleep((int)(event), &(object)->Lock, (interruptible))
#endif /* VM_OBJECT_DEBUG */

extern void vm_object_page_clean(vm_object_t, vm_offset_t, vm_offset_t);
extern int pager_cache(vm_object_t, boolean_t);

#endif /* _VM_OBJECT_ */
