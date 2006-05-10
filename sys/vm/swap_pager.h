/*-
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
 *	from: @(#)swap_pager.h	7.1 (Berkeley) 12/5/90
 * $FreeBSD$
 */

#ifndef	_VM_SWAP_PAGER_H_
#define	_VM_SWAP_PAGER_H_ 1

#ifdef _KERNEL

extern int swap_pager_full;
extern int swap_pager_avail;

struct swdevt;
void swap_pager_copy(vm_object_t, vm_object_t, vm_pindex_t, int);
void swap_pager_freespace(vm_object_t, vm_pindex_t, vm_size_t);
void swap_pager_swap_init(void);
int swap_pager_isswapped(vm_object_t, struct swdevt *);
int swap_pager_reserve(vm_object_t, vm_pindex_t, vm_size_t);
void swap_pager_status(int *total, int *used);
void swapoff_all(void);

#endif				/* _KERNEL */
#endif				/* _VM_SWAP_PAGER_H_ */
