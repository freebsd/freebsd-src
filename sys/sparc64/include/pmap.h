/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/tte.h>

#define	PMAP_CONTEXT_MAX	8192

#define	pmap_resident_count(pm)	(pm->pm_stats.resident_count)

typedef	struct pmap *pmap_t;

struct	md_page {
};

struct	pmap {
	struct	stte pm_stte;
	vm_object_t pm_object;
	u_int	pm_active;
	u_int	pm_context;
	u_int	pm_count;
	u_int	pm_pages;
	struct	pmap_statistics pm_stats;
};

void	pmap_bootstrap(vm_offset_t skpa, vm_offset_t ekva);
vm_offset_t pmap_kextract(vm_offset_t va);

extern	vm_offset_t avail_start;
extern	vm_offset_t avail_end;
extern	vm_offset_t clean_eva;
extern	vm_offset_t clean_sva;
extern	struct pmap *kernel_pmap;
extern	vm_offset_t phys_avail[];
extern	vm_offset_t virtual_avail;
extern	vm_offset_t virtual_end;

static __inline int
pmap_track_modified(vm_offset_t va)
{
	return ((va < clean_sva) || (va >= clean_eva));
}

#endif /* !_MACHINE_PMAP_H_ */
