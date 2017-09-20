/*-
 * Copyright (c) 2004-2010 Juli Mallett <jmallett@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef	_MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

#ifndef _LOCORE
#include <machine/param.h>

#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */
typedef	uint64_t pt_entry_t;
#else
typedef	uint32_t pt_entry_t;
#endif

#if defined(_KERNEL) && !defined(__CHERI_PURE_CAPABILITY__)
typedef	pt_entry_t *pd_entry_t;
#else
/*
 * XXX: used in the kernel to set VM system paramaters.  Only used for
 * the parameter macros (which use its size) in usespace.
 */
typedef uint64_t pd_entry_t;
#endif
#endif

/*
 * TLB and PTE management.  Most things operate within the context of
 * EntryLo0,1, and begin with TLBLO_.  Things which work with EntryHi
 * start with TLBHI_.  PTE bits begin with PTE_.
 *
 * Note that we use the same size VM and TLB pages.
 */
#define	TLB_PAGE_SHIFT	(PAGE_SHIFT)
#define	TLB_PAGE_SIZE	(1 << TLB_PAGE_SHIFT)
#define	TLB_PAGE_MASK	(TLB_PAGE_SIZE - 1)

/*
 * TLB PageMask register.  Has mask bits set above the default, 4K, page mask.
 */
#define	TLBMASK_SHIFT	(13)
#define	TLBMASK_MASK	((PAGE_MASK >> TLBMASK_SHIFT) << TLBMASK_SHIFT)

/*
 * FreeBSD/mips page-table entries take a near-identical format to MIPS TLB
 * entries, each consisting of two 32-bit or 64-bit values ("EntryHi" and
 * "EntryLo").  MIPS4k and MIPS64 both define certain bits in TLB entries as
 * reserved, and these must be zero-filled by software.  We overload these
 * bits in PTE entries to hold  PTE_ flags such as RO, W, and MANAGED.
 * However, we must mask these out when writing to TLB entries to ensure that
 * they do not become visible to hardware -- especially on MIPS64r2 which has
 * an extended physical memory space.
 *
 * When using n64 and n32, shift software-defined bits into the MIPS64r2
 * reserved range, which runs from bit 55 ... 63.  In other configurations
 * (32-bit MIPS4k and compatible), shift them out to bits 29 ... 31.
 *
 * NOTE: This means that for 32-bit use of CP0, we aren't able to set the top
 * bit of PFN to a non-zero value, as software is using it!  This physical
 * memory size limit may not be sufficiently enforced elsewhere.
 *
 * XXXRW: On CHERI, bits 63 and 62 are used for additional permissions that
 * prevent loading and storing of capabilities, so we have reduced the 55-bit
 * shift to 53 bits.
 */
#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */
#define	TLBLO_SWBITS_SHIFT	(53)		/* XXXRW: Was 55. */
#define	TLBLO_REF_BIT_SHIFT	(61)
#define	TLBLO_SWBITS_CLEAR_SHIFT	(11)	/* XXXSS: Was 9. */
#define	TLBLO_PFN_MASK		0xFFFFFFC0ULL
#define	TLB_1M_SUPERPAGE_SHIFT	(PDRSHIFT)
#define	TLBLO_SWBITS_MASK	((pt_entry_t)0x7F << TLBLO_SWBITS_SHIFT)
#else
#define	TLBLO_SWBITS_SHIFT	(29)
#define	TLBLO_SWBITS_CLEAR_SHIFT	(3)
#define	TLBLO_PFN_MASK		(0x1FFFFFC0)
#define	TLBLO_SWBITS_MASK	((pt_entry_t)0x7 << TLBLO_SWBITS_SHIFT)
#endif
#define	TLBLO_PFN_SHIFT		(6)

