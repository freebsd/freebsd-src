/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_zone.h>

#include <machine/asi.h>
#include <machine/frame.h>
#include <machine/pmap.h>
#include <machine/pv.h>
#include <machine/smp.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

vm_zone_t pvzone;
struct vm_zone pvzone_store;
struct vm_object pvzone_obj;
int pv_entry_count;
int pv_entry_max;
int pv_entry_high_water;
struct pv_entry *pvinit;

pv_entry_t
pv_alloc(void)
{

	pv_entry_count++;
	if (pv_entry_high_water && (pv_entry_count > pv_entry_high_water) &&
	    (pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup(&vm_pages_needed);
	}
	return (zalloc(pvzone));
}

void
pv_free(pv_entry_t pv)
{

	pv_entry_count--;
	zfree(pvzone, pv);
}

/*
 * Insert a mapped stte at the tail of an address alias chain.
 */
void
pv_insert(pmap_t pm, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pv_alloc();
	pv->pv_va = va;
	pv->pv_m = m;
	pv->pv_pmap = pm;
	TAILQ_INSERT_TAIL(&pm->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;
	pm->pm_stats.resident_count++;
}

pv_entry_t
pv_lookup(pmap_t pm, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;

	if (m != NULL && m->md.pv_list_count < pm->pm_stats.resident_count) {
		TAILQ_FOREACH(pv, &m->md.pv_list, pv_list)
			if (pm == pv->pv_pmap && va == pv->pv_va)
				break;
	} else {
		TAILQ_FOREACH(pv, &pm->pm_pvlist, pv_plist)
			if (va == pv->pv_va)
				break;
	}
	return (pv);
}

/*
 * Remove a mapped tte from its address alias chain.
 */
void
pv_remove(pmap_t pm, vm_page_t m, vm_offset_t va)
{
	pv_entry_t pv;

	if ((pv = pv_lookup(pm, m, va)) != NULL) {
		m->md.pv_list_count--;
		pm->pm_stats.resident_count--;
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		TAILQ_REMOVE(&pm->pm_pvlist, pv, pv_plist);
		if (TAILQ_EMPTY(&m->md.pv_list))
			vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
		pv_free(pv);
	}
}

void
pv_bit_clear(vm_page_t m, u_long bits)
{
	struct tte *tp;
	pv_entry_t pv;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		KASSERT(pv->pv_pmap != NULL, ("pv_bit_clear: null pmap"));
		if ((tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va)) != NULL &&
		    (tp->tte_data & bits) != 0) {
			if ((bits & TD_SW) != 0 &&
			    pmap_track_modified(pv->pv_pmap, pv->pv_va)) {
				if (tp->tte_data & TD_W)
					vm_page_dirty(m);
			}
			atomic_clear_long(&tp->tte_data, bits);
			tlb_tte_demap(*tp, pv->pv_pmap);
		}
	}
}

int
pv_bit_count(vm_page_t m, u_long bits)
{
	struct tte *tp;
	pv_entry_t pvf;
	pv_entry_t pvn;
	pv_entry_t pv;
	int count;

	count = 0;
	if ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pvf = pv;
		do {
			pvn = TAILQ_NEXT(pv, pv_list);
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
			if (!pmap_track_modified(pv->pv_pmap, pv->pv_va))
				continue;
			tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va);
			if (tp != NULL) {
				if ((tp->tte_data & bits) != 0) {
					atomic_clear_long(&tp->tte_data, bits);
					if (++count > 4)
						break;
				}
			}
		} while ((pv = pvn) != NULL && pv != pvf);
	}
	return (count);
}

int
pv_bit_test(vm_page_t m, u_long bits)
{
	struct tte *tp;
	pv_entry_t pv;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (bits & (TD_REF | TD_W)) {
			if (!pmap_track_modified(pv->pv_pmap, pv->pv_va))
				continue;
		}
		KASSERT(pv->pv_pmap != NULL, ("pv_bit_test: null pmap"));
		if ((tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va)) != NULL) {
			if (atomic_load_long(&tp->tte_data) & bits) {
				return (TRUE);
			}
		}
	}
	return (FALSE);
}

/*
 * See pmap_page_exists_quick for operational explanation of
 * pv_page_exists.
 */

int
pv_page_exists(pmap_t pm, vm_page_t m)
{
	pv_entry_t pv;
	int loops = 0;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pm) {
			return (TRUE);
		}
		loops++;
		if (loops >= 16)
			break;
	}
	return (FALSE);
}

void
pv_remove_all(vm_page_t m)
{
	struct tte *tp;
	pv_entry_t pv;
	u_long data;

	KASSERT((m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0,
	   ("pv_remove_all: illegal for unmanaged page %#lx",
	   VM_PAGE_TO_PHYS(m)));
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		tp = tsb_tte_lookup(pv->pv_pmap, pv->pv_va);
		KASSERT(tp != NULL, ("pv_remove_all: mapping lost"));
		data = atomic_load_long(&tp->tte_data);
		if ((data & TD_WIRED) != 0)
			pv->pv_pmap->pm_stats.wired_count--;
		if ((data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if ((data & TD_W) != 0) {
			if (pmap_track_modified(pv->pv_pmap, pv->pv_va))
				vm_page_dirty(m);
		}
		atomic_clear_long(&tp->tte_data, TD_V);
		tlb_tte_demap(*tp, pv->pv_pmap);
		tp->tte_vpn = 0;
		tp->tte_data = 0;
		pv->pv_pmap->pm_stats.resident_count--;
		m->md.pv_list_count--;
		TAILQ_REMOVE(&pv->pv_pmap->pm_pvlist, pv, pv_plist);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		pmap_cache_remove(pv->pv_m, pv->pv_va);
		pv_free(pv);
	}
	KASSERT(m->md.pv_list_count == 0,
	    ("pv_remove_all: leaking pv entries 0 != %d", m->md.pv_list_count));
	KASSERT(TAILQ_EMPTY(&m->md.pv_list),
	    ("pv_remove_all: leaking pv entries"));
	vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
}
