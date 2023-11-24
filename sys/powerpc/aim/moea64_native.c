/*-
 * SPDX-License-Identifier: BSD-2-Clause AND BSD-4-Clause
 *
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
/*
 * Native 64-bit page table operations for running without a hypervisor.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/rwlock.h>
#include <sys/endian.h>

#include <sys/kdb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>

#include <machine/cpu.h>
#include <machine/hid.h>
#include <machine/md_var.h>
#include <machine/mmuvar.h>

#include "mmu_oea64.h"

#define	PTESYNC()	__asm __volatile("ptesync");
#define	TLBSYNC()	__asm __volatile("tlbsync; ptesync");
#define	SYNC()		__asm __volatile("sync");
#define	EIEIO()		__asm __volatile("eieio");

#define	VSID_HASH_MASK	0x0000007fffffffffULL

/* POWER9 only permits a 64k partition table size. */
#define	PART_SIZE	0x10000

/* Actual page sizes (to be used with tlbie, when L=0) */
#define	AP_4K		0x00
#define	AP_16M		0x80

#define	LPTE_KERNEL_VSID_BIT	(KERNEL_VSID_BIT << \
				(16 - (ADDR_API_SHFT64 - ADDR_PIDX_SHFT)))

/* Abbreviated Virtual Address Page - high bits */
#define	LPTE_AVA_PGNHI_MASK	0x0000000000000F80ULL
#define	LPTE_AVA_PGNHI_SHIFT	7

/* Effective Address Page - low bits */
#define	EA_PAGELO_MASK		0x7ffULL
#define	EA_PAGELO_SHIFT		11

static bool moea64_crop_tlbie;
static bool moea64_need_lock;

/*
 * The tlbie instruction has two forms: an old one used by PowerISA
 * 2.03 and prior, and a newer one used by PowerISA 2.06 and later.
 * We need to support both.
 */
