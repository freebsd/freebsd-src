/*-
 * Copyright (c) 2006 Peter Wemm
 * Copyright (c) 2019 Leandro Lupori
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
 * From: FreeBSD: src/lib/libkvm/kvm_minidump_riscv.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <vm/vm.h>

#include <kvm.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../sys/powerpc/include/minidump.h"
#include "kvm_private.h"
#include "kvm_powerpc64.h"

/*
 * PowerPC64 HPT machine dependent routines for kvm and minidumps.
 *
 * Address Translation parameters:
 *
 * b = 12 (SLB base page size: 4 KB)
 * b = 24 (SLB base page size: 16 MB)
 * p = 12 (page size: 4 KB)
 * p = 24 (page size: 16 MB)
 * s = 28 (segment size: 256 MB)
 */

/* Large (huge) page params */
#define	LP_PAGE_SHIFT		24
#define	LP_PAGE_SIZE		(1ULL << LP_PAGE_SHIFT)
#define	LP_PAGE_MASK		0x00ffffffULL

/* SLB */

#define	SEGMENT_LENGTH		0x10000000ULL

#define	round_seg(x)		roundup2((uint64_t)(x), SEGMENT_LENGTH)

/* Virtual real-mode VSID in LPARs */
#define	VSID_VRMA		0x1ffffffULL

#define	SLBV_L			0x0000000000000100ULL /* Large page selector */
#define	SLBV_CLASS		0x0000000000000080ULL /* Class selector */
#define	SLBV_LP_MASK		0x0000000000000030ULL
#define	SLBV_VSID_MASK		0x3ffffffffffff000ULL /* Virtual SegID mask */
#define	SLBV_VSID_SHIFT		12

#define	SLBE_B_MASK		0x0000000006000000ULL
#define	SLBE_B_256MB		0x0000000000000000ULL
#define	SLBE_VALID		0x0000000008000000ULL /* SLB entry valid */
#define	SLBE_INDEX_MASK		0x0000000000000fffULL /* SLB index mask */
#define	SLBE_ESID_MASK		0xfffffffff0000000ULL /* Effective SegID mask */
#define	SLBE_ESID_SHIFT		28

/* PTE */

#define	LPTEH_VSID_SHIFT	12
#define	LPTEH_AVPN_MASK		0xffffffffffffff80ULL
#define	LPTEH_B_MASK		0xc000000000000000ULL
#define	LPTEH_B_256MB		0x0000000000000000ULL
#define	LPTEH_BIG		0x0000000000000004ULL	/* 4KB/16MB page */
#define	LPTEH_HID		0x0000000000000002ULL
#define	LPTEH_VALID		0x0000000000000001ULL

#define	LPTEL_RPGN		0xfffffffffffff000ULL
#define	LPTEL_LP_MASK		0x00000000000ff000ULL
#define	LPTEL_NOEXEC		0x0000000000000004ULL

/* Supervisor        (U: RW, S: RW) */
#define	LPTEL_BW		0x0000000000000002ULL

/* Both Read Only    (U: RO, S: RO) */
#define	LPTEL_BR		0x0000000000000003ULL

#define	LPTEL_RW		LPTEL_BW
#define	LPTEL_RO		LPTEL_BR

/*
 * PTE AVA field manipulation macros.
 *
 * AVA[0:54] = PTEH[2:56]
 * AVA[VSID] = AVA[0:49] = PTEH[2:51]
 * AVA[PAGE] = AVA[50:54] = PTEH[52:56]
 */
#define	PTEH_AVA_VSID_MASK	0x3ffffffffffff000UL
#define	PTEH_AVA_VSID_SHIFT	12
#define	PTEH_AVA_VSID(p) \
	(((p) & PTEH_AVA_VSID_MASK) >> PTEH_AVA_VSID_SHIFT)

