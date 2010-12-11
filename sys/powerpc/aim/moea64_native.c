/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com> of Allegro Networks, Inc.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: pmap.c,v 1.28 2000/03/26 20:42:36 kleink Exp $
 */
/*-
 * Copyright (C) 2001 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
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

/*
 * Native 64-bit page table operations for running without a hypervisor.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <sys/kdb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>

#include <machine/md_var.h>
#include <machine/mmuvar.h>

#include "mmu_oea64.h"
#include "mmu_if.h"
#include "moea64_if.h"

#define	PTESYNC()	__asm __volatile("ptesync");
#define	TLBSYNC()	__asm __volatile("tlbsync; ptesync");
#define	SYNC()		__asm __volatile("sync");
#define	EIEIO()		__asm __volatile("eieio");

#define	VSID_HASH_MASK	0x0000007fffffffffULL

/*
 * The tlbie instruction must be executed in 64-bit mode
 * so we have to twiddle MSR[SF] around every invocation.
 * Just to add to the fun, exceptions must be off as well
 * so that we can't trap in 64-bit mode. What a pain.
 */
struct mtx	tlbie_mutex;

static __inline void
TLBIE(uint64_t vpn) {
#ifndef __powerpc64__
	register_t vpn_hi, vpn_lo;
	register_t msr;
	register_t scratch;
#endif

	vpn <<= ADDR_PIDX_SHFT;
	vpn &= ~(0xffffULL << 48);

	mtx_lock_spin(&tlbie_mutex);
#ifdef __powerpc64__
	__asm __volatile("\
	    ptesync; \
	    tlbie %0; \
	    eieio; \
	    tlbsync; \
	    ptesync;" 
	:: "r"(vpn) : "memory");
#else
	vpn_hi = (uint32_t)(vpn >> 32);
	vpn_lo = (uint32_t)vpn;

	__asm __volatile("\
	    mfmsr %0; \
	    mr %1, %0; \
	    insrdi %1,%5,1,0; \
	    mtmsrd %1; isync; \
	    ptesync; \
	    \
	    sld %1,%2,%4; \
	    or %1,%1,%3; \
	    tlbie %1; \
	    \
	    mtmsrd %0; isync; \
	    eieio; \
	    tlbsync; \
	    ptesync;" 
	: "=r"(msr), "=r"(scratch) : "r"(vpn_hi), "r"(vpn_lo), "r"(32), "r"(1)
	    : "memory");
#endif
	mtx_unlock_spin(&tlbie_mutex);
}

#define DISABLE_TRANS(msr)	msr = mfmsr(); mtmsr(msr & ~PSL_DR); isync()
#define ENABLE_TRANS(msr)	mtmsr(msr); isync()

/*
 * PTEG data.
 */
static struct	lpteg *moea64_pteg_table;

/*
 * PTE calls.
 */
static int	moea64_pte_insert_native(mmu_t, u_int, struct lpte *);
static uintptr_t moea64_pvo_to_pte_native(mmu_t, const struct pvo_entry *);
static void	moea64_pte_synch_native(mmu_t, uintptr_t pt,
		    struct lpte *pvo_pt);
static void	moea64_pte_clear_native(mmu_t, uintptr_t pt,
		    struct lpte *pvo_pt, uint64_t vpn, uint64_t ptebit);
static void	moea64_pte_change_native(mmu_t, uintptr_t pt,
		    struct lpte *pvo_pt, uint64_t vpn);
static void	moea64_pte_unset_native(mmu_t mmu, uintptr_t pt,
		    struct lpte *pvo_pt, uint64_t vpn);

/*
 * Utility routines.
 */
static void		moea64_bootstrap_native(mmu_t mmup, 
			    vm_offset_t kernelstart, vm_offset_t kernelend);
static void		moea64_cpu_bootstrap_native(mmu_t, int ap);
static void		tlbia(void);

static mmu_method_t moea64_native_methods[] = {
	/* Internal interfaces */
	MMUMETHOD(mmu_bootstrap,	moea64_bootstrap_native),
	MMUMETHOD(mmu_cpu_bootstrap,	moea64_cpu_bootstrap_native),

	MMUMETHOD(moea64_pte_synch,	moea64_pte_synch_native),
	MMUMETHOD(moea64_pte_clear,	moea64_pte_clear_native),	
	MMUMETHOD(moea64_pte_unset,	moea64_pte_unset_native),	
	MMUMETHOD(moea64_pte_change,	moea64_pte_change_native),	
	MMUMETHOD(moea64_pte_insert,	moea64_pte_insert_native),	
	MMUMETHOD(moea64_pvo_to_pte,	moea64_pvo_to_pte_native),	

	{ 0, 0 }
};

