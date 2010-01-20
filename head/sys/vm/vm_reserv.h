/*-
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007-2008 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Alan L. Cox,
 * Olivier Crameri, Peter Druschel, Sitaram Iyer, and Juan Navarro.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 *	Superpage reservation management definitions
 */

#ifndef	_VM_RESERV_H_
#define	_VM_RESERV_H_

#ifdef _KERNEL

#if VM_NRESERVLEVEL > 0

vm_page_t	vm_reserv_alloc_page(vm_object_t object, vm_pindex_t pindex);
void		vm_reserv_break_all(vm_object_t object);
boolean_t	vm_reserv_free_page(vm_page_t m);
void		vm_reserv_init(void);
int		vm_reserv_level_iffullpop(vm_page_t m);
boolean_t	vm_reserv_reactivate_page(vm_page_t m);
boolean_t	vm_reserv_reclaim_contig(vm_paddr_t size, vm_paddr_t low,
		    vm_paddr_t high, unsigned long alignment,
		    unsigned long boundary);
boolean_t	vm_reserv_reclaim_inactive(void);
void		vm_reserv_rename(vm_page_t m, vm_object_t new_object,
		    vm_object_t old_object, vm_pindex_t old_object_offset);
vm_paddr_t	vm_reserv_startup(vm_offset_t *vaddr, vm_paddr_t end,
		    vm_paddr_t high_water);

#endif	/* VM_NRESERVLEVEL > 0 */
#endif	/* _KERNEL */
#endif	/* !_VM_RESERV_H_ */