#define	PTEH_AVA_PAGE_MASK	0x0000000000000f80UL
#define	PTEH_AVA_PAGE_SHIFT	7
#define	PTEH_AVA_PAGE(p) \
	(((p) & PTEH_AVA_PAGE_MASK) >> PTEH_AVA_PAGE_SHIFT)

/* Masks to obtain the Physical Address from PTE low 64-bit word. */
#define	PTEL_PA_MASK		0x0ffffffffffff000UL
#define	PTEL_LP_PA_MASK		0x0fffffffff000000UL

#define	PTE_HASH_MASK		0x0000007fffffffffUL

/*
 * Number of AVA/VA page bits to shift right, in order to leave only the
 * ones that should be considered.
 *
 * q = MIN(54, 77-b) (PowerISA v2.07B, 5.7.7.3)
 * n = q + 1 - 50 (VSID size in bits)
 * s(ava) = 5 - n
 * s(va) = (28 - b) - n
 *
 * q: bit number of lower limit of VA/AVA bits to compare
 * n: number of AVA/VA page bits to compare
 * s: shift amount
 * 28 - b: VA page size in bits
 */
#define	AVA_PAGE_SHIFT(b)	(5 - (MIN(54, 77-(b)) + 1 - 50))
#define	VA_PAGE_SHIFT(b)	(28 - (b) - (MIN(54, 77-(b)) + 1 - 50))

/* Kernel ESID -> VSID mapping */
#define	KERNEL_VSID_BIT	0x0000001000000000UL /* Bit set in all kernel VSIDs */
#define	KERNEL_VSID(esid) ((((((uint64_t)esid << 8) | ((uint64_t)esid >> 28)) \
				* 0x13bbUL) & (KERNEL_VSID_BIT - 1)) | \
				KERNEL_VSID_BIT)

/* Types */

typedef uint64_t	ppc64_physaddr_t;

typedef struct {
	uint64_t slbv;
	uint64_t slbe;
} ppc64_slb_entry_t;

typedef struct {
	uint64_t pte_hi;
	uint64_t pte_lo;
} ppc64_pt_entry_t;

struct hpt_data {
	ppc64_slb_entry_t *slbs;
	uint32_t slbsize;
};


static void
slb_fill(ppc64_slb_entry_t *slb, uint64_t ea, uint64_t i)
{
	uint64_t esid;

	esid = ea >> SLBE_ESID_SHIFT;
	slb->slbv = KERNEL_VSID(esid) << SLBV_VSID_SHIFT;
	slb->slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID | i;
}

static int
slb_init(kvm_t *kd)
{
	struct minidumphdr *hdr;
	struct hpt_data *data;
	ppc64_slb_entry_t *slb;
	uint32_t slbsize;
	uint64_t ea, i, maxmem;

	hdr = &kd->vmst->hdr;
	data = PPC64_MMU_DATA(kd);

	/* Alloc SLBs */
	maxmem = hdr->bitmapsize * 8 * PPC64_PAGE_SIZE;
	slbsize = round_seg(hdr->kernend + 1 - hdr->kernbase + maxmem) /
	    SEGMENT_LENGTH * sizeof(ppc64_slb_entry_t);
	data->slbs = _kvm_malloc(kd, slbsize);
	if (data->slbs == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate slbs");
		return (-1);
	}
	data->slbsize = slbsize;

	dprintf("%s: maxmem=0x%jx, segs=%jd, slbsize=0x%jx\n",
	    __func__, (uintmax_t)maxmem,
	    (uintmax_t)slbsize / sizeof(ppc64_slb_entry_t), (uintmax_t)slbsize);

	/*
	 * Generate needed SLB entries.
	 *
	 * When translating addresses from EA to VA to PA, the needed SLB
	 * entry could be generated on the fly, but this is not the case
	 * for the walk_pages method, that needs to search the SLB entry
	 * by VSID, in order to find out the EA from a PTE.
	 */

	/* VM area */
	for (ea = hdr->kernbase, i = 0, slb = data->slbs;
	    ea < hdr->kernend; ea += SEGMENT_LENGTH, i++, slb++)
		slb_fill(slb, ea, i);

	/* DMAP area */
	for (ea = hdr->dmapbase;
	    ea < MIN(hdr->dmapend, hdr->dmapbase + maxmem);
	    ea += SEGMENT_LENGTH, i++, slb++) {
		slb_fill(slb, ea, i);
		if (hdr->hw_direct_map)
			slb->slbv |= SLBV_L;
	}

	return (0);
}