MMU_DEF_INHERIT(oea64_mmu_native, MMU_TYPE_G5, moea64_native_methods,
    0, oea64_mmu);

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

static void
moea64_pte_synch_native(mmu_t mmu, uintptr_t pt_cookie, struct lpte *pvo_pt)
{
	struct lpte *pt = (struct lpte *)pt_cookie;

	pvo_pt->pte_lo |= pt->pte_lo & (LPTE_REF | LPTE_CHG);
}

static void
moea64_pte_clear_native(mmu_t mmu, uintptr_t pt_cookie, struct lpte *pvo_pt,
    uint64_t vpn, uint64_t ptebit)
{
	struct lpte *pt = (struct lpte *)pt_cookie;

	/*
	 * As shown in Section 7.6.3.2.3
	 */
	pt->pte_lo &= ~ptebit;
	TLBIE(vpn);
}

static void
moea64_pte_set_native(struct lpte *pt, struct lpte *pvo_pt)
{

	pvo_pt->pte_hi |= LPTE_VALID;

	/*
	 * Update the PTE as defined in section 7.6.3.1.
	 * Note that the REF/CHG bits are from pvo_pt and thus should have
	 * been saved so this routine can restore them (if desired).
	 */
	pt->pte_lo = pvo_pt->pte_lo;
	EIEIO();
	pt->pte_hi = pvo_pt->pte_hi;
	PTESYNC();
	moea64_pte_valid++;
}

static void
moea64_pte_unset_native(mmu_t mmu, uintptr_t pt_cookie, struct lpte *pvo_pt,
    uint64_t vpn)
{
	struct lpte *pt = (struct lpte *)pt_cookie;

	pvo_pt->pte_hi &= ~LPTE_VALID;

	/* Finish all pending operations */
	isync();

	/*
	 * Force the reg & chg bits back into the PTEs.
	 */
	SYNC();

	/*
	 * Invalidate the pte.
	 */
	pt->pte_hi &= ~LPTE_VALID;
	TLBIE(vpn);

	/*
	 * Save the reg & chg bits.
	 */
	moea64_pte_synch_native(mmu, pt_cookie, pvo_pt);
	moea64_pte_valid--;
}

static void
moea64_pte_change_native(mmu_t mmu, uintptr_t pt, struct lpte *pvo_pt,
    uint64_t vpn)
{

	/*
	 * Invalidate the PTE
	 */
	moea64_pte_unset_native(mmu, pt, pvo_pt, vpn);
	moea64_pte_set_native((struct lpte *)pt, pvo_pt);
}

static void
moea64_cpu_bootstrap_native(mmu_t mmup, int ap)
{
	int i = 0;
	#ifdef __powerpc64__
	struct slb *slb = PCPU_GET(slb);
	register_t seg0;
	#endif

	/*
	 * Initialize segment registers and MMU
	 */

	mtmsr(mfmsr() & ~PSL_DR & ~PSL_IR); isync();

	/*
	 * Install kernel SLB entries
	 */

	#ifdef __powerpc64__
		__asm __volatile ("slbia");
		__asm __volatile ("slbmfee %0,%1; slbie %0;" : "=r"(seg0) :
		    "r"(0));

		for (i = 0; i < 64; i++) {
			if (!(slb[i].slbe & SLBE_VALID))
				continue;

			__asm __volatile ("slbmte %0, %1" :: 
			    "r"(slb[i].slbv), "r"(slb[i].slbe)); 
		}
	#else
		for (i = 0; i < 16; i++)
			mtsrin(i << ADDR_SR_SHFT, kernel_pmap->pm_sr[i]);
	#endif

	/*
	 * Install page table
	 */

	__asm __volatile ("ptesync; mtsdr1 %0; isync"
	    :: "r"((uintptr_t)moea64_pteg_table 
		     | (uintptr_t)(flsl(moea64_pteg_mask >> 11))));
	tlbia();
}

