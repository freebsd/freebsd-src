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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
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

struct stte *tsb_kernel;
vm_offset_t tsb_kernel_phys;

struct stte *
tsb_get_bucket(pmap_t pm, u_int level, vm_offset_t va, int allocate)
{
	struct stte *bucket;
	struct stte *stp;
	vm_offset_t bva;
	u_long bits;

	bucket = tsb_vtobucket(va, level);
	if (level == 0)
		return (bucket);
	bits = (va & ((tsb_mask(level) & ~tsb_mask(level - 1)) << PAGE_SHIFT))
	    >> tsb_mask_width(level);
	if (level == 1) {
		bits |= ((long)bucket & TSB_LEVEL1_BUCKET_MASK) >>
		    TSB_LEVEL1_BUCKET_SHIFT;
	}
	bva = trunc_page((u_long)tsb_vtobucket(va, level - 1)) | bits;
	stp = (struct stte *)(long)bva + tsb_bucket_size(level - 1) - 1;
	if (tte_match(stp->st_tte, (u_long)bucket) == 0) {
		if (!allocate)
			return (NULL);
		tsb_page_fault(pm, level, trunc_page((u_long)bucket), stp);
	} else {
		tlb_store_slot(TLB_DTLB, trunc_page((u_long)bucket),
		    TLB_CTX_KERNEL, stp->st_tte, tsb_tlb_slot(1));
	}
	return (bucket);
}

struct tte
tsb_page_alloc(pmap_t pm, vm_offset_t va)
{
	struct tte tte;
	vm_offset_t pa;
	vm_page_t m;

	m = vm_page_alloc(pm->pm_object, pm->pm_pages++, VM_ALLOC_SYSTEM);
	if (m == NULL)
		panic("tsb_page_alloc: vm_page_alloc\n");
	pa = VM_PAGE_TO_PHYS(m);
	tte.tte_tag = TT_CTX(TLB_CTX_KERNEL) | TT_VA(va);
	tte.tte_data = TD_V | TD_8K | TD_VA_LOW(va) | TD_PA(pa) | TD_L |
	    TD_CP | TD_P | TD_W;
	return (tte);
}

void
tsb_page_fault(pmap_t pm, int level, vm_offset_t va, struct stte *stp)
{
	struct tte tte;

	tte = tsb_page_alloc(pm, va);
	stp->st_tte = tte;
	tlb_store_slot(TLB_DTLB, va, TLB_CTX_KERNEL, stp->st_tte,
	    tsb_tlb_slot(level));
	tsb_page_init((void *)va, level);
}

void
tsb_page_init(void *va, int level)
{
	struct stte *stp;
	caddr_t p;
	u_int bsize;
	u_int inc;
	u_int i;

	inc = PAGE_SIZE >> TSB_BUCKET_SPREAD_SHIFT;
	if (level == 0)
		inc >>= TSB_SECONDARY_BUCKET_SHIFT - TSB_PRIMARY_BUCKET_SHIFT;
	bsize = tsb_bucket_size(level);
	bzero(va, PAGE_SIZE);
	for (i = 0; i < PAGE_SIZE; i += inc) {
		p = (caddr_t)va + i;
		stp = (struct stte *)p + bsize - 1;
		stp->st_tte.tte_data = TD_L;
	}
}

struct stte *
tsb_stte_lookup(pmap_t pm, vm_offset_t va)
{
	struct stte *bucket;
	struct stte *stp;
	u_int level;
	u_int i;

	if (pm == kernel_pmap) {
		stp = tsb_kvtostte(va);
		CTR3(KTR_PMAP,
		    "tsb_stte_lookup: kernel va=%#lx stp=%#lx data=%#lx",
		    va, stp, stp->st_tte.tte_data);
		if (tte_match(stp->st_tte, va)) {
			CTR1(KTR_PMAP, "tsb_stte_lookup: match va=%#lx", va);
			return (stp);
		}
		goto out;
	}

	CTR2(KTR_PMAP, "tsb_stte_lookup: ctx=%#lx va=%#lx", pm->pm_context, va);

	va = trunc_page(va);
	for (level = 0; level < TSB_DEPTH; level++) {
		bucket = tsb_get_bucket(pm, level, va, 0);
		CTR2(KTR_PMAP, "tsb_stte_lookup: lvl=%d b=%p", level, bucket);
		if (bucket == NULL)
			break;
		for (i = 0; i < tsb_bucket_size(level); i++) {
			if (tte_match(bucket[i].st_tte, va)) {
				stp = &bucket[i];
				CTR2(KTR_PMAP,
				    "tsb_stte_lookup: match va=%#lx stp=%p",
				    va, stp);
				return (stp);
			}
		}
	}
out:
	CTR2(KTR_PMAP, "tsb_stte_lookup: miss ctx=%#lx va=%#lx",
	    pm->pm_context, va);
	return (NULL);
}