static void
ppc64mmu_hpt_cleanup(kvm_t *kd)
{
	struct hpt_data *data;

	if (kd->vmst == NULL)
		return;

	data = PPC64_MMU_DATA(kd);
	free(data->slbs);
	free(data);
	PPC64_MMU_DATA(kd) = NULL;
}

static int
ppc64mmu_hpt_init(kvm_t *kd)
{
	struct hpt_data *data;

	/* Alloc MMU data */
	data = _kvm_malloc(kd, sizeof(*data));
	if (data == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate MMU data");
		return (-1);
	}
	data->slbs = NULL;
	PPC64_MMU_DATA(kd) = data;

	if (slb_init(kd) == -1)
		goto failed;

	return (0);

failed:
	ppc64mmu_hpt_cleanup(kd);
	return (-1);
}

static ppc64_slb_entry_t *
slb_search(kvm_t *kd, kvaddr_t ea)
{
	struct hpt_data *data;
	ppc64_slb_entry_t *slb;
	int i, n;

	data = PPC64_MMU_DATA(kd);
	slb = data->slbs;
	n = data->slbsize / sizeof(ppc64_slb_entry_t);

	/* SLB search */
	for (i = 0; i < n; i++, slb++) {
		if ((slb->slbe & SLBE_VALID) == 0)
			continue;

		/* Compare 36-bit ESID of EA with segment one (64-s) */
		if ((slb->slbe & SLBE_ESID_MASK) != (ea & SLBE_ESID_MASK))
			continue;

		/* Match found */
		dprintf("SEG#%02d: slbv=0x%016jx, slbe=0x%016jx\n",
		    i, (uintmax_t)slb->slbv, (uintmax_t)slb->slbe);
		break;
	}

	/* SLB not found */
	if (i == n) {
		_kvm_err(kd, kd->program, "%s: segment not found for EA 0x%jx",
		    __func__, (uintmax_t)ea);
		return (NULL);
	}
	return (slb);
}

static ppc64_pt_entry_t
pte_get(kvm_t *kd, u_long ptex)
{
	ppc64_pt_entry_t pte, *p;

	p = _kvm_pmap_get(kd, ptex, sizeof(pte));
	pte.pte_hi = be64toh(p->pte_hi);
	pte.pte_lo = be64toh(p->pte_lo);
	return (pte);
}