static void
moea64_bootstrap_native(mmu_t mmup, vm_offset_t kernelstart,
    vm_offset_t kernelend)
{
	vm_size_t	size;
	vm_offset_t	off;
	vm_paddr_t	pa;
	register_t	msr;

	moea64_early_bootstrap(mmup, kernelstart, kernelend);

	/*
	 * Allocate PTEG table.
	 */

	size = moea64_pteg_count * sizeof(struct lpteg);
	CTR2(KTR_PMAP, "moea64_bootstrap: %d PTEGs, %d bytes", 
	    moea64_pteg_count, size);

	/*
	 * We now need to allocate memory. This memory, to be allocated,
	 * has to reside in a page table. The page table we are about to
	 * allocate. We don't have BAT. So drop to data real mode for a minute
	 * as a measure of last resort. We do this a couple times.
	 */

	moea64_pteg_table = (struct lpteg *)moea64_bootstrap_alloc(size, size);
	DISABLE_TRANS(msr);
	bzero((void *)moea64_pteg_table, moea64_pteg_count * sizeof(struct lpteg));
	ENABLE_TRANS(msr);

	CTR1(KTR_PMAP, "moea64_bootstrap: PTEG table at %p", moea64_pteg_table);

	/*
	 * Initialize the TLBIE lock. TLBIE can only be executed by one CPU.
	 */
	mtx_init(&tlbie_mutex, "tlbie mutex", NULL, MTX_SPIN);

	moea64_mid_bootstrap(mmup, kernelstart, kernelend);

	/*
	 * Add a mapping for the page table itself if there is no direct map.
	 */
	if (!hw_direct_map) {
		size = moea64_pteg_count * sizeof(struct lpteg);
		off = (vm_offset_t)(moea64_pteg_table);
		DISABLE_TRANS(msr);
		for (pa = off; pa < off + size; pa += PAGE_SIZE)
			pmap_kenter(pa, pa);
		ENABLE_TRANS(msr);
	}

	/* Bring up virtual memory */
	moea64_late_bootstrap(mmup, kernelstart, kernelend);
}

static void
tlbia(void)
{
	vm_offset_t i;
	#ifndef __powerpc64__
	register_t msr, scratch;
	#endif

	TLBSYNC();

	for (i = 0; i < 0xFF000; i += 0x00001000) {
		#ifdef __powerpc64__
		__asm __volatile("tlbiel %0" :: "r"(i));
		#else
		__asm __volatile("\
		    mfmsr %0; \
		    mr %1, %0; \
		    insrdi %1,%3,1,0; \
		    mtmsrd %1; \
		    isync; \
		    \
		    tlbiel %2; \
		    \
		    mtmsrd %0; \
		    isync;" 
		: "=r"(msr), "=r"(scratch) : "r"(i), "r"(1));
		#endif
	}

	EIEIO();
	TLBSYNC();
}

static uintptr_t
moea64_pvo_to_pte_native(mmu_t mmu, const struct pvo_entry *pvo)
{
	struct lpte 	*pt;
	int		pteidx, ptegidx;
	uint64_t	vsid;

	/* If the PTEG index is not set, then there is no page table entry */
	if (!PVO_PTEGIDX_ISSET(pvo))
		return (-1);

	/*
	 * Calculate the ptegidx
	 */
	vsid = PVO_VSID(pvo);
	ptegidx = va_to_pteg(vsid, PVO_VADDR(pvo),
	    pvo->pvo_vaddr & PVO_LARGE);

	/*
	 * We can find the actual pte entry without searching by grabbing
	 * the PTEG index from 3 unused bits in pvo_vaddr and by
	 * noticing the HID bit.
	 */
	if (pvo->pvo_pte.lpte.pte_hi & LPTE_HID)
		ptegidx ^= moea64_pteg_mask;

	pteidx = (ptegidx << 3) | PVO_PTEGIDX_GET(pvo);

	if ((pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) && 
	    !PVO_PTEGIDX_ISSET(pvo)) {
		panic("moea64_pvo_to_pte: pvo %p has valid pte in pvo but no "
		    "valid pte index", pvo);
	}

	if ((pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) == 0 && 
	    PVO_PTEGIDX_ISSET(pvo)) {
		panic("moea64_pvo_to_pte: pvo %p has valid pte index in pvo "
		    "pvo but no valid pte", pvo);
	}

	pt = &moea64_pteg_table[pteidx >> 3].pt[pteidx & 7];
	if ((pt->pte_hi ^ (pvo->pvo_pte.lpte.pte_hi & ~LPTE_VALID)) == 
	    LPTE_VALID) {
		if ((pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) == 0) {
			panic("moea64_pvo_to_pte: pvo %p has valid pte in "
			    "moea64_pteg_table %p but invalid in pvo", pvo, pt);
		}

		if (((pt->pte_lo ^ pvo->pvo_pte.lpte.pte_lo) & 
		    ~(LPTE_M|LPTE_CHG|LPTE_REF)) != 0) {
			panic("moea64_pvo_to_pte: pvo %p pte does not match "
			    "pte %p in moea64_pteg_table difference is %#x", 
			    pvo, pt,
			    (uint32_t)(pt->pte_lo ^ pvo->pvo_pte.lpte.pte_lo));
		}

		return ((uintptr_t)pt);
	}

	if (pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) {
		panic("moea64_pvo_to_pte: pvo %p has invalid pte %p in "
		    "moea64_pteg_table but valid in pvo", pvo, pt);
	}

	return (-1);
}