static __inline void
TLBIE(uint64_t vpn, uint64_t oldptehi)
{
#ifndef __powerpc64__
	register_t vpn_hi, vpn_lo;
	register_t msr;
	register_t scratch, intr;
#endif

	static volatile u_int tlbie_lock = 0;
	bool need_lock = moea64_need_lock;

	vpn <<= ADDR_PIDX_SHFT;

	/* Hobo spinlock: we need stronger guarantees than mutexes provide */
	if (need_lock) {
		while (!atomic_cmpset_int(&tlbie_lock, 0, 1));
		isync(); /* Flush instruction queue once lock acquired */

		if (moea64_crop_tlbie) {
			vpn &= ~(0xffffULL << 48);
#ifdef __powerpc64__
			if ((oldptehi & LPTE_BIG) != 0)
				__asm __volatile("tlbie %0, 1" :: "r"(vpn) :
				    "memory");
			else
				__asm __volatile("tlbie %0, 0" :: "r"(vpn) :
				    "memory");
			__asm __volatile("eieio; tlbsync; ptesync" :::
			    "memory");
			goto done;
#endif
		}
	}

#ifdef __powerpc64__
	/*
	 * If this page has LPTE_BIG set and is from userspace, then
	 * it must be a superpage with 4KB base/16MB actual page size.
	 */
	if ((oldptehi & LPTE_BIG) != 0 &&
	    (oldptehi & LPTE_KERNEL_VSID_BIT) == 0)
		vpn |= AP_16M;

	/*
	 * Explicitly clobber r0.  The tlbie instruction has two forms: an old
	 * one used by PowerISA 2.03 and prior, and a newer one used by PowerISA
	 * 2.06 (maybe 2.05?) and later.  We need to support both, and it just
	 * so happens that since we use 4k pages we can simply zero out r0, and
	 * clobber it, and the assembler will interpret the single-operand form
	 * of tlbie as having RB set, and everything else as 0.  The RS operand
	 * in the newer form is in the same position as the L(page size) bit of
	 * the old form, so a slong as RS is 0, we're good on both sides.
	 */
	__asm __volatile("li 0, 0 \n tlbie %0, 0" :: "r"(vpn) : "r0", "memory");
	__asm __volatile("eieio; tlbsync; ptesync" ::: "memory");
done:

#else
	vpn_hi = (uint32_t)(vpn >> 32);
	vpn_lo = (uint32_t)vpn;

	intr = intr_disable();
	__asm __volatile("\
	    mfmsr %0; \
	    mr %1, %0; \
	    insrdi %1,%5,1,0; \
	    mtmsrd %1; isync; \
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
	intr_restore(intr);
#endif

	/* No barriers or special ops -- taken care of by ptesync above */
	if (need_lock)
		tlbie_lock = 0;
}

#define DISABLE_TRANS(msr)	msr = mfmsr(); mtmsr(msr & ~PSL_DR)
#define ENABLE_TRANS(msr)	mtmsr(msr)

/*
 * PTEG data.
 */
static volatile struct lpte *moea64_pteg_table;
static struct rwlock moea64_eviction_lock;

static volatile struct pate *moea64_part_table;

/*
 * Dump function.
 */
static void	*moea64_dump_pmap_native(void *ctx, void *buf,
		    u_long *nbytes);

/*
 * PTE calls.
 */
static int64_t	moea64_pte_insert_native(struct pvo_entry *);
static int64_t	moea64_pte_synch_native(struct pvo_entry *);
static int64_t	moea64_pte_clear_native(struct pvo_entry *, uint64_t);
static int64_t	moea64_pte_replace_native(struct pvo_entry *, int);
static int64_t	moea64_pte_unset_native(struct pvo_entry *);
static int64_t	moea64_pte_insert_sp_native(struct pvo_entry *);
static int64_t	moea64_pte_unset_sp_native(struct pvo_entry *);
static int64_t	moea64_pte_replace_sp_native(struct pvo_entry *);

/*
 * Utility routines.
 */
static void	moea64_bootstrap_native(
		    vm_offset_t kernelstart, vm_offset_t kernelend);
static void	moea64_cpu_bootstrap_native(int ap);
static void	tlbia(void);
static void	moea64_install_native(void);

static struct pmap_funcs moea64_native_methods = {
	.install = moea64_install_native,

	/* Internal interfaces */
	.bootstrap = moea64_bootstrap_native,
	.cpu_bootstrap = moea64_cpu_bootstrap_native,
        .dumpsys_dump_pmap =         moea64_dump_pmap_native,
};

static struct moea64_funcs moea64_native_funcs = {
	.pte_synch = moea64_pte_synch_native,
	.pte_clear = moea64_pte_clear_native,
	.pte_unset = moea64_pte_unset_native,
	.pte_replace = moea64_pte_replace_native,
	.pte_insert = moea64_pte_insert_native,
	.pte_insert_sp = moea64_pte_insert_sp_native,
	.pte_unset_sp = moea64_pte_unset_sp_native,
	.pte_replace_sp = moea64_pte_replace_sp_native,
};

MMU_DEF_INHERIT(oea64_mmu_native, MMU_TYPE_G5, moea64_native_methods, oea64_mmu);

static void
moea64_install_native(void)
{

	/* Install the MOEA64 ops. */
	moea64_ops = &moea64_native_funcs;

	moea64_install();
}

static int64_t
moea64_pte_synch_native(struct pvo_entry *pvo)
{
	volatile struct lpte *pt = moea64_pteg_table + pvo->pvo_pte.slot;
	uint64_t ptelo, pvo_ptevpn;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);

	pvo_ptevpn = moea64_pte_vpn_from_pvo_vpn(pvo);

	rw_rlock(&moea64_eviction_lock);
	if ((be64toh(pt->pte_hi) & LPTE_AVPN_MASK) != pvo_ptevpn) {
		/* Evicted */
		rw_runlock(&moea64_eviction_lock);
		return (-1);
	}
		
	PTESYNC();
	ptelo = be64toh(pt->pte_lo);

	rw_runlock(&moea64_eviction_lock);

	return (ptelo & (LPTE_REF | LPTE_CHG));
}

