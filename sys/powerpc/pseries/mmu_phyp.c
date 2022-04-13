/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2010 Andreas Tobler
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>

#include <powerpc/aim/mmu_oea64.h>

#include "phyp-hvcall.h"

#define MMU_PHYP_DEBUG 0
#define MMU_PHYP_ID "mmu_phyp: "
#if MMU_PHYP_DEBUG
#define dprintf(fmt, ...) printf(fmt, ## __VA_ARGS__)
#define dprintf0(fmt, ...) dprintf(MMU_PHYP_ID fmt, ## __VA_ARGS__)
#else
#define dprintf(fmt, args...) do { ; } while(0)
#define dprintf0(fmt, args...) do { ; } while(0)
#endif

static struct rmlock mphyp_eviction_lock;

/*
 * Kernel MMU interface
 */

static void	mphyp_install(void);
static void	mphyp_bootstrap(vm_offset_t kernelstart,
		    vm_offset_t kernelend);
static void	mphyp_cpu_bootstrap(int ap);
static void	*mphyp_dump_pmap(void *ctx, void *buf,
		    u_long *nbytes);
static int64_t	mphyp_pte_synch(struct pvo_entry *pvo);
static int64_t	mphyp_pte_clear(struct pvo_entry *pvo, uint64_t ptebit);
static int64_t	mphyp_pte_unset(struct pvo_entry *pvo);
static int64_t	mphyp_pte_insert(struct pvo_entry *pvo);
static int64_t	mphyp_pte_unset_sp(struct pvo_entry *pvo);
static int64_t	mphyp_pte_insert_sp(struct pvo_entry *pvo);
static int64_t	mphyp_pte_replace_sp(struct pvo_entry *pvo);

static struct pmap_funcs mphyp_methods = {
	.install =           mphyp_install,
        .bootstrap =         mphyp_bootstrap,
        .cpu_bootstrap =     mphyp_cpu_bootstrap,
        .dumpsys_dump_pmap = mphyp_dump_pmap,
};

static struct moea64_funcs mmu_phyp_funcs = {
	.pte_synch =      mphyp_pte_synch,
        .pte_clear =      mphyp_pte_clear,
        .pte_unset =      mphyp_pte_unset,
        .pte_insert =     mphyp_pte_insert,
        .pte_unset_sp =   mphyp_pte_unset_sp,
        .pte_insert_sp =  mphyp_pte_insert_sp,
        .pte_replace_sp = mphyp_pte_replace_sp,
};

MMU_DEF_INHERIT(pseries_mmu, "mmu_phyp", mphyp_methods, oea64_mmu);

static int brokenkvm = 0;
static uint64_t final_pteg_count = 0;

static void
print_kvm_bug_warning(void *data)
{

	if (brokenkvm)
		printf("WARNING: Running on a broken hypervisor that does "
		    "not support mandatory H_CLEAR_MOD and H_CLEAR_REF "
		    "hypercalls. Performance will be suboptimal.\n");
}

SYSINIT(kvmbugwarn1, SI_SUB_COPYRIGHT, SI_ORDER_THIRD + 1,
    print_kvm_bug_warning, NULL);
SYSINIT(kvmbugwarn2, SI_SUB_LAST, SI_ORDER_THIRD + 1, print_kvm_bug_warning,
    NULL);

