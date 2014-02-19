/*
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
#include <sys/msgbuf.h>
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

#include "mmu_if.h"
#include "moea64_if.h"

#include "phyp-hvcall.h"

extern int n_slbs;

/*
 * Kernel MMU interface
 */

static void	mphyp_bootstrap(mmu_t mmup, vm_offset_t kernelstart,
		    vm_offset_t kernelend);
static void	mphyp_cpu_bootstrap(mmu_t mmup, int ap);
static void	mphyp_pte_synch(mmu_t, uintptr_t pt, struct lpte *pvo_pt);
static void	mphyp_pte_clear(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn, u_int64_t ptebit);
static void	mphyp_pte_unset(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static void	mphyp_pte_change(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static int	mphyp_pte_insert(mmu_t, u_int ptegidx, struct lpte *pvo_pt);
static uintptr_t mphyp_pvo_to_pte(mmu_t, const struct pvo_entry *pvo);

#define VSID_HASH_MASK		0x0000007fffffffffULL


static mmu_method_t mphyp_methods[] = {
        MMUMETHOD(mmu_bootstrap,        mphyp_bootstrap),
        MMUMETHOD(mmu_cpu_bootstrap,    mphyp_cpu_bootstrap),

	MMUMETHOD(moea64_pte_synch,     mphyp_pte_synch),
        MMUMETHOD(moea64_pte_clear,     mphyp_pte_clear),
        MMUMETHOD(moea64_pte_unset,     mphyp_pte_unset),
        MMUMETHOD(moea64_pte_change,    mphyp_pte_change),
        MMUMETHOD(moea64_pte_insert,    mphyp_pte_insert),
        MMUMETHOD(moea64_pvo_to_pte,    mphyp_pvo_to_pte),

        { 0, 0 }
};

MMU_DEF_INHERIT(pseries_mmu, "mmu_phyp", mphyp_methods, 0, oea64_mmu);

static void
mphyp_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	uint64_t final_pteg_count = 0;
	char buf[8];
	uint32_t prop[2];
	uint32_t nptlp, shift = 0, slb_encoding = 0;
	phandle_t dev, node, root;
	int idx, len, res;

	moea64_early_bootstrap(mmup, kernelstart, kernelend);

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

	res = OF_getprop(node, "ibm,pft-size", prop, sizeof(prop));
	if (res <= 0)
		panic("mmu_phyp: unknown PFT size");
	final_pteg_count = 1 << prop[1];
	res = OF_getprop(node, "ibm,slb-size", prop, sizeof(prop[0]));
	if (res > 0)
		n_slbs = prop[0];

	moea64_pteg_count = final_pteg_count / sizeof(struct lpteg);

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
		cell_t arr[len/sizeof(cell_t)];
		res = OF_getprop(node, "ibm,segment-page-sizes", &arr,
				 sizeof(arr));
		len /= 4;
		idx = 0;
		while (len > 0) {
			shift = arr[idx];
			slb_encoding = arr[idx + 1];
			nptlp = arr[idx + 2];
			idx += 3;
			len -= 3;
			while (len > 0 && nptlp) {
				idx += 2;
				len -= 2;
				nptlp--;
			}
		}

		/* For now we allow shift only to be <= 0x18. */
		if (shift >= 0x18)
		    shift = 0x18;

		moea64_large_page_shift = shift;
		moea64_large_page_size = 1ULL << shift;
	}

	moea64_mid_bootstrap(mmup, kernelstart, kernelend);
	moea64_late_bootstrap(mmup, kernelstart, kernelend);
}