struct stte *
tsb_stte_promote(pmap_t pm, vm_offset_t va, struct stte *stp)
{
	struct stte *bucket;
	struct tte tte;
	int bmask;
	int b0;
	int i;

	CTR3(KTR_PMAP, "tsb_stte_promote: ctx=%#x va=%#lx stp=%p",
	    pm->pm_context, va, stp);
	bmask = tsb_bucket_mask(0);
	bucket = tsb_vtobucket(va, 0);
	b0 = rd(tick) & bmask;
	i = b0;
	do {
		if ((bucket[i].st_tte.tte_data & TD_V) == 0 ||
		    (bucket[i].st_tte.tte_data & (TD_L | TD_REF)) == 0) {
			tte = stp->st_tte;
			CTR3(KTR_PMAP,
			    "tsb_stte_promote: replace stp=%p va=%#lx pa=%#lx",
			    stp, tte_get_va(tte), TD_PA(tte.tte_data));
			if (tte.tte_data & TD_MNG)
				pv_remove_virt(stp);
			stp->st_tte.tte_data = 0;
			stp = tsb_tte_enter(pm, va, tte);
			break;
		}
	} while ((i = (i + 1) & bmask) != b0);
	CTR1(KTR_PMAP, "tsb_stte_promote: return stp=%p", stp);
	return (stp);
}

void
tsb_stte_remove(struct stte *stp)
{
	struct tte tte;

	tte = stp->st_tte;
	if (tte.tte_data & TD_V) {
		tte_invalidate(&stp->st_tte);
		tsb_tte_local_remove(&tte);
	}
}

void
tsb_tte_local_remove(struct tte *tp)
{
	tlb_page_demap(TLB_DTLB | TLB_ITLB, TT_GET_CTX(tp->tte_tag),
	    tte_get_va(*tp));
}

struct stte *
tsb_tte_enter(pmap_t pm, vm_offset_t va, struct tte tte)
{
	struct stte *bucket;
	struct stte *nstp;
	struct stte *rstp;
	struct stte *stp;
	struct tte otte;
	u_int bmask;
	int level;
	int b0;
	int i;

	CTR3(KTR_PMAP, "tsb_tte_enter: ctx=%#lx va=%#lx data=%#lx",
	    pm->pm_context, va, tte.tte_data);

	if (pm == kernel_pmap) {
		stp = tsb_kvtostte(va);
		if (stp->st_tte.tte_data & TD_MNG)
			pv_remove_virt(stp);
		stp->st_tte = tte;
		if (tte.tte_data & TD_MNG)
			pv_insert(pm, TD_PA(tte.tte_data), va, stp);
		return (stp);
	}

	nstp = NULL;
	for (level = 0; level < TSB_DEPTH; level++) {
		bucket = tsb_get_bucket(pm, level, va, 1);
		CTR3(KTR_PMAP, "tsb_tte_enter: va=%#lx bucket=%p level=%d",
		    va, bucket, level);

		stp = NULL;
		rstp = NULL;
		bmask = tsb_bucket_mask(level);
		b0 = rd(tick) & bmask;
		i = b0;
		do {
			if ((bucket[i].st_tte.tte_data & TD_L) != 0)
				continue;
			if ((bucket[i].st_tte.tte_data & TD_V) == 0) {
				stp = &bucket[i];
				break;
			}
			if (stp == NULL) {
				if ((bucket[i].st_tte.tte_data & TD_REF) == 0)
					stp = &bucket[i];
				else if (rstp == NULL)
					rstp = &bucket[i];
			}
		} while ((i = (i + 1) & bmask) != b0);

		if (stp == NULL)
			stp = rstp;
		if (stp == NULL)
			panic("tsb_enter_tte");
		if (nstp == NULL)
			nstp = stp;

		otte = stp->st_tte;
		if (otte.tte_data & TD_V && otte.tte_data & TD_MNG)
			pv_remove_virt(stp);
		stp->st_tte = tte;
		if (tte.tte_data & TD_MNG)
			pv_insert(pm, TD_PA(tte.tte_data), va, stp);
		if ((otte.tte_data & TD_V) == 0)
			break;
		tte = otte;
		va = tte_get_va(tte);
	}
	CTR1(KTR_PMAP, "tsb_tte_enter: return stp=%p", nstp);
	if (level >= TSB_DEPTH)
		panic("tsb_enter_tte: TSB full");
	return (nstp);
}