static void
mphyp_install()
{
	char buf[8];
	uint32_t prop[2];
	uint32_t nptlp, shift = 0, slb_encoding = 0;
	uint32_t lp_size, lp_encoding;
	phandle_t dev, node, root;
	int idx, len, res;
	bool has_lp;

	root = OF_peer(0);

	dev = OF_child(root);
	while (dev != 0) {
		res = OF_getprop(dev, "name", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpus") == 0)
			break;
		dev = OF_peer(dev);
	}

	node = OF_child(dev);

	while (node != 0) {
		res = OF_getprop(node, "device_type", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpu") == 0)
			break;
		node = OF_peer(node);
	}

	res = OF_getencprop(node, "ibm,pft-size", prop, sizeof(prop));
	if (res <= 0)
		panic("mmu_phyp: unknown PFT size");
	final_pteg_count = 1 << prop[1];
	res = OF_getencprop(node, "ibm,slb-size", prop, sizeof(prop[0]));
	if (res > 0)
		n_slbs = prop[0];
	dprintf0("slb-size=%i\n", n_slbs);

	/*
	 * Scan the large page size property for PAPR compatible machines.
	 * See PAPR D.5 Changes to Section 5.1.4, 'CPU Node Properties'
	 * for the encoding of the property.
	 */

	len = OF_getproplen(node, "ibm,segment-page-sizes");
	if (len > 0) {
		/*
		 * We have to use a variable length array on the stack
		 * since we have very limited stack space.
		 */
		pcell_t arr[len/sizeof(cell_t)];
		res = OF_getencprop(node, "ibm,segment-page-sizes", arr,
		    sizeof(arr));
		len /= 4;
		idx = 0;
		has_lp = false;
		while (len > 0) {
			shift = arr[idx];
			slb_encoding = arr[idx + 1];
			nptlp = arr[idx + 2];

			dprintf0("Segment Page Size: "
			    "%uKB, slb_enc=0x%X: {size, encoding}[%u] =",
			    shift > 10? 1 << (shift-10) : 0,
			    slb_encoding, nptlp);

			idx += 3;
			len -= 3;
			while (len > 0 && nptlp) {
				lp_size = arr[idx];
				lp_encoding = arr[idx+1];

				dprintf(" {%uKB, 0x%X}",
				    lp_size > 10? 1 << (lp_size-10) : 0,
				    lp_encoding);

				if (slb_encoding == SLBV_L && lp_encoding == 0)
					has_lp = true;

				if (slb_encoding == SLB_PGSZ_4K_4K &&
				    lp_encoding == LP_4K_16M)
					moea64_has_lp_4k_16m = true;

				idx += 2;
				len -= 2;
				nptlp--;
			}
			dprintf("\n");
			if (has_lp && moea64_has_lp_4k_16m)
				break;
		}

		if (has_lp) {
			moea64_large_page_shift = shift;
			moea64_large_page_size = 1ULL << lp_size;
			moea64_large_page_mask = moea64_large_page_size - 1;
			hw_direct_map = 1;
			printf(MMU_PHYP_ID
			    "Support for hugepages of %uKB detected\n",
			    moea64_large_page_shift > 10?
				1 << (moea64_large_page_shift-10) : 0);
		} else {
			moea64_large_page_size = 0;
			moea64_large_page_shift = 0;
			moea64_large_page_mask = 0;
			hw_direct_map = 0;
			printf(MMU_PHYP_ID
			    "Support for hugepages not found\n");
		}
	}

	moea64_ops = &mmu_phyp_funcs;

	moea64_install();
}

static void
mphyp_bootstrap(vm_offset_t kernelstart, vm_offset_t kernelend)
{
	struct lpte old;
	uint64_t vsid;
	int idx;

	rm_init(&mphyp_eviction_lock, "pte eviction");

	moea64_early_bootstrap(kernelstart, kernelend);

	moea64_pteg_count = final_pteg_count / sizeof(struct lpteg);

	/* Clear any old page table entries */
	for (idx = 0; idx < moea64_pteg_count*8; idx++) {
		phyp_pft_hcall(H_READ, 0, idx, 0, 0, &old.pte_hi,
		    &old.pte_lo, &old.pte_lo);
		vsid = (old.pte_hi << (ADDR_API_SHFT64 - ADDR_PIDX_SHFT)) >> 28;
		if (vsid == VSID_VRMA || vsid == 0 /* Older VRMA */)
			continue;

		if (old.pte_hi & LPTE_VALID)
			phyp_hcall(H_REMOVE, 0, idx, 0);
	}

	moea64_mid_bootstrap(kernelstart, kernelend);
	moea64_late_bootstrap(kernelstart, kernelend);

	/* Test for broken versions of KVM that don't conform to the spec */
	if (phyp_hcall(H_CLEAR_MOD, 0, 0) == H_FUNCTION)
		brokenkvm = 1;
}

static void
mphyp_cpu_bootstrap(int ap)
{
	struct slb *slb = PCPU_GET(aim.slb);
	register_t seg0;
	int i;

	/*
	 * Install kernel SLB entries
	 */

        __asm __volatile ("slbia");
        __asm __volatile ("slbmfee %0,%1; slbie %0;" : "=r"(seg0) : "r"(0));
	for (i = 0; i < 64; i++) {
		if (!(slb[i].slbe & SLBE_VALID))
			continue;

		__asm __volatile ("slbmte %0, %1" ::
		    "r"(slb[i].slbv), "r"(slb[i].slbe));
	}
}

static int64_t
mphyp_pte_synch(struct pvo_entry *pvo)
{
	struct lpte pte;
	uint64_t junk;

	__asm __volatile("ptesync");
	phyp_pft_hcall(H_READ, 0, pvo->pvo_pte.slot, 0, 0, &pte.pte_hi,
	    &pte.pte_lo, &junk);
	if ((pte.pte_hi & LPTE_AVPN_MASK) !=
	    ((pvo->pvo_vpn >> (ADDR_API_SHFT64 - ADDR_PIDX_SHFT)) &
	    LPTE_AVPN_MASK))
		return (-1);
	if (!(pte.pte_hi & LPTE_VALID))
		return (-1);

	return (pte.pte_lo & (LPTE_CHG | LPTE_REF));
}

static int64_t
mphyp_pte_clear(struct pvo_entry *pvo, uint64_t ptebit)
{
	struct rm_priotracker track;
	int64_t refchg;
	uint64_t ptelo, junk;
	int err __diagused;

	/*
	 * This involves two steps (synch and clear) so we need the entry
	 * not to change in the middle. We are protected against deliberate
	 * unset by virtue of holding the pmap lock. Protection against
	 * incidental unset (page table eviction) comes from holding the
	 * shared eviction lock.
	 */
	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);
	rm_rlock(&mphyp_eviction_lock, &track);

	refchg = mphyp_pte_synch(pvo);
	if (refchg < 0) {
		rm_runlock(&mphyp_eviction_lock, &track);
		return (refchg);
	}

	if (brokenkvm) {
		/*
		 * No way to clear either bit, which is total madness.
		 * Pessimistically claim that, once modified, it stays so
		 * forever and that it is never referenced.
		 */
		rm_runlock(&mphyp_eviction_lock, &track);
		return (refchg & ~LPTE_REF);
	}

	if (ptebit & LPTE_CHG) {
		err = phyp_pft_hcall(H_CLEAR_MOD, 0, pvo->pvo_pte.slot, 0, 0,
		    &ptelo, &junk, &junk);
		KASSERT(err == H_SUCCESS,
		    ("Error clearing page change bit: %d", err));
		refchg |= (ptelo & LPTE_CHG);
	}
	if (ptebit & LPTE_REF) {
		err = phyp_pft_hcall(H_CLEAR_REF, 0, pvo->pvo_pte.slot, 0, 0,
		    &ptelo, &junk, &junk);
		KASSERT(err == H_SUCCESS,
		    ("Error clearing page reference bit: %d", err));
		refchg |= (ptelo & LPTE_REF);
	}

	rm_runlock(&mphyp_eviction_lock, &track);

	return (refchg);
}

static int64_t
mphyp_pte_unset(struct pvo_entry *pvo)
{
	struct lpte pte;
	uint64_t junk;
	int err;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);

	moea64_pte_from_pvo(pvo, &pte);

	err = phyp_pft_hcall(H_REMOVE, H_AVPN, pvo->pvo_pte.slot,
	    pte.pte_hi & LPTE_AVPN_MASK, 0, &pte.pte_hi, &pte.pte_lo,
	    &junk);
	KASSERT(err == H_SUCCESS || err == H_NOT_FOUND,
	    ("Error removing page: %d", err));

	if (err == H_NOT_FOUND) {
		STAT_MOEA64(moea64_pte_overflow--);
		return (-1);
	}

	return (pte.pte_lo & (LPTE_REF | LPTE_CHG));
}

