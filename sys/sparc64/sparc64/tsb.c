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

#include "opt_ddb.h"
#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/pv.h>
#include <machine/tlb.h>
#include <machine/tsb.h>
#include <machine/tte.h>

#define	KTR_TSB		KTR_CT3

CTASSERT((1 << TTE_SHIFT) == sizeof(struct tte));

#ifdef PMAP_STATS
static long tsb_nrepl;
static long tsb_nlookup_k;
static long tsb_nlookup_u;
static long tsb_nenter_k;
static long tsb_nenter_u;
static long tsb_nforeach;

SYSCTL_DECL(_debug_pmap_stats);
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nrepl, CTLFLAG_RD, &tsb_nrepl, 0,
    "Number of TSB replacements");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nlookup_k, CTLFLAG_RD,
    &tsb_nlookup_k, 0, "Number of calls to tsb_tte_lookup(), kernel pmap");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nlookup_u, CTLFLAG_RD,
    &tsb_nlookup_u, 0, "Number of calls to tsb_tte_lookup(), user pmap");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nenter_k, CTLFLAG_RD,
    &tsb_nenter_k, 0, "Number of calls to tsb_tte_enter()");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nenter_u, CTLFLAG_RD,
    &tsb_nenter_u, 0, "Number of calls to tsb_tte_enter()");
SYSCTL_LONG(_debug_pmap_stats, OID_AUTO, tsb_nforeach, CTLFLAG_RD,
    &tsb_nforeach, 0, "Number of calls to tsb_foreach()");

#define	TSB_STATS_INC(var)	atomic_add_long(&var, 1)
#else
#define	TSB_STATS_INC(var)
#endif

struct tte *tsb_kernel;
vm_offset_t tsb_kernel_phys;

struct tte *
tsb_tte_lookup(pmap_t pm, vm_offset_t va)
{
	struct tte *bucket;
	struct tte *tp;
	u_int i;

	if (pm == kernel_pmap) {
		TSB_STATS_INC(tsb_nlookup_k);
		tp = tsb_kvtotte(va);
		CTR3(KTR_TSB,
		    "tsb_tte_lookup: kernel va=%#lx tp=%#lx data=%#lx",
		    va, tp, tp->tte_data);
		if (tte_match(*tp, va)) {
			CTR1(KTR_TSB, "tsb_tte_lookup: match va=%#lx", va);
			return (tp);
		}
	} else {
		TSB_STATS_INC(tsb_nlookup_u);
		va = trunc_page(va);
		bucket = tsb_vtobucket(pm, va);
		CTR3(KTR_TSB, "tsb_tte_lookup: ctx=%#lx va=%#lx bucket=%p",
		    pm->pm_context, va, bucket);
		for (i = 0; i < TSB_BUCKET_SIZE; i++) {
			if (tte_match(bucket[i], va)) {
				tp = &bucket[i];
				CTR2(KTR_TSB,
				    "tsb_tte_lookup: match va=%#lx tp=%p",
				    va, tp);
				return (tp);
			}
		}
	}
	CTR2(KTR_TSB, "tsb_tte_lookup: miss ctx=%#lx va=%#lx",
	    pm->pm_context, va);
	return (NULL);
}

struct tte *
tsb_tte_enter(pmap_t pm, vm_page_t m, vm_offset_t va, struct tte tte)
{
	struct tte *bucket;
	struct tte *rtp;
	struct tte *tp;
	vm_page_t om;
	vm_offset_t ova;
	int b0;
	int i;

	if (pm == kernel_pmap) {
		TSB_STATS_INC(tsb_nenter_k);
		tp = tsb_kvtotte(va);
		*tp = tte;
		return (tp);
	}

	TSB_STATS_INC(tsb_nenter_u);
	bucket = tsb_vtobucket(pm, va);
	CTR4(KTR_TSB, "tsb_tte_enter: ctx=%#lx va=%#lx data=%#lx bucket=%p",
	    pm->pm_context, va, tte.tte_data, bucket);

	tp = NULL;
	rtp = NULL;
	b0 = rd(tick) & (TSB_BUCKET_SIZE - 1);
	i = b0;
	do {
		if ((bucket[i].tte_data & TD_V) == 0) {
			tp = &bucket[i];
			break;
		}
		if (tp == NULL) {
			if ((bucket[i].tte_data & TD_REF) == 0)
				tp = &bucket[i];
			else if (rtp == NULL)
				rtp = &bucket[i];
		}
	} while ((i = (i + 1) & (TSB_BUCKET_SIZE - 1)) != b0);

	if (tp == NULL)
		tp = rtp;
	if ((tp->tte_data & TD_V) != 0) {
		TSB_STATS_INC(tsb_nrepl);
		ova = tte_get_va(*tp);
		if ((tp->tte_data & TD_PV) != 0) {
			om = PHYS_TO_VM_PAGE(TD_GET_PA(tp->tte_data));
			if ((tp->tte_data & TD_W) != 0 &&
			    pmap_track_modified(pm, ova))
				vm_page_dirty(om);
			if ((tp->tte_data & TD_REF) != 0)
				vm_page_flag_set(om, PG_REFERENCED);
			pmap_cache_remove(om, ova);
			pv_remove(pm, om, ova);
		}
		tlb_page_demap(TLB_DTLB | TLB_ITLB, pm->pm_context, ova);
	}

	*tp = tte;

	CTR1(KTR_TSB, "tsb_tte_enter: return tp=%p", tp);
	return (tp);
}

/*
 * Traverse the tsb of a pmap, calling the callback function for any tte entry
 * that has a virtual address between start and end. If this function returns 0,
 * tsb_foreach() terminates.
 * This is used by pmap_remove() and pmap_protect() in the case that the number
 * of pages in the range given to them reaches the dimensions of the tsb size as
 * an optimization.
 */
void
tsb_foreach(pmap_t pm, vm_offset_t start, vm_offset_t end,
    tsb_callback_t *callback)
{
	vm_offset_t va;
	struct tte *tp;
	int i;

	for (i = 0; i < TSB_SIZE; i++) {
		tp = &pm->pm_tsb[i];
		if ((tp->tte_data & TD_V) != 0) {
			va = tte_get_va(*tp);
			if (va >= start && va < end) {
				if (!callback(pm, tp, va))
					break;
			}
		}
	}
}