static int64_t 
moea64_pte_clear_native(struct pvo_entry *pvo, uint64_t ptebit)
{
	volatile struct lpte *pt = moea64_pteg_table + pvo->pvo_pte.slot;
	struct lpte properpt;
	uint64_t ptelo;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);

	moea64_pte_from_pvo(pvo, &properpt);

	rw_rlock(&moea64_eviction_lock);
	if ((be64toh(pt->pte_hi) & LPTE_AVPN_MASK) !=
	    (properpt.pte_hi & LPTE_AVPN_MASK)) {
		/* Evicted */
		rw_runlock(&moea64_eviction_lock);
		return (-1);
	}

	if (ptebit == LPTE_REF) {
		/* See "Resetting the Reference Bit" in arch manual */
		PTESYNC();
		/* 2-step here safe: precision is not guaranteed */
		ptelo = be64toh(pt->pte_lo);

		/* One-byte store to avoid touching the C bit */
		((volatile uint8_t *)(&pt->pte_lo))[6] =
#if BYTE_ORDER == BIG_ENDIAN
		    ((uint8_t *)(&properpt.pte_lo))[6];
#else
		    ((uint8_t *)(&properpt.pte_lo))[1];
#endif
		rw_runlock(&moea64_eviction_lock);

		critical_enter();
		TLBIE(pvo->pvo_vpn, properpt.pte_hi);
		critical_exit();
	} else {
		rw_runlock(&moea64_eviction_lock);
		ptelo = moea64_pte_unset_native(pvo);
		moea64_pte_insert_native(pvo);
	}

	return (ptelo & (LPTE_REF | LPTE_CHG));
}

static __always_inline int64_t
moea64_pte_unset_locked(volatile struct lpte *pt, uint64_t vpn)
{
	uint64_t ptelo, ptehi;

	/*
	 * Invalidate the pte, briefly locking it to collect RC bits. No
	 * atomics needed since this is protected against eviction by the lock.
	 */
	isync();
	critical_enter();
	ptehi = (be64toh(pt->pte_hi) & ~LPTE_VALID) | LPTE_LOCKED;
	pt->pte_hi = htobe64(ptehi);
	PTESYNC();
	TLBIE(vpn, ptehi);
	ptelo = be64toh(pt->pte_lo);
	*((volatile int32_t *)(&pt->pte_hi) + 1) = 0; /* Release lock */
	critical_exit();

	/* Keep statistics */
	STAT_MOEA64(moea64_pte_valid--);

	return (ptelo & (LPTE_CHG | LPTE_REF));
}

static int64_t
moea64_pte_unset_native(struct pvo_entry *pvo)
{
	volatile struct lpte *pt = moea64_pteg_table + pvo->pvo_pte.slot;
	int64_t ret;
	uint64_t pvo_ptevpn;

	pvo_ptevpn = moea64_pte_vpn_from_pvo_vpn(pvo);

	rw_rlock(&moea64_eviction_lock);

	if ((be64toh(pt->pte_hi) & LPTE_AVPN_MASK) != pvo_ptevpn) {
		/* Evicted */
		STAT_MOEA64(moea64_pte_overflow--);
		ret = -1;
	} else
		ret = moea64_pte_unset_locked(pt, pvo->pvo_vpn);

	rw_runlock(&moea64_eviction_lock);

	return (ret);
}