static uintptr_t
mphyp_pte_spillable_ident(uintptr_t ptegbase, struct lpte *to_evict)
{
	uint64_t slot, junk, k;
	struct lpte pt;
	int     i, j;

	/* Start at a random slot */
	i = mftb() % 8;
	k = -1;
	for (j = 0; j < 8; j++) {
		slot = ptegbase + (i + j) % 8;
		phyp_pft_hcall(H_READ, 0, slot, 0, 0, &pt.pte_hi,
		    &pt.pte_lo, &junk);
		
		if ((pt.pte_hi & (LPTE_WIRED | LPTE_BIG)) != 0)
			continue;

		/* This is a candidate, so remember it */
		k = slot;

		/* Try to get a page that has not been used lately */
		if (!(pt.pte_hi & LPTE_VALID) || !(pt.pte_lo & LPTE_REF)) {
			memcpy(to_evict, &pt, sizeof(struct lpte));
			return (k);
		}
	}

	if (k == -1)
		return (k);

	phyp_pft_hcall(H_READ, 0, k, 0, 0, &to_evict->pte_hi,
	    &to_evict->pte_lo, &junk);
	return (k);
}

static __inline int64_t
mphyp_pte_insert_locked(struct pvo_entry *pvo, struct lpte *pte)
{
	struct lpte evicted;
	uint64_t index, junk;
	int64_t result;

	/*
	 * First try primary hash.
	 */
	pvo->pvo_pte.slot &= ~7UL; /* Base slot address */
	result = phyp_pft_hcall(H_ENTER, 0, pvo->pvo_pte.slot, pte->pte_hi,
	    pte->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS) {
		pvo->pvo_pte.slot = index;
		return (0);
	}
	KASSERT(result == H_PTEG_FULL, ("Page insertion error: %ld "
	    "(ptegidx: %#zx/%#lx, PTE %#lx/%#lx", result, pvo->pvo_pte.slot,
	    moea64_pteg_count, pte->pte_hi, pte->pte_lo));

	/*
	 * Next try secondary hash.
	 */
	pvo->pvo_vaddr ^= PVO_HID;
	pte->pte_hi ^= LPTE_HID;
	pvo->pvo_pte.slot ^= (moea64_pteg_mask << 3);

	result = phyp_pft_hcall(H_ENTER, 0, pvo->pvo_pte.slot,
	    pte->pte_hi, pte->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS) {
		pvo->pvo_pte.slot = index;
		return (0);
	}
	KASSERT(result == H_PTEG_FULL, ("Secondary page insertion error: %ld",
	    result));

	return (-1);
}