static __inline int
moea64_pte_spillable_ident(u_int ptegidx)
{
	struct	lpte *pt;
	int	i, j, k;

	/* Start at a random slot */
	i = mftb() % 8;
	k = -1;
	for (j = 0; j < 8; j++) {
		pt = &moea64_pteg_table[ptegidx].pt[(i + j) % 8];
		if (pt->pte_hi & (LPTE_LOCKED | LPTE_WIRED))
			continue;

		/* This is a candidate, so remember it */
		k = (i + j) % 8;

		/* Try to get a page that has not been used lately */
		if (!(pt->pte_lo & LPTE_REF))
			return (k);
	}
	
	return (k);
}

static int
moea64_pte_insert_native(mmu_t mmu, u_int ptegidx, struct lpte *pvo_pt)
{
	struct	lpte *pt;
	struct	pvo_entry *pvo;
	u_int	pteg_bktidx;
	int	i;

	/*
	 * First try primary hash.
	 */
	pteg_bktidx = ptegidx;
	for (pt = moea64_pteg_table[pteg_bktidx].pt, i = 0; i < 8; i++, pt++) {
		if ((pt->pte_hi & (LPTE_VALID | LPTE_LOCKED)) == 0) {
			pvo_pt->pte_hi &= ~LPTE_HID;
			moea64_pte_set_native(pt, pvo_pt);
			return (i);
		}
	}

	/*
	 * Now try secondary hash.
	 */
	pteg_bktidx ^= moea64_pteg_mask;
	for (pt = moea64_pteg_table[pteg_bktidx].pt, i = 0; i < 8; i++, pt++) {
		if ((pt->pte_hi & (LPTE_VALID | LPTE_LOCKED)) == 0) {
			pvo_pt->pte_hi |= LPTE_HID;
			moea64_pte_set_native(pt, pvo_pt);
			return (i);
		}
	}

	/*
	 * Out of luck. Find a PTE to sacrifice.
	 */
	pteg_bktidx = ptegidx;
	i = moea64_pte_spillable_ident(pteg_bktidx);
	if (i < 0) {
		pteg_bktidx ^= moea64_pteg_mask;
		i = moea64_pte_spillable_ident(pteg_bktidx);
	}

	if (i < 0) {
		/* No freeable slots in either PTEG? We're hosed. */
		panic("moea64_pte_insert: overflow");
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
	pt = &moea64_pteg_table[pteg_bktidx].pt[i];

	if (pt->pte_hi & LPTE_HID)
		pteg_bktidx ^= moea64_pteg_mask; /* PTEs indexed by primary */

	LIST_FOREACH(pvo, &moea64_pvo_table[pteg_bktidx], pvo_olink) {
		if (pvo->pvo_pte.lpte.pte_hi == pt->pte_hi) {
			KASSERT(pvo->pvo_pte.lpte.pte_hi & LPTE_VALID, 
			    ("Invalid PVO for valid PTE!"));
			moea64_pte_unset_native(mmu, (uintptr_t)pt,
			    &pvo->pvo_pte.lpte, pvo->pvo_vpn);
			PVO_PTEGIDX_CLR(pvo);
			moea64_pte_overflow++;
			break;
		}
	}

	KASSERT(pvo->pvo_pte.lpte.pte_hi == pt->pte_hi,
	   ("Unable to find PVO for spilled PTE"));

	/*
	 * Set the new PTE.
	 */
	moea64_pte_set_native(pt, pvo_pt);

	return (i);
}