static int64_t
moea64_pte_replace_inval_native(struct pvo_entry *pvo,
    volatile struct lpte *pt)
{
	struct lpte properpt;
	uint64_t ptelo, ptehi;

	moea64_pte_from_pvo(pvo, &properpt);

	rw_rlock(&moea64_eviction_lock);
	if ((be64toh(pt->pte_hi) & LPTE_AVPN_MASK) !=
	    (properpt.pte_hi & LPTE_AVPN_MASK)) {
		/* Evicted */
		STAT_MOEA64(moea64_pte_overflow--);
		rw_runlock(&moea64_eviction_lock);
		return (-1);
	}

	/*
	 * Replace the pte, briefly locking it to collect RC bits. No
	 * atomics needed since this is protected against eviction by the lock.
	 */
	isync();
	critical_enter();
	ptehi = (be64toh(pt->pte_hi) & ~LPTE_VALID) | LPTE_LOCKED;
	pt->pte_hi = htobe64(ptehi);
	PTESYNC();
	TLBIE(pvo->pvo_vpn, ptehi);
	ptelo = be64toh(pt->pte_lo);
	EIEIO();
	pt->pte_lo = htobe64(properpt.pte_lo);
	EIEIO();
	pt->pte_hi = htobe64(properpt.pte_hi); /* Release lock */
	PTESYNC();
	critical_exit();
	rw_runlock(&moea64_eviction_lock);

	return (ptelo & (LPTE_CHG | LPTE_REF));
}

static int64_t
moea64_pte_replace_native(struct pvo_entry *pvo, int flags)
{
	volatile struct lpte *pt = moea64_pteg_table + pvo->pvo_pte.slot;
	struct lpte properpt;
	int64_t ptelo;

	if (flags == 0) {
		/* Just some software bits changing. */
		moea64_pte_from_pvo(pvo, &properpt);

		rw_rlock(&moea64_eviction_lock);
		if ((be64toh(pt->pte_hi) & LPTE_AVPN_MASK) !=
		    (properpt.pte_hi & LPTE_AVPN_MASK)) {
			rw_runlock(&moea64_eviction_lock);
			return (-1);
		}
		pt->pte_hi = htobe64(properpt.pte_hi);
		ptelo = be64toh(pt->pte_lo);
		rw_runlock(&moea64_eviction_lock);
	} else {
		/* Otherwise, need reinsertion and deletion */
		ptelo = moea64_pte_replace_inval_native(pvo, pt);
	}

	return (ptelo);
}