static __inline int64_t
mphyp_pte_evict_and_insert_locked(struct pvo_entry *pvo, struct lpte *pte)
{
	struct lpte evicted;
	uint64_t index, junk, lastptelo;
	int64_t result;

	evicted.pte_hi = 0;

	index = mphyp_pte_spillable_ident(pvo->pvo_pte.slot, &evicted);
	if (index == -1L) {
		/* Try other hash table? */
		pvo->pvo_vaddr ^= PVO_HID;
		pte->pte_hi ^= LPTE_HID;
		pvo->pvo_pte.slot ^= (moea64_pteg_mask << 3);
		index = mphyp_pte_spillable_ident(pvo->pvo_pte.slot, &evicted);
	}

	if (index == -1L) {
		/* No freeable slots in either PTEG? We're hosed. */
		rm_wunlock(&mphyp_eviction_lock);
		panic("mphyp_pte_insert: overflow");
		return (-1);
	}

	/* Victim acquired: update page before waving goodbye */
	if (evicted.pte_hi & LPTE_VALID) {
		result = phyp_pft_hcall(H_REMOVE, H_AVPN, index,
		    evicted.pte_hi & LPTE_AVPN_MASK, 0, &junk, &lastptelo,
		    &junk);
		STAT_MOEA64(moea64_pte_overflow++);
		KASSERT(result == H_SUCCESS || result == H_NOT_FOUND,
		    ("Error evicting page: %d", (int)result));
	}

	/*
	 * Set the new PTE.
	 */
	result = phyp_pft_hcall(H_ENTER, H_EXACT, index, pte->pte_hi,
	    pte->pte_lo, &index, &evicted.pte_lo, &junk);

	pvo->pvo_pte.slot = index;
	if (result == H_SUCCESS)
		return (0);

	rm_wunlock(&mphyp_eviction_lock);
	panic("Page replacement error: %ld", result);
	return (result);
}

static int64_t
mphyp_pte_insert(struct pvo_entry *pvo)
{
	struct rm_priotracker track;
	int64_t ret;
	struct lpte pte;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);

	/* Initialize PTE */
	moea64_pte_from_pvo(pvo, &pte);

	/* Make sure further insertion is locked out during evictions */
	rm_rlock(&mphyp_eviction_lock, &track);

	ret = mphyp_pte_insert_locked(pvo, &pte);
	rm_runlock(&mphyp_eviction_lock, &track);

	if (ret == -1) {
		/*
		 * Out of luck. Find a PTE to sacrifice.
		 */

		/* Lock out all insertions for a bit */
		rm_wlock(&mphyp_eviction_lock);
		ret = mphyp_pte_evict_and_insert_locked(pvo, &pte);
		rm_wunlock(&mphyp_eviction_lock); /* All clear */
	}

	return (ret);
}

