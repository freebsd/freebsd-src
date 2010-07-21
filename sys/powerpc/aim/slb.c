/*-
 * Copyright (c) 2010 Nathan Whitehorn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/tree.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_map.h>

#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

uintptr_t moea64_get_unique_vsid(void);
void moea64_release_vsid(uint64_t vsid);

struct slbcontainer {
	struct slb slb;
	SPLAY_ENTRY(slbcontainer) slb_node;
};

static int slb_compare(struct slbcontainer *a, struct slbcontainer *b);
static void slb_zone_init(void *);

SPLAY_PROTOTYPE(slb_tree, slbcontainer, slb_node, slb_compare);
SPLAY_GENERATE(slb_tree, slbcontainer, slb_node, slb_compare);

uma_zone_t slb_zone;
uma_zone_t slb_cache_zone;

SYSINIT(slb_zone_init, SI_SUB_KMEM, SI_ORDER_ANY, slb_zone_init, NULL);

int
va_to_slb_entry(pmap_t pm, vm_offset_t va, struct slb *slb)
{
	struct slbcontainer cont, *found;
	uint64_t esid;

	esid = (uintptr_t)va >> ADDR_SR_SHFT;
	slb->slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID;

	if (pm == kernel_pmap) {
		/* Set kernel VSID to deterministic value */
		slb->slbv = va_to_vsid(kernel_pmap, va) << SLBV_VSID_SHIFT;

		/* Figure out if this is a large-page mapping */
		if (hw_direct_map && va < VM_MIN_KERNEL_ADDRESS) {
			/*
			 * XXX: If we have set up a direct map, assumes
			 * all physical memory is mapped with large pages.
			 */
			if (mem_valid(va, 0) == 0)
				slb->slbv |= SLBV_L;
		}
			
		return (0);
	}

	PMAP_LOCK_ASSERT(pm, MA_OWNED);

	cont.slb.slbe = slb->slbe;
	found = SPLAY_FIND(slb_tree, &pm->pm_slbtree, &cont);

	if (found == NULL)
		return (-1);

	slb->slbv = found->slb.slbv;
	return (0);
}

uint64_t
va_to_vsid(pmap_t pm, vm_offset_t va)
{
	struct slb entry;
	int large;

	/* Shortcut kernel case */
	if (pm == kernel_pmap) {
		large = 0;
		if (hw_direct_map && va < VM_MIN_KERNEL_ADDRESS &&
		    mem_valid(va, 0) == 0)
			large = 1;

		return (KERNEL_VSID((uintptr_t)va >> ADDR_SR_SHFT, large));
	}

	/*
	 * If there is no vsid for this VA, we need to add a new entry
	 * to the PMAP's segment table.
	 */

	if (va_to_slb_entry(pm, va, &entry) != 0)
		return (allocate_vsid(pm, (uintptr_t)va >> ADDR_SR_SHFT, 0));

	return ((entry.slbv & SLBV_VSID_MASK) >> SLBV_VSID_SHIFT);
}

uint64_t
allocate_vsid(pmap_t pm, uint64_t esid, int large)
{
	uint64_t vsid;
	struct slbcontainer *slb_entry, kern_entry;
	struct slb *prespill;

	prespill = NULL;

	if (pm == kernel_pmap) {
		vsid = va_to_vsid(pm, esid << ADDR_SR_SHFT);
		slb_entry = &kern_entry;
		prespill = PCPU_GET(slb);
	} else {
		vsid = moea64_get_unique_vsid();
		slb_entry = uma_zalloc(slb_zone, M_NOWAIT);

		if (slb_entry == NULL)
			panic("Could not allocate SLB mapping!");

		prespill = pm->pm_slb;
	}

	slb_entry->slb.slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID;
	slb_entry->slb.slbv = vsid << SLBV_VSID_SHIFT;

	if (large)
		slb_entry->slb.slbv |= SLBV_L;

	if (pm != kernel_pmap) {
		PMAP_LOCK_ASSERT(pm, MA_OWNED);
		SPLAY_INSERT(slb_tree, &pm->pm_slbtree, slb_entry);
	}

	/*
	 * Someone probably wants this soon, and it may be a wired
	 * SLB mapping, so pre-spill this entry.
	 */
	if (prespill != NULL)
		slb_insert(pm, prespill, &slb_entry->slb);

	return (vsid);
}

