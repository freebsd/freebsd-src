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
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>

#include <machine/asi.h>
#include <machine/frame.h>
#include <machine/pmap.h>
#include <machine/pv.h>
#include <machine/smp.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

/*
 * Insert a mapped stte at the tail of an address alias chain.
 */
void
pv_insert(pmap_t pm, vm_page_t m, struct tte *tp)
{

	STAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
	tp->tte_pmap = pm;
	pm->pm_stats.resident_count++;
}

/*
 * Remove a mapped tte from its address alias chain.
 */
void
pv_remove(pmap_t pm, vm_page_t m, struct tte *tp)
{

	STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
	if (STAILQ_EMPTY(&m->md.tte_list))
		vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
	tp->tte_pmap->pm_stats.resident_count--;
}

void
pv_bit_clear(vm_page_t m, u_long bits)
{
	struct tte *tp;

	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if ((tp->tte_data & bits) != 0) {
			if ((bits & TD_SW) != 0 &&
			    pmap_track_modified(TTE_GET_PMAP(tp),
			     TTE_GET_VA(tp))) {
				if (tp->tte_data & TD_W)
					vm_page_dirty(m);
			}
			tp->tte_data &= ~bits;
			tlb_tte_demap(tp, TTE_GET_PMAP(tp));
		}
	}
}

int
pv_bit_count(vm_page_t m, u_long bits)
{
	struct tte *tpf;
	struct tte *tpn;
	struct tte *tp;
	int count;

	count = 0;
	if ((tp = STAILQ_FIRST(&m->md.tte_list)) != NULL) {
		tpf = tp;
		do {
			tpn = STAILQ_NEXT(tp, tte_link);
			STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
			STAILQ_INSERT_TAIL(&m->md.tte_list, tp, tte_link);
			if (!pmap_track_modified(TTE_GET_PMAP(tp),
			    TTE_GET_VA(tp)))
				continue;
			if ((tp->tte_data & bits) != 0) {
				tp->tte_data &= ~bits;
				if (++count > 4)
					break;
			}
		} while ((tp = tpn) != NULL && tp != tpf);
	}
	return (count);
}

int
pv_bit_test(vm_page_t m, u_long bits)
{
	struct tte *tp;

	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if (bits & (TD_REF | TD_W)) {
			if (!pmap_track_modified(TTE_GET_PMAP(tp),
			    TTE_GET_VA(tp)))
				continue;
		}
		if (tp->tte_data & bits)
			return (TRUE);
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
	struct tte *tp;
	int loops;

	loops = 0;
	STAILQ_FOREACH(tp, &m->md.tte_list, tte_link) {
		if (TTE_GET_PMAP(tp) == pm)
			return (TRUE);
		if (++loops >= 16)
			break;
	}
	return (FALSE);
}

void
pv_remove_all(vm_page_t m)
{
	struct pmap *pm;
	struct tte *tp;
	vm_offset_t va;

	KASSERT((m->flags & (PG_FICTITIOUS|PG_UNMANAGED)) == 0,
	   ("pv_remove_all: illegal for unmanaged page %#lx",
	   VM_PAGE_TO_PHYS(m)));
	while ((tp = STAILQ_FIRST(&m->md.tte_list)) != NULL) {
		pm = TTE_GET_PMAP(tp);
		va = TTE_GET_VA(tp);
		if ((tp->tte_data & TD_WIRED) != 0)
			pm->pm_stats.wired_count--;
		if ((tp->tte_data & TD_REF) != 0)
			vm_page_flag_set(m, PG_REFERENCED);
		if ((tp->tte_data & TD_W) != 0) {
			if (pmap_track_modified(pm, va))
				vm_page_dirty(m);
		}
		tp->tte_data &= ~TD_V;
		tlb_page_demap(TLB_DTLB | TLB_ITLB, pm, va);
		STAILQ_REMOVE(&m->md.tte_list, tp, tte, tte_link);
		pm->pm_stats.resident_count--;
		pmap_cache_remove(m, va);
		TTE_ZERO(tp);
	}
	KASSERT(STAILQ_EMPTY(&m->md.tte_list),
	    ("pv_remove_all: leaking pv entries"));
	vm_page_flag_clear(m, PG_MAPPED | PG_WRITEABLE);
}