static void *
mphyp_dump_pmap(void *ctx, void *buf, u_long *nbytes)
{
	struct dump_context *dctx;
	struct lpte p, *pbuf;
	int bufidx;
	uint64_t junk;
	u_long ptex, ptex_end;

	dctx = (struct dump_context *)ctx;
	pbuf = (struct lpte *)buf;
	bufidx = 0;
	ptex = dctx->ptex;
	ptex_end = ptex + dctx->blksz / sizeof(struct lpte);
	ptex_end = MIN(ptex_end, dctx->ptex_end);
	*nbytes = (ptex_end - ptex) * sizeof(struct lpte);

	if (*nbytes == 0)
		return (NULL);

	for (; ptex < ptex_end; ptex++) {
		phyp_pft_hcall(H_READ, 0, ptex, 0, 0,
			&p.pte_hi, &p.pte_lo, &junk);
		pbuf[bufidx++] = p;
	}

	dctx->ptex = ptex;
	return (buf);
}

static int64_t
mphyp_pte_unset_sp(struct pvo_entry *pvo)
{
	struct lpte pte;
	uint64_t junk, refchg;
	int err;
	vm_offset_t eva;
	pmap_t pm __diagused;

	pm = pvo->pvo_pmap;
	PMAP_LOCK_ASSERT(pm, MA_OWNED);
	KASSERT((PVO_VADDR(pvo) & HPT_SP_MASK) == 0,
	    ("%s: va %#jx unaligned", __func__, (uintmax_t)PVO_VADDR(pvo)));

	refchg = 0;
	eva = PVO_VADDR(pvo) + HPT_SP_SIZE;

	for (; pvo != NULL && PVO_VADDR(pvo) < eva;
	    pvo = RB_NEXT(pvo_tree, &pm->pmap_pvo, pvo)) {
		moea64_pte_from_pvo(pvo, &pte);

		err = phyp_pft_hcall(H_REMOVE, H_AVPN, pvo->pvo_pte.slot,
		    pte.pte_hi & LPTE_AVPN_MASK, 0, &pte.pte_hi, &pte.pte_lo,
		    &junk);
		KASSERT(err == H_SUCCESS || err == H_NOT_FOUND,
		    ("Error removing page: %d", err));

		if (err == H_NOT_FOUND)
			STAT_MOEA64(moea64_pte_overflow--);
		refchg |= pte.pte_lo & (LPTE_REF | LPTE_CHG);
	}

	return (refchg);
}

static int64_t
mphyp_pte_insert_sp(struct pvo_entry *pvo)
{
	struct rm_priotracker track;
	int64_t ret;
	struct lpte pte;
	vm_offset_t eva;
	pmap_t pm __diagused;

	pm = pvo->pvo_pmap;
	PMAP_LOCK_ASSERT(pm, MA_OWNED);
	KASSERT((PVO_VADDR(pvo) & HPT_SP_MASK) == 0,
	    ("%s: va %#jx unaligned", __func__, (uintmax_t)PVO_VADDR(pvo)));

	eva = PVO_VADDR(pvo) + HPT_SP_SIZE;

	/* Make sure further insertion is locked out during evictions */
	rm_rlock(&mphyp_eviction_lock, &track);

	for (; pvo != NULL && PVO_VADDR(pvo) < eva;
	    pvo = RB_NEXT(pvo_tree, &pm->pmap_pvo, pvo)) {
		/* Initialize PTE */
		moea64_pte_from_pvo(pvo, &pte);

		ret = mphyp_pte_insert_locked(pvo, &pte);
		if (ret == -1) {
			/*
			 * Out of luck. Find a PTE to sacrifice.
			 */

			/* Lock out all insertions for a bit */
			rm_runlock(&mphyp_eviction_lock, &track);
			rm_wlock(&mphyp_eviction_lock);
			mphyp_pte_evict_and_insert_locked(pvo, &pte);
			rm_wunlock(&mphyp_eviction_lock); /* All clear */
			rm_rlock(&mphyp_eviction_lock, &track);
		}
	}

	rm_runlock(&mphyp_eviction_lock, &track);
	return (0);
}

static int64_t
mphyp_pte_replace_sp(struct pvo_entry *pvo)
{
	int64_t refchg;

	refchg = mphyp_pte_unset_sp(pvo);
	mphyp_pte_insert_sp(pvo);
	return (refchg);
}
