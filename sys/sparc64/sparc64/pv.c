/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI: pmap.c,v 1.28.2.15 2000/04/27 03:10:31 cp Exp
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/asi.h>
#include <machine/frame.h>
#include <machine/pmap.h>
#include <machine/pv.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

/*
 * Physical address of array of physical addresses of stte alias chain heads,
 * and generation count of alias chains.
 */
vm_offset_t pv_table;
u_long pv_generation;

static void pv_local_remove_all(vm_offset_t pvh);

/*
 * Insert a mapped stte at the tail of an address alias chain.
 */
void
pv_insert(pmap_t pm, vm_offset_t pa, vm_offset_t va, struct stte *stp)
{
	vm_offset_t pstp;
	vm_offset_t pvh;

	pstp = tsb_stte_vtophys(pm, stp);
	pvh = pv_lookup(pa);
	PV_LOCK();
	if ((stp->st_next = pvh_get_first(pvh)) != 0)
		pv_set_prev(stp->st_next, pstp + ST_NEXT);
	pvh_set_first(pvh, pstp);
	stp->st_prev = pvh;
	pv_generation++;
	PV_UNLOCK();
}

/*
 * Remove a mapped tte from its address alias chain.
 */
void
pv_remove_virt(struct stte *stp)
{
	PV_LOCK();
	if (stp->st_next != 0)
		pv_set_prev(stp->st_next, stp->st_prev);
	stxp(stp->st_prev, stp->st_next);
	pv_generation++;
	PV_UNLOCK();
}

void
pv_bit_clear(vm_page_t m, u_long bits)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;
	vm_offset_t va;
	struct tte tte;

	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
#ifdef notyet
restart:
#endif
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		tte = pv_get_tte(pstp);
		KASSERT(TD_PA(tte.tte_data) == pa,
		    ("pv_bit_clear: corrupt alias chain"));
		if ((tte.tte_data & bits) == 0)
			continue;
		va = tte_get_va(tte);
		if (bits & (TD_W | TD_SW) && !pmap_track_modified(va))
			continue;	
		if (bits & (TD_W | TD_SW) && tte.tte_data & TD_W)
			vm_page_dirty(m);
		pv_atomic_bit_clear(pstp, bits);
#ifdef notyet
		generation = pv_generation;
		PV_UNLOCK();
		/* XXX pass function and parameter to ipi call */
		ipi_all(IPI_TLB_PAGE_DEMAP);
		PV_LOCK();
		if (generation != pv_generation)
			goto restart;
#else
		tlb_page_demap(TLB_DTLB, TT_GET_CTX(tte.tte_tag), va);
#endif
	}
	PV_UNLOCK();
}

int
pv_bit_count(vm_page_t m, u_long bits)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;
	vm_offset_t va;
	struct tte tte;
	int count;

	count = 0;
	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
#ifdef notyet
restart:
#endif
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		tte = pv_get_tte(pstp);
		va = tte_get_va(tte);
		KASSERT(TD_PA(tte.tte_data) == pa,
		    ("pv_bit_count: corrupt alias chain"));
		if (tte.tte_data & bits)
			count++;
		pv_atomic_bit_clear(pstp, bits);
#ifdef notyet
		generation = pv_generation;
		PV_UNLOCK();
		ipi_all(IPI_TLB_PAGE_DEMAP);
		PV_LOCK();
		if (generation != pv_generation)
			goto restart;
#else
		tlb_page_demap(TLB_DTLB, TT_GET_CTX(tte.tte_tag), va);
#endif
	}
	PV_UNLOCK();
	return (count);
}

void
pv_bit_set(vm_page_t m, u_long bits)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;
	vm_offset_t va;
	struct tte tte;

	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
#ifdef notyet
restart:
#endif
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		tte = pv_get_tte(pstp);
		va = tte_get_va(tte);
		KASSERT(TD_PA(tte.tte_data) == pa,
		    ("pv_bit_set: corrupt alias chain"));
		if (tte.tte_data & bits)
			continue;
		pv_atomic_bit_set(pstp, bits);
#ifdef notyet
		generation = pv_generation;
		PV_UNLOCK();
		/* XXX pass function and parameter to ipi call */
		ipi_all(IPI_TLB_PAGE_DEMAP);
		PV_LOCK();
		if (generation != pv_generation)
			goto restart;
#else
		tlb_page_demap(TLB_DTLB, TT_GET_CTX(tte.tte_tag), va);
#endif
	}
	PV_UNLOCK();
}

int
pv_bit_test(vm_page_t m, u_long bits)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;

	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		if (pv_atomic_bit_test(pstp, bits)) {
			PV_UNLOCK();
			return (1);
		}
	}
	PV_UNLOCK();
	return (0);
}

void
pv_global_remove_all(vm_page_t m)
{
	vm_offset_t pstp;
	vm_offset_t pvh;
	vm_offset_t pa;

	pa = VM_PAGE_TO_PHYS(m);
	pvh = pv_lookup(pa);
	PV_LOCK();
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp))
		pv_atomic_bit_clear(pstp, TD_V);
	PV_UNLOCK();
	pv_local_remove_all(pvh);
	PV_LOCK();
	while ((pstp = pvh_get_first(pvh)) != 0)
		pv_remove_phys(pstp);
	PV_UNLOCK();
}

static void
pv_local_remove_all(vm_offset_t pvh)
{
	vm_offset_t pstp;
	struct tte tte;

	PV_LOCK();
	for (pstp = pvh_get_first(pvh); pstp != 0; pstp = pv_get_next(pstp)) {
		tte = pv_get_tte(pstp);
		tsb_tte_local_remove(&tte);
	}
	PV_UNLOCK();
}