static void
moea64_cpu_bootstrap_native(int ap)
{
	int i = 0;
	#ifdef __powerpc64__
	struct slb *slb = PCPU_GET(aim.slb);
	register_t seg0;
	#endif

	/*
	 * Initialize segment registers and MMU
	 */

	mtmsr(mfmsr() & ~PSL_DR & ~PSL_IR);

	switch(mfpvr() >> 16) {
	case IBMPOWER9:
		mtspr(SPR_HID0, mfspr(SPR_HID0) & ~HID0_RADIX);
		break;
	}

	/*
	 * Install kernel SLB entries
	 */

	#ifdef __powerpc64__
		__asm __volatile ("slbia");
		__asm __volatile ("slbmfee %0,%1; slbie %0;" : "=r"(seg0) :
		    "r"(0));

		for (i = 0; i < n_slbs; i++) {
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

	if (cpu_features2 & PPC_FEATURE2_ARCH_3_00)
		mtspr(SPR_PTCR,
		    ((uintptr_t)moea64_part_table & ~DMAP_BASE_ADDRESS) |
		     flsl((PART_SIZE >> 12) - 1));
	else
		__asm __volatile ("ptesync; mtsdr1 %0; isync"
		    :: "r"(((uintptr_t)moea64_pteg_table & ~DMAP_BASE_ADDRESS)
			     | (uintptr_t)(flsl(moea64_pteg_mask >> 11))));
	tlbia();
}

static void
moea64_bootstrap_native(vm_offset_t kernelstart, vm_offset_t kernelend)
{
	vm_size_t	size;
	vm_offset_t	off;
	vm_paddr_t	pa;
	register_t	msr;

	moea64_early_bootstrap(kernelstart, kernelend);

	switch (mfpvr() >> 16) {
	case IBMPOWER9:
		moea64_need_lock = false;
		break;
	case IBMPOWER4:
	case IBMPOWER4PLUS:
	case IBM970:
	case IBM970FX:
	case IBM970GX:
	case IBM970MP:
		moea64_crop_tlbie = true;
	default:
		moea64_need_lock = true;
	}
	/*
	 * Allocate PTEG table.
	 */

	size = moea64_pteg_count * sizeof(struct lpteg);
	CTR2(KTR_PMAP, "moea64_bootstrap: %lu PTEGs, %lu bytes", 
	    moea64_pteg_count, size);
	rw_init(&moea64_eviction_lock, "pte eviction");

	/*
	 * We now need to allocate memory. This memory, to be allocated,
	 * has to reside in a page table. The page table we are about to
	 * allocate. We don't have BAT. So drop to data real mode for a minute
	 * as a measure of last resort. We do this a couple times.
	 */
	/*
	 * PTEG table must be aligned on a 256k boundary, but can be placed
	 * anywhere with that alignment on POWER ISA 3+ systems. On earlier
	 * systems, offset addition is done by the CPU with bitwise OR rather
	 * than addition, so the table must also be aligned on a boundary of
	 * its own size. Pick the larger of the two, which works on all
	 * systems.
	 */
	moea64_pteg_table = (struct lpte *)moea64_bootstrap_alloc(size, 
	    MAX(256*1024, size));
	if (hw_direct_map)
		moea64_pteg_table =
		    (struct lpte *)PHYS_TO_DMAP((vm_offset_t)moea64_pteg_table);
	/* Allocate partition table (ISA 3.0). */
	if (cpu_features2 & PPC_FEATURE2_ARCH_3_00) {
		moea64_part_table =
		    (struct pate *)moea64_bootstrap_alloc(PART_SIZE, PART_SIZE);
		moea64_part_table =
		    (struct pate *)PHYS_TO_DMAP((vm_offset_t)moea64_part_table);
	}
	DISABLE_TRANS(msr);
	bzero(__DEVOLATILE(void *, moea64_pteg_table), moea64_pteg_count *
	    sizeof(struct lpteg));
	if (cpu_features2 & PPC_FEATURE2_ARCH_3_00) {
		bzero(__DEVOLATILE(void *, moea64_part_table), PART_SIZE);
		moea64_part_table[0].pagetab = htobe64(
			(DMAP_TO_PHYS((vm_offset_t)moea64_pteg_table)) |
			(uintptr_t)(flsl((moea64_pteg_count - 1) >> 11)));
	}
	ENABLE_TRANS(msr);

	CTR1(KTR_PMAP, "moea64_bootstrap: PTEG table at %p", moea64_pteg_table);

	moea64_mid_bootstrap(kernelstart, kernelend);

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
	moea64_late_bootstrap(kernelstart, kernelend);
}

static void
tlbia(void)
{
	vm_offset_t i;
	#ifndef __powerpc64__
	register_t msr, scratch;
	#endif

	i = 0xc00; /* IS = 11 */
	switch (mfpvr() >> 16) {
	case IBM970:
	case IBM970FX:
	case IBM970MP:
	case IBM970GX:
	case IBMPOWER4:
	case IBMPOWER4PLUS:
	case IBMPOWER5:
	case IBMPOWER5PLUS:
		i = 0; /* IS not supported */
		break;
	}

	TLBSYNC();

	for (; i < 0x400000; i += 0x00001000) {
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

static int
atomic_pte_lock(volatile struct lpte *pte, uint64_t bitmask, uint64_t *oldhi)
{
	int	ret;
#ifdef __powerpc64__
	uint64_t temp;
#else
	uint32_t oldhihalf;
#endif

	/*
	 * Note: in principle, if just the locked bit were set here, we
	 * could avoid needing the eviction lock. However, eviction occurs
	 * so rarely that it isn't worth bothering about in practice.
	 */
#ifdef __powerpc64__
	/*
	 * Note: Success of this sequence has the side effect of invalidating
	 * the PTE, as we are setting it to LPTE_LOCKED and discarding the
	 * other bits, including LPTE_V.
	 */
	__asm __volatile (
		"1:\tldarx %1, 0, %3\n\t"	/* load old value */
		"and. %0,%1,%4\n\t"		/* check if any bits set */
		"bne 2f\n\t"			/* exit if any set */
		"stdcx. %5, 0, %3\n\t"		/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"li %0, 1\n\t"			/* success - retval = 1 */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stdcx. %1, 0, %3\n\t"       	/* clear reservation (74xx) */
		"li %0, 0\n\t"			/* failure - retval = 0 */
		"3:\n\t"
		: "=&r" (ret), "=&r"(temp), "=m" (pte->pte_hi)
		: "r" ((volatile char *)&pte->pte_hi),
		  "r" (htobe64(bitmask)), "r" (htobe64(LPTE_LOCKED)),
		  "m" (pte->pte_hi)
		: "cr0", "cr1", "cr2", "memory");
	*oldhi = be64toh(temp);
#else
	/*
	 * This code is used on bridge mode only.
	 */
	__asm __volatile (
		"1:\tlwarx %1, 0, %3\n\t"	/* load old value */
		"and. %0,%1,%4\n\t"		/* check if any bits set */
		"bne 2f\n\t"			/* exit if any set */
		"stwcx. %5, 0, %3\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"li %0, 1\n\t"			/* success - retval = 1 */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stwcx. %1, 0, %3\n\t"       	/* clear reservation (74xx) */
		"li %0, 0\n\t"			/* failure - retval = 0 */
		"3:\n\t"
		: "=&r" (ret), "=&r"(oldhihalf), "=m" (pte->pte_hi)
		: "r" ((volatile char *)&pte->pte_hi + 4),
		  "r" ((uint32_t)bitmask), "r" ((uint32_t)LPTE_LOCKED),
		  "m" (pte->pte_hi)
		: "cr0", "cr1", "cr2", "memory");

	*oldhi = (pte->pte_hi & 0xffffffff00000000ULL) | oldhihalf;
#endif

	return (ret);
}

static uintptr_t
moea64_insert_to_pteg_native(struct lpte *pvo_pt, uintptr_t slotbase,
    uint64_t mask)
{
	volatile struct lpte *pt;
	uint64_t oldptehi, va;
	uintptr_t k;
	int i, j;

	/* Start at a random slot */
	i = mftb() % 8;
	for (j = 0; j < 8; j++) {
		k = slotbase + (i + j) % 8;
		pt = &moea64_pteg_table[k];
		/* Invalidate and seize lock only if no bits in mask set */
		if (atomic_pte_lock(pt, mask, &oldptehi)) /* Lock obtained */
			break;
	}

	if (j == 8)
		return (-1);

	if (oldptehi & LPTE_VALID) {
		KASSERT(!(oldptehi & LPTE_WIRED), ("Unmapped wired entry"));
		/*
		 * Need to invalidate old entry completely: see
		 * "Modifying a Page Table Entry". Need to reconstruct
		 * the virtual address for the outgoing entry to do that.
		 */
		va = oldptehi >> (ADDR_SR_SHFT - ADDR_API_SHFT64);
		if (oldptehi & LPTE_HID)
			va = (((k >> 3) ^ moea64_pteg_mask) ^ va) &
			    (ADDR_PIDX >> ADDR_PIDX_SHFT);
		else
			va = ((k >> 3) ^ va) & (ADDR_PIDX >> ADDR_PIDX_SHFT);
		va |= (oldptehi & LPTE_AVPN_MASK) <<
		    (ADDR_API_SHFT64 - ADDR_PIDX_SHFT);
		PTESYNC();
		TLBIE(va, oldptehi);
		STAT_MOEA64(moea64_pte_valid--);
		STAT_MOEA64(moea64_pte_overflow++);
	}

	/*
	 * Update the PTE as per "Adding a Page Table Entry". Lock is released
	 * by setting the high doubleworld.
	 */
	pt->pte_lo = htobe64(pvo_pt->pte_lo);
	EIEIO();
	pt->pte_hi = htobe64(pvo_pt->pte_hi);
	PTESYNC();

	/* Keep statistics */
	STAT_MOEA64(moea64_pte_valid++);

	return (k);
}

static __always_inline int64_t
moea64_pte_insert_locked(struct pvo_entry *pvo, struct lpte *insertpt,
    uint64_t mask)
{
	uintptr_t slot;

	/*
	 * First try primary hash.
	 */
	slot = moea64_insert_to_pteg_native(insertpt, pvo->pvo_pte.slot,
	    mask | LPTE_WIRED | LPTE_LOCKED);
	if (slot != -1) {
		pvo->pvo_pte.slot = slot;
		return (0);
	}

	/*
	 * Now try secondary hash.
	 */
	pvo->pvo_vaddr ^= PVO_HID;
	insertpt->pte_hi ^= LPTE_HID;
	pvo->pvo_pte.slot ^= (moea64_pteg_mask << 3);
	slot = moea64_insert_to_pteg_native(insertpt, pvo->pvo_pte.slot,
	    mask | LPTE_WIRED | LPTE_LOCKED);
	if (slot != -1) {
		pvo->pvo_pte.slot = slot;
		return (0);
	}

	return (-1);
}

static int64_t
moea64_pte_insert_native(struct pvo_entry *pvo)
{
	struct lpte insertpt;
	int64_t ret;

	/* Initialize PTE */
	moea64_pte_from_pvo(pvo, &insertpt);

	/* Make sure further insertion is locked out during evictions */
	rw_rlock(&moea64_eviction_lock);

	pvo->pvo_pte.slot &= ~7ULL; /* Base slot address */
	ret = moea64_pte_insert_locked(pvo, &insertpt, LPTE_VALID);
	if (ret == -1) {
		/*
		 * Out of luck. Find a PTE to sacrifice.
		 */

		/* Lock out all insertions for a bit */
		if (!rw_try_upgrade(&moea64_eviction_lock)) {
			rw_runlock(&moea64_eviction_lock);
			rw_wlock(&moea64_eviction_lock);
		}
		/* Don't evict large pages */
		ret = moea64_pte_insert_locked(pvo, &insertpt, LPTE_BIG);
		rw_wunlock(&moea64_eviction_lock);
		/* No freeable slots in either PTEG? We're hosed. */
		if (ret == -1)
			panic("moea64_pte_insert: overflow");
	} else
		rw_runlock(&moea64_eviction_lock);

	return (0);
}

static void *
moea64_dump_pmap_native(void *ctx, void *buf, u_long *nbytes)
{
	struct dump_context *dctx;
	u_long ptex, ptex_end;

	dctx = (struct dump_context *)ctx;
	ptex = dctx->ptex;
	ptex_end = ptex + dctx->blksz / sizeof(struct lpte);
	ptex_end = MIN(ptex_end, dctx->ptex_end);
	*nbytes = (ptex_end - ptex) * sizeof(struct lpte);

	if (*nbytes == 0)
		return (NULL);

	dctx->ptex = ptex_end;
	return (__DEVOLATILE(struct lpte *, moea64_pteg_table) + ptex);
}

static __always_inline uint64_t
moea64_vpn_from_pte(uint64_t ptehi, uintptr_t slot)
{
	uint64_t pgn, pgnlo, vsid;

	vsid = (ptehi & LPTE_AVA_MASK) >> LPTE_VSID_SHIFT;
	if ((ptehi & LPTE_HID) != 0)
		slot ^= (moea64_pteg_mask << 3);
	pgnlo = ((vsid & VSID_HASH_MASK) ^ (slot >> 3)) & EA_PAGELO_MASK;
	pgn = ((ptehi & LPTE_AVA_PGNHI_MASK) << (EA_PAGELO_SHIFT -
	    LPTE_AVA_PGNHI_SHIFT)) | pgnlo;
	return ((vsid << 16) | pgn);
}

static __always_inline int64_t
moea64_pte_unset_sp_locked(struct pvo_entry *pvo)
{
	volatile struct lpte *pt;
	uint64_t ptehi, refchg, vpn;
	vm_offset_t eva;

	refchg = 0;
	eva = PVO_VADDR(pvo) + HPT_SP_SIZE;

	for (; pvo != NULL && PVO_VADDR(pvo) < eva;
	    pvo = RB_NEXT(pvo_tree, &pvo->pvo_pmap->pmap_pvo, pvo)) {
		pt = moea64_pteg_table + pvo->pvo_pte.slot;
		ptehi = be64toh(pt->pte_hi);
		if ((ptehi & LPTE_AVPN_MASK) !=
		    moea64_pte_vpn_from_pvo_vpn(pvo)) {
			/* Evicted: invalidate new entry */
			STAT_MOEA64(moea64_pte_overflow--);
			vpn = moea64_vpn_from_pte(ptehi, pvo->pvo_pte.slot);
			CTR1(KTR_PMAP, "Evicted page in pte_unset_sp: vpn=%jx",
			    (uintmax_t)vpn);
			/* Assume evicted page was modified */
			refchg |= LPTE_CHG;
		} else
			vpn = pvo->pvo_vpn;

		refchg |= moea64_pte_unset_locked(pt, vpn);
	}

	return (refchg);
}

static int64_t
moea64_pte_unset_sp_native(struct pvo_entry *pvo)
{
	uint64_t refchg;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);
	KASSERT((PVO_VADDR(pvo) & HPT_SP_MASK) == 0,
	    ("%s: va %#jx unaligned", __func__, (uintmax_t)PVO_VADDR(pvo)));

	rw_rlock(&moea64_eviction_lock);
	refchg = moea64_pte_unset_sp_locked(pvo);
	rw_runlock(&moea64_eviction_lock);

	return (refchg);
}

static __always_inline int64_t
moea64_pte_insert_sp_locked(struct pvo_entry *pvo)
{
	struct lpte insertpt;
	int64_t ret;
	vm_offset_t eva;

	eva = PVO_VADDR(pvo) + HPT_SP_SIZE;

	for (; pvo != NULL && PVO_VADDR(pvo) < eva;
	    pvo = RB_NEXT(pvo_tree, &pvo->pvo_pmap->pmap_pvo, pvo)) {
		moea64_pte_from_pvo(pvo, &insertpt);
		pvo->pvo_pte.slot &= ~7ULL; /* Base slot address */

		ret = moea64_pte_insert_locked(pvo, &insertpt, LPTE_VALID);
		if (ret == -1) {
			/* Lock out all insertions for a bit */
			if (!rw_try_upgrade(&moea64_eviction_lock)) {
				rw_runlock(&moea64_eviction_lock);
				rw_wlock(&moea64_eviction_lock);
			}
			/* Don't evict large pages */
			ret = moea64_pte_insert_locked(pvo, &insertpt,
			    LPTE_BIG);
			rw_downgrade(&moea64_eviction_lock);
			/* No freeable slots in either PTEG? We're hosed. */
			if (ret == -1)
				panic("moea64_pte_insert_sp: overflow");
		}
	}

	return (0);
}

static int64_t
moea64_pte_insert_sp_native(struct pvo_entry *pvo)
{
	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);
	KASSERT((PVO_VADDR(pvo) & HPT_SP_MASK) == 0,
	    ("%s: va %#jx unaligned", __func__, (uintmax_t)PVO_VADDR(pvo)));

	rw_rlock(&moea64_eviction_lock);
	moea64_pte_insert_sp_locked(pvo);
	rw_runlock(&moea64_eviction_lock);

	return (0);
}

static int64_t
moea64_pte_replace_sp_native(struct pvo_entry *pvo)
{
	uint64_t refchg;

	PMAP_LOCK_ASSERT(pvo->pvo_pmap, MA_OWNED);
	KASSERT((PVO_VADDR(pvo) & HPT_SP_MASK) == 0,
	    ("%s: va %#jx unaligned", __func__, (uintmax_t)PVO_VADDR(pvo)));

	rw_rlock(&moea64_eviction_lock);
	refchg = moea64_pte_unset_sp_locked(pvo);
	moea64_pte_insert_sp_locked(pvo);
	rw_runlock(&moea64_eviction_lock);

	return (refchg);
}