static int
pte_search(kvm_t *kd, ppc64_slb_entry_t *slb, uint64_t hid, kvaddr_t ea,
    ppc64_pt_entry_t *p)
{
	uint64_t hash, hmask;
	uint64_t pteg, ptex;
	uint64_t va_vsid, va_page;
	int b;
	int ava_pg_shift, va_pg_shift;
	ppc64_pt_entry_t pte;

	/*
	 * Get VA:
	 *
	 * va(78) = va_vsid(50) || va_page(s-b) || offset(b)
	 *
	 * va_vsid: 50-bit VSID (78-s)
	 * va_page: (s-b)-bit VA page
	 */
	b = slb->slbv & SLBV_L? LP_PAGE_SHIFT : PPC64_PAGE_SHIFT;
	va_vsid = (slb->slbv & SLBV_VSID_MASK) >> SLBV_VSID_SHIFT;
	va_page = (ea & ~SLBE_ESID_MASK) >> b;

	dprintf("%s: hid=0x%jx, ea=0x%016jx, b=%d, va_vsid=0x%010jx, "
	    "va_page=0x%04jx\n",
	    __func__, (uintmax_t)hid, (uintmax_t)ea, b,
	    (uintmax_t)va_vsid, (uintmax_t)va_page);

	/*
	 * Get hash:
	 *
	 * Primary hash: va_vsid(11:49) ^ va_page(s-b)
	 * Secondary hash: ~primary_hash
	 */
	hash = (va_vsid & PTE_HASH_MASK) ^ va_page;
	if (hid)
		hash = ~hash & PTE_HASH_MASK;

	/*
	 * Get PTEG:
	 *
	 * pteg = (hash(0:38) & hmask) << 3
	 *
	 * hmask (hash mask): mask generated from HTABSIZE || 11*0b1
	 * hmask = number_of_ptegs - 1
	 */
	hmask = kd->vmst->hdr.pmapsize / (8 * sizeof(ppc64_pt_entry_t)) - 1;
	pteg = (hash & hmask) << 3;

	ava_pg_shift = AVA_PAGE_SHIFT(b);
	va_pg_shift = VA_PAGE_SHIFT(b);

	dprintf("%s: hash=0x%010jx, hmask=0x%010jx, (hash & hmask)=0x%010jx, "
	    "pteg=0x%011jx, ava_pg_shift=%d, va_pg_shift=%d\n",
	    __func__, (uintmax_t)hash, (uintmax_t)hmask,
	    (uintmax_t)(hash & hmask), (uintmax_t)pteg,
	    ava_pg_shift, va_pg_shift);

	/* Search PTEG */
	for (ptex = pteg; ptex < pteg + 8; ptex++) {
		pte = pte_get(kd, ptex);

		/* Check H, V and B */
		if ((pte.pte_hi & LPTEH_HID) != hid ||
		    (pte.pte_hi & LPTEH_VALID) == 0 ||
		    (pte.pte_hi & LPTEH_B_MASK) != LPTEH_B_256MB)
			continue;

		/* Compare AVA with VA */
		if (PTEH_AVA_VSID(pte.pte_hi) != va_vsid ||
		    (PTEH_AVA_PAGE(pte.pte_hi) >> ava_pg_shift) !=
		    (va_page >> va_pg_shift))
			continue;

		/*
		 * Check if PTE[L] matches SLBV[L].
		 *
		 * Note: this check ignores PTE[LP], as does the kernel.
		 */
		if (b == PPC64_PAGE_SHIFT) {
			if (pte.pte_hi & LPTEH_BIG)
				continue;
		} else if ((pte.pte_hi & LPTEH_BIG) == 0)
			continue;

		/* Match found */
		dprintf("%s: PTE found: ptex=0x%jx, pteh=0x%016jx, "
		    "ptel=0x%016jx\n",
		    __func__, (uintmax_t)ptex, (uintmax_t)pte.pte_hi,
		    (uintmax_t)pte.pte_lo);
		break;
	}

	/* Not found? */
	if (ptex == pteg + 8) {
		/* Try secondary hash */
		if (hid == 0)
			return (pte_search(kd, slb, LPTEH_HID, ea, p));
		else {
			_kvm_err(kd, kd->program,
			    "%s: pte not found", __func__);
			return (-1);
		}
	}

	/* PTE found */
	*p = pte;
	return (0);
}

static int
pte_lookup(kvm_t *kd, kvaddr_t ea, ppc64_pt_entry_t *pte)
{
	ppc64_slb_entry_t *slb;

	/* First, find SLB */
	if ((slb = slb_search(kd, ea)) == NULL)
		return (-1);

	/* Next, find PTE */
	return (pte_search(kd, slb, 0, ea, pte));
}

