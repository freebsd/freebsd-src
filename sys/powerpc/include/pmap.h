/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: pmap.h,v 1.17 2000/03/30 16:18:24 jdolecek Exp $
 * $FreeBSD$
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/sr.h>

struct	pmap {
	u_int		pm_sr[16];
	u_int		pm_active;
	u_int		pm_context;
	u_int		pm_count;
	struct		pmap_statistics	pm_stats;
};

typedef	struct pmap *pmap_t;

struct pvo_entry {
	LIST_ENTRY(pvo_entry) pvo_vlink;	/* Link to common virt page */
	LIST_ENTRY(pvo_entry) pvo_olink;	/* Link to overflow entry */
	struct		pte pvo_pte;		/* PTE */
	pmap_t		pvo_pmap;		/* Owning pmap */
	vm_offset_t	pvo_vaddr;		/* VA of entry */
};
LIST_HEAD(pvo_head, pvo_entry);

struct	md_page {
	u_int	mdpg_attrs;
	struct	pvo_head mdpg_pvoh;
};

extern	struct pmap kernel_pmap_store;
#define	kernel_pmap	(&kernel_pmap_store)

#define	pmap_resident_count(pm)	(pm->pm_stats.resident_count)

#ifdef _KERNEL

void		pmap_bootstrap(vm_offset_t, vm_offset_t);
vm_offset_t	pmap_kextract(vm_offset_t);

int		pmap_pte_spill(vm_offset_t);

extern	vm_offset_t avail_start;
extern	vm_offset_t avail_end;
extern	vm_offset_t phys_avail[];
extern	vm_offset_t virtual_avail;
extern	vm_offset_t virtual_end;

extern	vm_offset_t msgbuf_phys;

#endif

#endif /* !_MACHINE_PMAP_H_ */