/*
 * XXX This comment is not correct for anything more modern than R4K.
 *
 * VPN for EntryHi register.  Upper two bits select user, supervisor,
 * or kernel.  Bits 61 to 40 copy bit 63.  VPN2 is bits 39 and down to
 * as low as 13, down to PAGE_SHIFT, to index 2 TLB pages*.  From bit 12
 * to bit 8 there is a 5-bit 0 field.  Low byte is ASID.
 *
 * XXX This comment is not correct for FreeBSD.
 * Note that in FreeBSD, we map 2 TLB pages is equal to 1 VM page.
 */
#define	TLBHI_ASID_MASK		(0xff)
#if defined(__mips_n64)
#define	TLBHI_R_SHIFT		62
#define	TLBHI_R_USER		(0x00UL << TLBHI_R_SHIFT)
#define	TLBHI_R_SUPERVISOR	(0x01UL << TLBHI_R_SHIFT)
#define	TLBHI_R_KERNEL		(0x03UL << TLBHI_R_SHIFT)
#define	TLBHI_R_MASK		(0x03UL << TLBHI_R_SHIFT)
#define	TLBHI_VA_R(va)		((va) & TLBHI_R_MASK)
#define	TLBHI_FILL_SHIFT	40
#define	TLBHI_VPN2_SHIFT	(TLB_PAGE_SHIFT + 1)
#define	TLBHI_VPN2_MASK		(((~((1UL << TLBHI_VPN2_SHIFT) - 1)) << (63 - TLBHI_FILL_SHIFT)) >> (63 - TLBHI_FILL_SHIFT))
#define	TLBHI_VA_TO_VPN2(va)	((va) & TLBHI_VPN2_MASK)
#define	TLBHI_ENTRY(va, asid)	((TLBHI_VA_R((va))) /* Region. */ | \
				 (TLBHI_VA_TO_VPN2((va))) /* VPN2. */ | \
				 ((asid) & TLBHI_ASID_MASK))
#else /* !defined(__mips_n64) */
#define	TLBHI_PAGE_MASK		(2 * PAGE_SIZE - 1)
#define	TLBHI_ENTRY(va, asid)	(((va) & ~TLBHI_PAGE_MASK) | ((asid) & TLBHI_ASID_MASK))
#endif /* defined(__mips_n64) */

/*
 * PTE Hardware Bits (EntryLo0-1 register fields)
 *
 * Lower bits of a 32 bit PTE:
 *
 *                                  28 --------------- 6 5 - 3  2   1   0
 *                                  --------------------------------------
 *                                 |         PFN        |  C  | D | VR| G |
 *                                  --------------------------------------
 *
 * Lower bits of a 64 bit PTE:
 *
 *  52 -------------------- 34  33 ------------------- 6 5 - 3  2   1   0
 *  ----------------------------------------------------------------------
 * |       Reserved (Zero)     |          PFN           |  C  | D | VR| G |
 *  ----------------------------------------------------------------------
 *
 * TLB flags managed in hardware:
 *    PFN:	Page Frame Number.
 * 	C:	Cache attribute.
 * 	D:	Dirty bit.  This means a page is writable.  It is not
 * 		set at first, and a write is trapped, and the dirty
 * 		bit is set.  See also PTE_RO.
 * 	VR:	Valid/Reference bit. See also PTE_SV.
 * 	G:	Global bit.  This means that this mapping is present
 * 		in EVERY address space, and to ignore the ASID when
 * 		it is matched.
 */
#define	PTE_C(attr)		((attr & 0x07) << 3)
#define	PTE_C_MASK		(PTE_C(0x07))
#define	PTE_C_UNCACHED		(PTE_C(MIPS_CCA_UNCACHED))
#define	PTE_C_CACHE		(PTE_C(MIPS_CCA_CACHED))
#define	PTE_C_WC		(PTE_C(MIPS_CCA_WC))
#define	PTE_D			0x04
#define	PTE_VR			0x02
#define	PTE_G			0x01

#ifdef CPU_CHERI
/*
 * CHERI EntryLo extensions that limit storing loading and storing tagged
 * values.
 */