/* Lock entries mapping kernel text and stacks */

#define SLB_SPILLABLE(slbe) \
	(((slbe & SLBE_ESID_MASK) < VM_MIN_KERNEL_ADDRESS && \
	    (slbe & SLBE_ESID_MASK) > 16*SEGMENT_LENGTH) || \
	    (slbe & SLBE_ESID_MASK) > VM_MAX_KERNEL_ADDRESS)
void
slb_insert(pmap_t pm, struct slb *slbcache, struct slb *slb_entry)
{
	uint64_t slbe, slbv;
	int i, j, to_spill;

	/* We don't want to be preempted while modifying the kernel map */
	critical_enter();

	to_spill = -1;
	slbv = slb_entry->slbv;
	slbe = slb_entry->slbe;

	/* Hunt for a likely candidate */

	for (i = mftb() % 64, j = 0; j < 64; j++, i = (i+1) % 64) {
		if (pm == kernel_pmap && i == USER_SR)
				continue;

		if (!(slbcache[i].slbe & SLBE_VALID)) {
			to_spill = i;
			break;
		}

		if (to_spill < 0 && (pm != kernel_pmap ||
		    SLB_SPILLABLE(slbcache[i].slbe)))
			to_spill = i;
	}

	if (to_spill < 0)
		panic("SLB spill on ESID %#lx, but no available candidates!\n",
		   (slbe & SLBE_ESID_MASK) >> SLBE_ESID_SHIFT);

	if (slbcache[to_spill].slbe & SLBE_VALID) {
		/* Invalidate this first to avoid races */
		slbcache[to_spill].slbe = 0;
		mb();
	}
	slbcache[to_spill].slbv = slbv;
	slbcache[to_spill].slbe = slbe | (uint64_t)to_spill;

	/* If it is for this CPU, put it in the SLB right away */
	if (pm == kernel_pmap && pmap_bootstrapped) {
		/* slbie not required */
		__asm __volatile ("slbmte %0, %1" :: 
		    "r"(slbcache[to_spill].slbv),
		    "r"(slbcache[to_spill].slbe)); 
	}

	critical_exit();
}

int
vsid_to_esid(pmap_t pm, uint64_t vsid, uint64_t *esid)
{
	uint64_t slbv;
	struct slbcontainer *entry;

#ifdef INVARIANTS
	if (pm == kernel_pmap)
		panic("vsid_to_esid only works on user pmaps");

	PMAP_LOCK_ASSERT(pm, MA_OWNED);
#endif

	slbv = vsid << SLBV_VSID_SHIFT;

	SPLAY_FOREACH(entry, slb_tree, &pm->pm_slbtree) {
		if (slbv == entry->slb.slbv) {
			*esid = entry->slb.slbe >> SLBE_ESID_SHIFT;
			return (0);
		}
	}

	return (-1);
}

void
free_vsids(pmap_t pm)
{
	struct slbcontainer *entry;

	while (!SPLAY_EMPTY(&pm->pm_slbtree)) {
		entry = SPLAY_MIN(slb_tree, &pm->pm_slbtree);

		SPLAY_REMOVE(slb_tree, &pm->pm_slbtree, entry);

		moea64_release_vsid(entry->slb.slbv >> SLBV_VSID_SHIFT);
		uma_zfree(slb_zone, entry);
	}
}

static int
slb_compare(struct slbcontainer *a, struct slbcontainer *b)
{
	if (a->slb.slbe == b->slb.slbe)
		return (0);
	else if (a->slb.slbe < b->slb.slbe)
		return (-1);
	else
		return (1);
}

static void
slb_zone_init(void *dummy)
{

	slb_zone = uma_zcreate("SLB segment", sizeof(struct slbcontainer),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	slb_cache_zone = uma_zcreate("SLB cache", 64*sizeof(struct slb),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
}

struct slb *
slb_alloc_user_cache(void)
{
	return (uma_zalloc(slb_cache_zone, M_ZERO));
}

void
slb_free_user_cache(struct slb *slb)
{
	uma_zfree(slb_cache_zone, slb);
}
