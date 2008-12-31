/*-
 * Copyright (c) 2006 Kip Macy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/tte.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ddb.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h> 
#include <vm/vm_page.h>

#include <machine/cpufunc.h>
#include <machine/smp.h>
#include <machine/mmu.h>
#include <machine/tte.h>
#include <machine/cpu.h>
#include <machine/tte_hash.h>

void 
tte_clear_phys_bit(vm_page_t m, uint64_t flags)
{
	pv_entry_t pv;
	tte_t active_flags = (flags & ~VTD_SW_W);

	if (m->flags & PG_FICTITIOUS) 
		return;

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	/*
	 * Loop over all current mappings setting/clearing as appropos If
	 * setting RO do we need to clear the VAC?
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		tte_t otte_data, matchbits;
		pmap_t pmap;

		pmap = pv->pv_pmap;
		PMAP_LOCK(pmap);
		otte_data = tte_hash_clear_bits(pmap->pm_hash, pv->pv_va, flags);
		if ((matchbits = (otte_data & active_flags)) != 0) {
			if ((otte_data & (VTD_SW_W|VTD_W)) == (VTD_SW_W|VTD_W)) 
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va, TRUE);
		}
		PMAP_UNLOCK(pmap);
	}
}

boolean_t 
tte_get_phys_bit(vm_page_t m, uint64_t flags)
{

	pv_entry_t pv;
	pmap_t pmap;
	boolean_t rv;
	
	rv = FALSE;
	if (m->flags & PG_FICTITIOUS)
		return (rv);

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		tte_t otte_data;

		pmap = pv->pv_pmap;
		otte_data = tte_hash_lookup(pmap->pm_hash, pv->pv_va);
		if ((rv = ((otte_data & flags) != 0)))
			break;
	}

	return (rv);
}

void 
tte_clear_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags)
{
	(void)tte_hash_clear_bits(pmap->pm_hash, va, flags);
}

void 
tte_set_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags)
{
	UNIMPLEMENTED;
}

boolean_t 
tte_get_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags)
{
	tte_t tte_data;
	
	tte_data = tte_hash_lookup(pmap->pm_hash, va);
	
	return ((tte_data & flags) == flags);
}