#define	PTE_SC			(0x1ULL << 63)
#define	PTE_LC			(0x1ULL << 62)
#endif

/*
 * PTE Software Bits
 *
 * Upper bits of a 32 bit PTE:
 *
 *     31   30   29
 *    --------------
 *   | MN | W  | RO |
 *    --------------
 *
 * Upper bits of a 64 bit PTE:
 *
 *   63-62   61-60  59   58 -- 56    55   54   53
 *   ---------------------------------------------
 *  |  RG  |      | SV | PG SZ IDX | MN | W  | RO |
 *   ---------------------------------------------
 *
 * VM flags managed in software:
 *  RG: Region.  (Reserved. Currently not used.)
 *  SV: Soft Valid bit.
 *  PG SZ IDX: Page Size Index (0-7).
 *      Index   Page Mask (Binary)  HW Page Size
 *      -----   ------------------- ------------
 *      0   0000 0000 0000 0000   4K
 *      1   0000 0000 0000 0011  16K
 *      2   0000 0000 0000 1111  64K
 *      3   0000 0000 0011 1111 256K
 *      4   0000 0000 1111 1111   1M
 *      5   0000 0011 1111 1111   4M
 *      6   0000 1111 1111 1111  16M
 * (MIPS 3:)
 *      7   0011 1111 1111 1111  64M
 *      8   1111 1111 1111 1111 256M (Not currently supported)
 *
 *  RO: Read only.  Never set PTE_D on this page, and don't
 *      listen to requests to write to it.
 *  W:  Wired.  ???
 *  MANAGED:Managed.  This PTE maps a managed page.
 *
 * These bits should not be written into the TLB, so must first be masked out
 * explicitly in C, or using CLEAR_PTE_SWBITS() in assembly.
 */
#define	PTE_RO			((pt_entry_t)0x01 << TLBLO_SWBITS_SHIFT)
#define	PTE_W			((pt_entry_t)0x02 << TLBLO_SWBITS_SHIFT)
#define	PTE_MANAGED		((pt_entry_t)0x04 << TLBLO_SWBITS_SHIFT)
#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */
#define	PTE_PS_16K		((pt_entry_t)0x08 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_64K		((pt_entry_t)0x10 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_256K		((pt_entry_t)0x18 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_1M		((pt_entry_t)0x20 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_4M		((pt_entry_t)0x28 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_16M		((pt_entry_t)0x30 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_64M		((pt_entry_t)0x38 << TLBLO_SWBITS_SHIFT)
#define	PTE_PS_IDX_MASK		((pt_entry_t)0x38 << TLBLO_SWBITS_SHIFT)
#define	PTE_PSIDX_NBITS_TO_LEFT		5
#define	PTE_PSIDX_NBITS_TO_RIGHT	56
#define	PTE_PFN_NBITS_TO_LEFT		11
#define	PTE_PFN_NBITS_TO_RIGHT		6
#define	PTE_HWFLAGS_NBITS_TO_LEFT	58
#define	SW_VALID		0x40
#define	PTE_SV			((pt_entry_t)SW_VALID << TLBLO_SWBITS_SHIFT)
#else
#define	PTE_PS_IDX_MASK		0
#define	PTE_SV			0
#endif

/*
 * Promotion to a 4MB (PDE) page mapping requires that the corresponding 4KB
 * (PTE) page mappings have identical settings for the following fields:
 */
#define	PG_PROMOTE_MASK	(PTE_G | PTE_VALID | PTE_D | PTE_C_UNCACHED | \
			PTE_C_CACHE | PTE_RO | PTE_W | PTE_MANAGED | \
			PTE_REF)