static void
mphyp_cpu_bootstrap(mmu_t mmup, int ap)
{
	struct slb *slb = PCPU_GET(slb);
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

static void
mphyp_pte_synch(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt)
{
	struct lpte pte;
	uint64_t junk;

	__asm __volatile("ptesync");
	phyp_pft_hcall(H_READ, 0, slot, 0, 0, &pte.pte_hi, &pte.pte_lo,
	    &junk);

	pvo_pt->pte_lo |= pte.pte_lo & (LPTE_CHG | LPTE_REF);
}

static void
mphyp_pte_clear(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn,
    u_int64_t ptebit)
{

	if (ptebit & LPTE_CHG)
		phyp_hcall(H_CLEAR_MOD, 0, slot);
	if (ptebit & LPTE_REF)
		phyp_hcall(H_CLEAR_REF, 0, slot);
}

static void
mphyp_pte_unset(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn)
{
	struct lpte pte;
	uint64_t junk;
	int err;

	err = phyp_pft_hcall(H_REMOVE, 1UL << 31, slot,
	    pvo_pt->pte_hi & LPTE_AVPN_MASK, 0, &pte.pte_hi, &pte.pte_lo,
	    &junk);
	KASSERT(err == H_SUCCESS, ("Error removing page: %d", err));

	pvo_pt->pte_lo |= pte.pte_lo & (LPTE_CHG | LPTE_REF);
}

static void
mphyp_pte_change(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn)
{
	struct lpte evicted;
	uint64_t index, junk;
	int64_t result;

	/*
	 * NB: this is protected by the global table lock, so this two-step
	 * is safe, except for the scratch-page case. No CPUs on which we run
	 * this code should be using scratch pages.
	 */
	KASSERT(!(pvo_pt->pte_hi & LPTE_LOCKED),
	    ("Locked pages not supported on PHYP"));

	/* XXX: optimization using H_PROTECT for common case? */
	mphyp_pte_unset(mmu, slot, pvo_pt, vpn);
	result = phyp_pft_hcall(H_ENTER, H_EXACT, slot, pvo_pt->pte_hi,
				pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result != H_SUCCESS)
		panic("mphyp_pte_change() insertion failure: %ld\n", result);
}

static __inline int
mphyp_pte_spillable_ident(u_int ptegidx, struct lpte *to_evict)
{
	uint64_t slot, junk, k;
	struct lpte pt;
	int     i, j;

	/* Start at a random slot */
	i = mftb() % 8;
	k = -1;
	for (j = 0; j < 8; j++) {
		slot = (ptegidx << 3) + (i + j) % 8;
		phyp_pft_hcall(H_READ, 0, slot, 0, 0, &pt.pte_hi, &pt.pte_lo,
		    &junk);
		
		if (pt.pte_hi & LPTE_SWBITS)
			continue;

		/* This is a candidate, so remember it */
		k = slot;

		/* Try to get a page that has not been used lately */
		if (!(pt.pte_lo & LPTE_REF)) {
			memcpy(to_evict, &pt, sizeof(struct lpte));
			return (k);
		}
	}

	phyp_pft_hcall(H_READ, 0, slot, 0, 0, &to_evict->pte_hi,
	    &to_evict->pte_lo, &junk);
	return (k);
}

static int
mphyp_pte_insert(mmu_t mmu, u_int ptegidx, struct lpte *pvo_pt)
{
	int64_t result;
	struct lpte evicted;
	struct pvo_entry *pvo;
	uint64_t index, junk;
	u_int pteg_bktidx;

	/* Check for locked pages, which we can't support on this system */
	KASSERT(!(pvo_pt->pte_hi & LPTE_LOCKED),
	    ("Locked pages not supported on PHYP"));

	/* Initialize PTE */
	pvo_pt->pte_hi |= LPTE_VALID;
	pvo_pt->pte_hi &= ~LPTE_HID;
	evicted.pte_hi = 0;

	/*
	 * First try primary hash.
	 */
	pteg_bktidx = ptegidx;
	result = phyp_pft_hcall(H_ENTER, 0, pteg_bktidx << 3, pvo_pt->pte_hi,
	    pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS)
		return (index & 0x07);
	KASSERT(result == H_PTEG_FULL, ("Page insertion error: %ld "
	    "(ptegidx: %#x/%#x, PTE %#lx/%#lx", result, ptegidx,
	    moea64_pteg_count, pvo_pt->pte_hi, pvo_pt->pte_lo));

	/*
	 * Next try secondary hash.
	 */
	pteg_bktidx ^= moea64_pteg_mask;
	pvo_pt->pte_hi |= LPTE_HID;
	result = phyp_pft_hcall(H_ENTER, 0, pteg_bktidx << 3,
	    pvo_pt->pte_hi, pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS)
		return (index & 0x07);
	KASSERT(result == H_PTEG_FULL, ("Secondary page insertion error: %ld",
	    result));

	/*
	 * Out of luck. Find a PTE to sacrifice.
	 */
	pteg_bktidx = ptegidx;
	index = mphyp_pte_spillable_ident(pteg_bktidx, &evicted);
	if (index == -1L) {
		pteg_bktidx ^= moea64_pteg_mask;
		index = mphyp_pte_spillable_ident(pteg_bktidx, &evicted);
	}

	if (index == -1L) {
		/* No freeable slots in either PTEG? We're hosed. */
		panic("mphyp_pte_insert: overflow");
		return (-1);
	}

	if (pteg_bktidx == ptegidx)
                pvo_pt->pte_hi &= ~LPTE_HID;
        else
                pvo_pt->pte_hi |= LPTE_HID;

	/*
	 * Synchronize the sacrifice PTE with its PVO, then mark both
	 * invalid. The PVO will be reused when/if the VM system comes
	 * here after a fault.
	 */

	if (evicted.pte_hi & LPTE_HID)
		pteg_bktidx ^= moea64_pteg_mask; /* PTEs indexed by primary */

	LIST_FOREACH(pvo, &moea64_pvo_table[pteg_bktidx], pvo_olink) {
		if (pvo->pvo_pte.lpte.pte_hi == evicted.pte_hi) {
			KASSERT(pvo->pvo_pte.lpte.pte_hi & LPTE_VALID,
			    ("Invalid PVO for valid PTE!"));
			mphyp_pte_unset(mmu, index, &pvo->pvo_pte.lpte,
			    pvo->pvo_vpn);
			PVO_PTEGIDX_CLR(pvo);
			moea64_pte_overflow++;
			break;
		}
	}

	KASSERT(pvo->pvo_pte.lpte.pte_hi == evicted.pte_hi,
	   ("Unable to find PVO for spilled PTE"));

	/*
	 * Set the new PTE.
	 */
	result = phyp_pft_hcall(H_ENTER, H_EXACT, index, pvo_pt->pte_hi,
	    pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS)
		return (index & 0x07);

	panic("Page replacement error: %ld", result);
	return (-1);
}

static __inline u_int
va_to_pteg(uint64_t vsid, vm_offset_t addr, int large)
{
	uint64_t hash;
	int shift;

	shift = large ? moea64_large_page_shift : ADDR_PIDX_SHFT;
	hash = (vsid & VSID_HASH_MASK) ^ (((uint64_t)addr & ADDR_PIDX) >>
	    shift);
	return (hash & moea64_pteg_mask);
}

static uintptr_t
mphyp_pvo_to_pte(mmu_t mmu, const struct pvo_entry *pvo)
{
	uint64_t vsid;
	u_int ptegidx;

	/* If the PTEG index is not set, then there is no page table entry */
	if (!PVO_PTEGIDX_ISSET(pvo))
		return (-1);

	vsid = PVO_VSID(pvo);
	ptegidx = va_to_pteg(vsid, PVO_VADDR(pvo), pvo->pvo_vaddr & PVO_LARGE);

	/*
	 * We can find the actual pte entry without searching by grabbing
	 * the PTEG index from 3 unused bits in pvo_vaddr and by
	 * noticing the HID bit.
	 */
	if (pvo->pvo_pte.lpte.pte_hi & LPTE_HID)
		ptegidx ^= moea64_pteg_mask;

	return ((ptegidx << 3) | PVO_PTEGIDX_GET(pvo));
}

