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
 *	from: @(#)pmap.h	8.1 (Berkeley) 6/11/93
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Avadis Tevanian, Jr.
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
 * $FreeBSD: src/sys/vm/pmap.h,v 1.57 2003/04/10 18:42:06 jhb Exp $
 */

/*
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 */

#ifndef	_PMAP_VM_
#define	_PMAP_VM_
/*
 * Each machine dependent implementation is expected to
 * keep certain statistics.  They may do this anyway they
 * so choose, but are expected to return the statistics
 * in the following structure.
 */
struct pmap_statistics {
	long resident_count;	/* # of pages mapped (total) */
	long wired_count;	/* # of pages wired */
};
typedef struct pmap_statistics *pmap_statistics_t;

#include <machine/pmap.h>

#ifdef _KERNEL
struct proc;
struct thread;

/*
 * Updates to kernel_vm_end are synchronized by the kernel_map's system mutex.
 */
extern vm_offset_t kernel_vm_end;

extern	int	 pmap_pagedaemon_waken;

void		 pmap_change_wiring(pmap_t, vm_offset_t, boolean_t);
void		 pmap_clear_modify(vm_page_t m);
void		 pmap_clear_reference(vm_page_t m);
void		 pmap_copy(pmap_t, pmap_t, vm_offset_t, vm_size_t, vm_offset_t);
void		 pmap_copy_page(vm_page_t, vm_page_t);
void		 pmap_enter(pmap_t, vm_offset_t, vm_page_t, vm_prot_t,
		    boolean_t);
vm_paddr_t	 pmap_extract(pmap_t pmap, vm_offset_t va);
void		 pmap_growkernel(vm_offset_t);
void		 pmap_init(vm_paddr_t, vm_paddr_t);
boolean_t	 pmap_is_modified(vm_page_t m);
boolean_t	 pmap_ts_referenced(vm_page_t m);
vm_offset_t	 pmap_map(vm_offset_t *, vm_paddr_t, vm_paddr_t, int);
void		 pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex, vm_offset_t size,
		    int pagelimit);
boolean_t	 pmap_page_exists_quick(pmap_t pmap, vm_page_t m);
void		 pmap_page_protect(vm_page_t m, vm_prot_t prot);
void		 pmap_pinit(pmap_t);
void		 pmap_pinit0(pmap_t);
void		 pmap_pinit2(pmap_t);
void		 pmap_protect(pmap_t, vm_offset_t, vm_offset_t, vm_prot_t);
void		 pmap_qenter(vm_offset_t, vm_page_t *, int);
void		 pmap_qremove(vm_offset_t, int);
void		 pmap_release(pmap_t);
void		 pmap_remove(pmap_t, vm_offset_t, vm_offset_t);
void		 pmap_remove_all(vm_page_t m);
void		 pmap_remove_pages(pmap_t, vm_offset_t, vm_offset_t);
void		 pmap_zero_page(vm_page_t);
void		 pmap_zero_page_area(vm_page_t, int off, int size);
void		 pmap_zero_page_idle(vm_page_t);
void		 pmap_prefault(pmap_t, vm_offset_t, vm_map_entry_t);
int		 pmap_mincore(pmap_t pmap, vm_offset_t addr);
void		 pmap_new_thread(struct thread *td, int pages);
void		 pmap_dispose_thread(struct thread *td);
void		 pmap_new_altkstack(struct thread *td, int pages);
void		 pmap_dispose_altkstack(struct thread *td);
void		 pmap_swapout_thread(struct thread *td);
void		 pmap_swapin_thread(struct thread *td);
void		 pmap_activate(struct thread *td);
vm_offset_t	 pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size);
void		*pmap_kenter_temporary(vm_offset_t pa, int i);
void		 pmap_init2(void);
#endif /* _KERNEL */
#endif /* _PMAP_VM_ */