#ifdef MIPS64_NEW_PMAP
#define	TLBLO_PTE_TO_IDX(pte)	(((pte) & PTE_PS_IDX_MASK) >> 56)
#define	TLBMASK_IDX_TO_MASK(idx) (((1 << ((idx) << 1)) - 1) << TLBMASK_SHIFT)
#define	TLBLO_PTE_TO_MASK(pte)	TLBMASK_IDX_TO_MASK(TLBLO_PTE_TO_IDX(pte))
#define	TLBMASK_4K_PAGE		TLBMASK_IDX_TO_MASK(0)
#define	TLBMASK_16K_PAGE	TLBMASK_IDX_TO_MASK(1)
#define	TLBMASK_64K_PAGE	TLBMASK_IDX_TO_MASK(2)
#define	TLBMASK_256K_PAGE	TLBMASK_IDX_TO_MASK(3)
#define	TLBMASK_1M_PAGE		TLBMASK_IDX_TO_MASK(4)
#define	TLBMASK_4M_PAGE		TLBMASK_IDX_TO_MASK(5)
#define	TLBMASK_16M_PAGE	TLBMASK_IDX_TO_MASK(6)
#define	TLBMASK_64M_PAGE	TLBMASK_IDX_TO_MASK(7)
#else /* ! MIPS64_NEW_PMAP */
#define	TLBLO_PTE_TO_IDX(pte) 	0
#define	TLBLO_PTE_TO_MASK(pte)	0
#endif /* ! MIPS64_NEW_PMAP */

/*
 * PTE management functions for bits defined above.
 */
#ifndef _LOCORE
static __inline void
pte_clear(pt_entry_t *pte, pt_entry_t bit)
{

	*pte &= (~bit);
}

static __inline void
pte_set(pt_entry_t *pte, pt_entry_t bit)
{

	*pte |= bit;
}

static __inline int
pte_test(pt_entry_t *pte, pt_entry_t bit)
{

	return ((*pte & bit) == bit);
}

static __inline void
pde_clear(pd_entry_t *pde, pt_entry_t bit)
{

	*(pt_entry_t *)pde &= (~bit);
}

static __inline void
pde_set(pd_entry_t *pde, pt_entry_t bit)
{

	*(pt_entry_t *)pde |= bit;
}

static __inline int
pde_test(pd_entry_t *pde, pt_entry_t bit)
{

	return ((*(pt_entry_t *)pde & bit) == bit);
}

static __inline pt_entry_t
TLBLO_PA_TO_PFN(vm_paddr_t pa)
{

	return (((pa >> TLB_PAGE_SHIFT) << TLBLO_PFN_SHIFT) & TLBLO_PFN_MASK);
}

static __inline vm_paddr_t
TLBLO_PFN_TO_PA(pt_entry_t pfn)
{

	return ((vm_paddr_t)(pfn >> TLBLO_PFN_SHIFT) << TLB_PAGE_SHIFT);
}

static __inline pt_entry_t
TLBLO_PTE_TO_PFN(pt_entry_t pte)
{

	return (pte & TLBLO_PFN_MASK);
}

#ifdef MIPS64_NEW_PMAP

#define	PTE_REF		PTE_VR
#define	PTE_VALID 	PTE_SV

#define	pte_is_ref(pte)			pte_test((pte), PTE_REF)
#define	pte_ref_clear(pte)		pte_clear((pte), PTE_REF)
#define	pte_ref_set(pte)		pte_set((pte), PTE_REF)
#define	pte_ref_atomic_clear(pte)	atomic_clear_long((pte), PTE_REF)
#define	pte_ref_atomic_set(pte)		atomic_set_long((pte), PTE_REF)

#else /* ! MIPS64_NEW_PMAP */

#define	PTE_REF		0
#define	PTE_VALID	PTE_VR

#define	pte_is_ref(pte)			0
#define	pte_ref_clear(pte)
#define	pte_ref_set(pte)
#define	pte_ref_atomic_clear(pte)
#define	pte_ref_atomic_set(pte, bit)

#endif /* ! MIPS64_NEW_PMAP */

#define	pte_is_valid(pte)		pte_test((pte), PTE_VALID)

