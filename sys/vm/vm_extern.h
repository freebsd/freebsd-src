/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vm_extern.h	8.2 (Berkeley) 1/12/94
 * $Id: vm_extern.h,v 1.27 1996/09/15 11:24:21 bde Exp $
 */

#ifndef _VM_EXTERN_H_
#define	_VM_EXTERN_H_

struct buf;
struct proc;
struct vmspace;
struct vmtotal;
struct mount;
struct vnode;

#ifdef KERNEL

#ifdef TYPEDEF_FOR_UAP
int getpagesize __P((struct proc * p, void *, int *));
int madvise __P((struct proc *, void *, int *));
int mincore __P((struct proc *, void *, int *));
int mprotect __P((struct proc *, void *, int *));
int msync __P((struct proc *, void *, int *));
int munmap __P((struct proc *, void *, int *));
int obreak __P((struct proc *, void *, int *));
int sbrk __P((struct proc *, void *, int *));
int smmap __P((struct proc *, void *, int *));
int sstk __P((struct proc *, void *, int *));
int swapon __P((struct proc *, void *, int *));
#endif

void faultin __P((struct proc *p));
int grow __P((struct proc *, u_int));
int kernacc __P((caddr_t, int, int));
vm_offset_t kmem_alloc __P((vm_map_t, vm_size_t));
vm_offset_t kmem_alloc_pageable __P((vm_map_t, vm_size_t));
vm_offset_t kmem_alloc_wait __P((vm_map_t, vm_size_t));
void kmem_free __P((vm_map_t, vm_offset_t, vm_size_t));
void kmem_free_wakeup __P((vm_map_t, vm_offset_t, vm_size_t));
void kmem_init __P((vm_offset_t, vm_offset_t));
vm_offset_t kmem_malloc __P((vm_map_t, vm_size_t, boolean_t));
vm_map_t kmem_suballoc __P((vm_map_t, vm_offset_t *, vm_offset_t *, vm_size_t, boolean_t));
void munmapfd __P((struct proc *, int));
int pager_cache __P((vm_object_t, boolean_t));
int swaponvp __P((struct proc *, struct vnode *, dev_t , u_long));
void swapout_procs __P((void));
int useracc __P((caddr_t, int, int));
int vm_fault __P((vm_map_t, vm_offset_t, vm_prot_t, boolean_t));
void vm_fault_copy_entry __P((vm_map_t, vm_map_t, vm_map_entry_t, vm_map_entry_t));
void vm_fault_unwire __P((vm_map_t, vm_offset_t, vm_offset_t));
int vm_fault_wire __P((vm_map_t, vm_offset_t, vm_offset_t));
int vm_fault_user_wire __P((vm_map_t, vm_offset_t, vm_offset_t));
int vm_fork __P((struct proc *, struct proc *));
int vm_mmap __P((vm_map_t, vm_offset_t *, vm_size_t, vm_prot_t, vm_prot_t, int, caddr_t, vm_ooffset_t));
vm_offset_t vm_page_alloc_contig __P((vm_offset_t, vm_offset_t, vm_offset_t, vm_offset_t));
void vm_set_page_size __P((void));
void vmmeter __P((void));
struct vmspace *vmspace_alloc __P((vm_offset_t, vm_offset_t, int));
struct vmspace *vmspace_fork __P((struct vmspace *));
void vmspace_free __P((struct vmspace *));
void vnode_pager_setsize __P((struct vnode *, vm_ooffset_t));
void vnode_pager_umount __P((struct mount *));
void vnode_pager_uncache __P((struct vnode *));
void vslock __P((caddr_t, u_int));
void vsunlock __P((caddr_t, u_int, int));
void vm_object_print __P((/* db_expr_t */ int, boolean_t, /* db_expr_t */ int,
			  char *));
void vm_fault_quick __P((caddr_t v, int prot));

#endif				/* KERNEL */

#endif				/* !_VM_EXTERN_H_ */