static int
ppc64mmu_hpt_kvatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{
	struct minidumphdr *hdr;
	struct vmstate *vm;
	ppc64_pt_entry_t pte;
	ppc64_physaddr_t pgoff, pgpa;
	off_t ptoff;
	int err;

	vm = kd->vmst;
	hdr = &vm->hdr;
	pgoff = va & PPC64_PAGE_MASK;

	dprintf("%s: va=0x%016jx\n", __func__, (uintmax_t)va);

	/*
	 * A common use case of libkvm is to first find a symbol address
	 * from the kernel image and then use kvatop to translate it and
	 * to be able to fetch its corresponding data.
	 *
	 * The problem is that, in PowerPC64 case, the addresses of relocated
	 * data won't match those in the kernel image. This is handled here by
	 * adding the relocation offset to those addresses.
	 */
	if (va < hdr->dmapbase)
		va += hdr->startkernel - PPC64_KERNBASE;

	/* Handle DMAP */
	if (va >= hdr->dmapbase && va <= hdr->dmapend) {
		pgpa = (va & ~hdr->dmapbase) & ~PPC64_PAGE_MASK;
		ptoff = _kvm_pt_find(kd, pgpa, PPC64_PAGE_SIZE);
		if (ptoff == -1) {
			_kvm_err(kd, kd->program, "%s: "
			    "direct map address 0x%jx not in minidump",
			    __func__, (uintmax_t)va);
			goto invalid;
		}
		*pa = ptoff + pgoff;
		return (PPC64_PAGE_SIZE - pgoff);
	/* Translate VA to PA */
	} else if (va >= hdr->kernbase) {
		if ((err = pte_lookup(kd, va, &pte)) == -1) {
			_kvm_err(kd, kd->program,
			    "%s: pte not valid", __func__);
			goto invalid;
		}

		if (pte.pte_hi & LPTEH_BIG)
			pgpa = (pte.pte_lo & PTEL_LP_PA_MASK) |
			    (va & ~PPC64_PAGE_MASK & LP_PAGE_MASK);
		else
			pgpa = pte.pte_lo & PTEL_PA_MASK;
		dprintf("%s: pgpa=0x%016jx\n", __func__, (uintmax_t)pgpa);

		ptoff = _kvm_pt_find(kd, pgpa, PPC64_PAGE_SIZE);
		if (ptoff == -1) {
			_kvm_err(kd, kd->program, "%s: "
			    "physical address 0x%jx not in minidump",
			    __func__, (uintmax_t)pgpa);
			goto invalid;
		}
		*pa = ptoff + pgoff;
		return (PPC64_PAGE_SIZE - pgoff);
	} else {
		_kvm_err(kd, kd->program,
		    "%s: virtual address 0x%jx not minidumped",
		    __func__, (uintmax_t)va);
		goto invalid;
	}

invalid:
	_kvm_err(kd, 0, "invalid address (0x%jx)", (uintmax_t)va);
	return (0);
}

static vm_prot_t
entry_to_prot(ppc64_pt_entry_t *pte)
{
	vm_prot_t prot = VM_PROT_READ;

	if (pte->pte_lo & LPTEL_RW)
		prot |= VM_PROT_WRITE;
	if ((pte->pte_lo & LPTEL_NOEXEC) != 0)
		prot |= VM_PROT_EXECUTE;
	return (prot);
}

static ppc64_slb_entry_t *
slb_vsid_search(kvm_t *kd, uint64_t vsid)
{
	struct hpt_data *data;
	ppc64_slb_entry_t *slb;
	int i, n;

	data = PPC64_MMU_DATA(kd);
	slb = data->slbs;
	n = data->slbsize / sizeof(ppc64_slb_entry_t);
	vsid <<= SLBV_VSID_SHIFT;

	/* SLB search */
	for (i = 0; i < n; i++, slb++) {
		/* Check if valid and compare VSID */
		if ((slb->slbe & SLBE_VALID) &&
		    (slb->slbv & SLBV_VSID_MASK) == vsid)
			break;
	}

	/* SLB not found */
	if (i == n) {
		_kvm_err(kd, kd->program,
		    "%s: segment not found for VSID 0x%jx",
		    __func__, (uintmax_t)vsid >> SLBV_VSID_SHIFT);
		return (NULL);
	}
	return (slb);
}