#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */

#define	pte_atomic_clear(pte, bit)	atomic_clear_64((pte), bit)
#define	pte_atomic_set(pte, bit)	atomic_set_64((pte), bit)
#define	pte_load_store(ptep, pte)	atomic_readandset_64(ptep, pte)
#define	pde_load_store(pdep, pde)	(pd_entry_t)atomic_readandset_64(\
						(pt_entry_t *)pdep, pde)

#define	pte_atomic_store(ptep, pte)	atomic_store_rel_64(ptep, pte)
#define pte_store(ptep, pte)	do {		\
	*(u_long *)(ptep) = (u_long)(pte);	\
} while (0)
#define	pde_store(pdep, pde)		pte_store(pdep, pde)


#else /* ! PHYSADDR_64_BIT */

#define	pte_atomic_clear(pte, bit)	atomic_clear_32((pte), bit)
#define	pte_atomic_set(pte, bit)	atomic_set_32((pte), bit)
#define	pte_load_store(ptep, pte)	atomic_readandset_32(ptep, pte)
#define	pde_load_store(pdep, pde)	(pd_entry_t)atomic_readandset_32(\
						(pt_entry_t *)pdep, pde)

#define	pte_atomic_store(ptep, pte)	atomic_store_rel_32(ptep, pte)
#define pte_store(ptep, pte)	do {		\
	*(u_int *)(ptep) = (u_int)(pte);	\
} while (0)
#define	pde_store(pdep, pde)		pte_store(pdep, pde)

#endif /* ! PHYSADDR_64_BIT */

#endif /* ! _LOCORE */

#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */

#ifndef _LOCORE
/*
 * Check to see if a PDE is actually a superpage (PageSize > 4K) PTE.
 *
 * On __mips_n64 the kernel uses the virtual memory address range from
 * VM_MIN_KERNEL_ADDRESS (0xc000000000000000) to VM_MAX_KERNEL_ADDRESS
 * (0xc000008000000000). Therefore, a valid virtual address in the PDE
 * (a pointer to a page table) will have bits 61 to 40 set to zero. A
 * superpage will have one of the superpage size bits (bits 58 to 56)
 * set.
 */

/* Is the PDE a superpage of any size? */
static __inline int
pde_is_superpage(pd_entry_t *pde)
{

	return (((pt_entry_t)*pde & PTE_PS_IDX_MASK) != 0);
}

/* Is the PTE a superpage of any size? */
static __inline int
pte_is_superpage(pt_entry_t *pte)
{

	return ((*pte & PTE_PS_IDX_MASK) != 0);
}

/* Is the PDE an 1MB superpage? */
static __inline int
pde_is_1m_superpage(pd_entry_t *pde)
{

	return (((pt_entry_t)*pde & PTE_PS_1M) == PTE_PS_1M);
}

/* Is the PTE an 1MB superpage? */
static __inline int
pte_is_1m_superpage(pt_entry_t *pte)
{

	return ((*pte & PTE_PS_1M) == PTE_PS_1M);
}

/* Physical Address to Superpage Physical Frame Number. */
static __inline pt_entry_t
TLBLO_PA_TO_SPFN(vm_paddr_t pa)
{

	return (((pa >> TLB_1M_SUPERPAGE_SHIFT) << TLBLO_PFN_SHIFT) &
	    TLBLO_PFN_MASK);
}

/* Superpage Physical Frame Number to Physical Address. */
static __inline vm_paddr_t
TLBLO_SPFN_TO_PA(pt_entry_t spfn)
{

	return ((vm_paddr_t)(spfn >> TLBLO_PFN_SHIFT) <<
	    TLB_1M_SUPERPAGE_SHIFT);
}

/* Superpage Page Table Entry to Physical Address. */
static __inline vm_paddr_t
TLBLO_SPTE_TO_PA(pt_entry_t pte)
{
	return (TLBLO_SPFN_TO_PA(TLBLO_PTE_TO_PFN(pte)));
}

