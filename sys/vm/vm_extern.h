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
 * $FreeBSD: src/sys/vm/vm_extern.h,v 1.61 2003/03/25 00:07:05 jake Exp $
 */

#ifndef _VM_EXTERN_H_
#define	_VM_EXTERN_H_

struct buf;
struct proc;
struct vmspace;
struct vmtotal;
struct mount;
struct vnode;

#ifdef _KERNEL

#ifdef TYPEDEF_FOR_UAP
int getpagesize(struct thread *, void *, int *);
int madvise(struct thread *, void *, int *);
int mincore(struct thread *, void *, int *);
int mprotect(struct thread *, void *, int *);
int msync(struct thread *, void *, int *);
int munmap(struct thread *, void *, int *);
int obreak(struct thread *, void *, int *);
int sbrk(struct thread *, void *, int *);
int sstk(struct thread *, void *, int *);
int swapon(struct thread *, void *, int *);
#endif			/* TYPEDEF_FOR_UAP */

int kernacc(void *, int, int);
vm_offset_t kmem_alloc(vm_map_t, vm_size_t);
vm_offset_t kmem_alloc_nofault(vm_map_t, vm_size_t);
vm_offset_t kmem_alloc_pageable(vm_map_t, vm_size_t);
vm_offset_t kmem_alloc_wait(vm_map_t, vm_size_t);
void kmem_free(vm_map_t, vm_offset_t, vm_size_t);
void kmem_free_wakeup(vm_map_t, vm_offset_t, vm_size_t);
void kmem_init(vm_offset_t, vm_offset_t);
vm_offset_t kmem_malloc(vm_map_t, vm_size_t, boolean_t);
vm_map_t kmem_suballoc(vm_map_t, vm_offset_t *, vm_offset_t *, vm_size_t);
void munmapfd(struct thread *, int);
int swaponvp(struct thread *, struct vnode *, dev_t , u_long);
void swapout_procs(int);
int useracc(void *, int, int);
int vm_fault(vm_map_t, vm_offset_t, vm_prot_t, int);
void vm_fault_copy_entry(vm_map_t, vm_map_t, vm_map_entry_t, vm_map_entry_t);
void vm_fault_unwire(vm_map_t, vm_offset_t, vm_offset_t);
int vm_fault_wire(vm_map_t, vm_offset_t, vm_offset_t, boolean_t);
void vm_forkproc(struct thread *, struct proc *, struct thread *, int);
void vm_waitproc(struct proc *);
int vm_mmap(vm_map_t, vm_offset_t *, vm_size_t, vm_prot_t, vm_prot_t, int, void *, vm_ooffset_t);
vm_offset_t vm_page_alloc_contig(vm_offset_t, vm_paddr_t, vm_paddr_t, vm_offset_t);
void vm_set_page_size(void);
struct vmspace *vmspace_alloc(vm_offset_t, vm_offset_t);
struct vmspace *vmspace_fork(struct vmspace *);
void vmspace_exec(struct proc *, vm_offset_t, vm_offset_t);
void vmspace_unshare(struct proc *);
void vmspace_free(struct vmspace *);
void vmspace_exitfree(struct proc *);
void vnode_pager_setsize(struct vnode *, vm_ooffset_t);
void vslock(void *, u_int);
void vsunlock(void *, u_int);
void vm_object_print(/* db_expr_t */ long, boolean_t, /* db_expr_t */ long,
			  char *);
int vm_fault_quick(caddr_t v, int prot);
void vm_proc_new(struct proc *p);
void vm_proc_dispose(struct proc *p);
#endif				/* _KERNEL */
#endif				/* !_VM_EXTERN_H_ */