static u_long
get_ea(kvm_t *kd, ppc64_pt_entry_t *pte, u_long ptex)
{
	ppc64_slb_entry_t *slb;
	uint64_t ea, hash, vsid;
	int b, shift;

	/* Find SLB */
	vsid = PTEH_AVA_VSID(pte->pte_hi);
	if ((slb = slb_vsid_search(kd, vsid)) == NULL)
		return (~0UL);

	/* Get ESID part of EA */
	ea = slb->slbe & SLBE_ESID_MASK;

	b = slb->slbv & SLBV_L? LP_PAGE_SHIFT : PPC64_PAGE_SHIFT;

	/*
	 * If there are less than 64K PTEGs (16-bit), the upper bits of
	 * EA page must be obtained from PTEH's AVA.
	 */
	if (kd->vmst->hdr.pmapsize / (8 * sizeof(ppc64_pt_entry_t)) <
	    0x10000U) {
		/*
		 * Add 0 to 5 EA bits, right after VSID.
		 * b == 12: 5 bits
		 * b == 24: 4 bits
		 */
		shift = AVA_PAGE_SHIFT(b);
		ea |= (PTEH_AVA_PAGE(pte->pte_hi) >> shift) <<
		    (SLBE_ESID_SHIFT - 5 + shift);
	}

	/* Get VA page from hash and add to EA. */
	hash = (ptex & ~7) >> 3;
	if (pte->pte_hi & LPTEH_HID)
		hash = ~hash & PTE_HASH_MASK;
	ea |= ((hash ^ (vsid & PTE_HASH_MASK)) << b) & ~SLBE_ESID_MASK;
	return (ea);
}

static int
ppc64mmu_hpt_walk_pages(kvm_t *kd, kvm_walk_pages_cb_t *cb, void *arg)
{
	struct vmstate *vm;
	int ret;
	unsigned int pagesz;
	u_long dva, pa, va;
	u_long ptex, nptes;
	uint64_t vsid;

	ret = 0;
	vm = kd->vmst;
	nptes = vm->hdr.pmapsize / sizeof(ppc64_pt_entry_t);

	/* Walk through PTEs */
	for (ptex = 0; ptex < nptes; ptex++) {
		ppc64_pt_entry_t pte = pte_get(kd, ptex);
		if ((pte.pte_hi & LPTEH_VALID) == 0)
			continue;

		/* Skip non-kernel related pages, as well as VRMA ones */
		vsid = PTEH_AVA_VSID(pte.pte_hi);
		if ((vsid & KERNEL_VSID_BIT) == 0 ||
		    (vsid >> PPC64_PAGE_SHIFT) == VSID_VRMA)
			continue;

		/* Retrieve page's VA (EA on PPC64 terminology) */
		if ((va = get_ea(kd, &pte, ptex)) == ~0UL)
			goto out;

		/* Get PA and page size */
		if (pte.pte_hi & LPTEH_BIG) {
			pa = pte.pte_lo & PTEL_LP_PA_MASK;
			pagesz = LP_PAGE_SIZE;
		} else {
			pa = pte.pte_lo & PTEL_PA_MASK;
			pagesz = PPC64_PAGE_SIZE;
		}

		/* Get DMAP address */
		dva = vm->hdr.dmapbase + pa;

		if (!_kvm_visit_cb(kd, cb, arg, pa, va, dva,
		    entry_to_prot(&pte), pagesz, 0))
			goto out;
	}
	ret = 1;

out:
	return (ret);
}


static struct ppc64_mmu_ops ops = {
	.init		= ppc64mmu_hpt_init,
	.cleanup	= ppc64mmu_hpt_cleanup,
	.kvatop		= ppc64mmu_hpt_kvatop,
	.walk_pages	= ppc64mmu_hpt_walk_pages,
};
struct ppc64_mmu_ops *ppc64_mmu_ops_hpt = &ops;