static __inline vm_paddr_t
TLBLO_SPDE_TO_PA(pd_entry_t pde)
{
	return (TLBLO_SPFN_TO_PA(TLBLO_PTE_TO_PFN((pt_entry_t)pde)));
}


/* An 4KB Page Table Entry to Physical Address. */
static __inline vm_paddr_t
TLBLO_PTE_TO_PA(pt_entry_t pte)
{

	return (TLBLO_PFN_TO_PA(TLBLO_PTE_TO_PFN(pte)));
}

static __inline vm_paddr_t
TLBLO_PDE_TO_PA(pd_entry_t pde)
{

	return (TLBLO_PFN_TO_PA(TLBLO_PTE_TO_PFN((pt_entry_t)pde)));
}
#endif /* ! _LOCORE */

#else /* ! PHYSADDR_64_BIT */

#define	pte_is_referenced(pte)		0
#define	pte_reference_reset(pte)
#define	pte_reference_page(pte)
#define	pde_is_superpage(pde)		0
#define	pte_is_superpage(pde)		0
#define pde_is_1m_superpage(pte)	0
#define pte_is_1m_superpage(pte)	0

#ifndef _LOCORE
static __inline vm_paddr_t
TLBLO_PTE_TO_PA(pt_entry_t pte)
{

	return (TLBLO_PFN_TO_PA(TLBLO_PTE_TO_PFN(pte)));
}
#endif /* ! _LOCORE */
#endif /* ! PHYSADDR_64_BIT */

#define	pte_cache_bits(pte)	((*(pte) >> 3) & 0x07)

/* Assembly support for PTE access*/
#ifdef LOCORE
#if defined(__mips_n64) || defined(__mips_n32) /*  PHYSADDR_64_BIT */
#define	PTESHIFT		3
#define	PTE2MASK		0xff0	/* for the 2-page lo0/lo1 */
#define	PTEMASK			0xff8
#define	PTESIZE			8
#define	PTE_L			ld
#define	PTE_S			sd
#define	PTE_MTC0		dmtc0
#define	CLEAR_PTE_SWBITS(r)

#ifdef MIPS64_NEW_PMAP

/* Superpage and referenced bit emulation ASM macros. */

/*
 * GET_SUPERPAGE_IDX(r)
 *
 * Get the superpage index from the PTE by shifting it left by
 * PTE_PSIDX_NBITS_TO_LEFT (clearing the upper softbits) and then back to the
 * right by (PTE_PSIDX_NBITS_TO_RIGHT + PTE_PSIDX_NBITS_TO_RIGHT) clearing
 * all the lower bits in the process.
 */
#define GET_SUPERPAGE_IDX(r)				\
	dsll	r, (PTE_PSIDX_NBITS_TO_LEFT);		\
	dsrl32	r, (PTE_PSIDX_NBITS_TO_RIGHT + PTE_PSIDX_NBITS_TO_LEFT - 32)

/*
 * GET_HW_TLB_FLAGS(r)
 *
 * Get the lower hardware TLB flags but shifting left then right.
 */
#define GET_HW_TLB_FLAGS(r)				\
	dsll32	r, (PTE_HWFLAGS_NBITS_TO_LEFT - 32);	\
	dsrl32	r, (PTE_HWFLAGS_NBITS_TO_LEFT - 32)

/*
 * GET_ODD_1M_PFN_FROM_EVEN(r)
 *
 * Get the odd 1M PFN (TLB lo1) from the even 1M PTE.  First, mask out the PFN
 * from the even PTE. Then add 1M worth of pages to it (256). Finally, shift it
 * back to its position in the PTE.
 */
#define GET_ODD_1M_PFN_FROM_EVEN(r)			\
	dsll	r, (PTE_PFN_NBITS_TO_LEFT);		\
	dsrl	r, (PTE_PFN_NBITS_TO_LEFT + PTE_PFN_NBITS_TO_RIGHT); \
	daddiu	r, r, (1024 * 1024 / PAGE_SIZE);	\
	dsll	r, (PTE_PFN_NBITS_TO_RIGHT)

/*
 * IF_VALID_SET_REFBIT(r0, r1, offset, unique)
 *
 * If a PDE is valid then set the referenced bit (PTE_VR).  The first version
 * does it atomically.
 */
#define ATOMIC_REFBIT_UPDATE
#ifdef ATOMIC_REFBIT_UPDATE

#define IF_VALID_SET_REFBIT(r0, r1, offset, unique)	\
try_again ## unique ## : ;				\
	dsrl32	r0, (TLBLO_SWBITS_SHIFT - 32);		\
	andi	r0, r0, SW_VALID;			\
	beqz	r0, not_valid ## unique ;		\
	PTE_L	r0, offset ## (r1) ;			\
	lld	r0, offset ## (r1) ;			\
	ori	r0, r0, PTE_VR ;			\
	scd	r0, offset ## (r1) ;			\
	beqz	r0, try_again ## unique ;		\
	PTE_L	r0, offset ## (r1) ;			\
not_valid ## unique ## :

#else /* ! ATOMIC_REFBIT_UPDATE */

#define IF_VALID_SET_REFBIT(r0, r1, offset, unique)	\
try_again ## unique ## : ;				\
	dsrl32	r0, (TLBLO_SWBITS_SHIFT - 32) ;		\
	andi	r0, r0, SW_VALID ;			\
	beqz	r0, not_valid ## unique ;		\
	PTE_L	r0, offset ## (r1) ;			\
	ori	r0, r0, PTE_VR ;			\
	PTE_S	r0, offset ## (r1) ;			\
not_valid ## unique ## :
#endif /* ! ATOMIC_REFBIT_UPDATE */

#else /* ! MIPS64_NEW_PMAP */

#define	GET_SUPERPAGE_IDX(r)
#define GET_HW_TLB_FLAGS(r)
#define	IF_VALID_SET_REFBIT(r0, r1, offset, unique)

#endif /* ! MIPS64_NEW_PMAP */

#else /* ! LOCORE */
#define	PTESHIFT		2
#define	PTE2MASK		0xff8	/* for the 2-page lo0/lo1 */
#define	PTEMASK			0xffc
#define	PTESIZE			4
#define	PTE_L			lw
#define	PTE_S			sw
#define	PTE_MTC0		mtc0
#define	CLEAR_PTE_SWBITS(r)	LONG_SLL r, TLBLO_SWBITS_CLEAR_SHIFT; LONG_SRL r, TLBLO_SWBITS_CLEAR_SHIFT /* remove swbits */

#define	IS_PTE_VALID(r0, r1, offset, label)
#define	SET_REF_BIT(r0, r1, offset)

#endif /* defined(__mips_n64) || defined(__mips_n32) */

#if defined(__mips_n64)
#define	PTRSHIFT		3
#define	PDEPTRMASK		0xff8
#else
#define	PTRSHIFT		2
#define	PDEPTRMASK		0xffc
#endif

#endif /* LOCORE */

/* PageMask Register (CP0 Register 5, Select 0) Values */
#define	MIPS3_PGMASK_MASKX	0x00001800
#define	MIPS3_PGMASK_4K		0x00000000
#define	MIPS3_PGMASK_16K	0x00006000
#define	MIPS3_PGMASK_64K	0x0001e000
#define	MIPS3_PGMASK_256K	0x0007e000
#define	MIPS3_PGMASK_1M		0x001fe000
#define	MIPS3_PGMASK_4M		0x007fe000
#define	MIPS3_PGMASK_16M	0x01ffe000
#define	MIPS3_PGMASK_64M	0x07ffe000
#define	MIPS3_PGMASK_256M	0x1fffe000

#endif /* !_MACHINE_PTE_H_ */
