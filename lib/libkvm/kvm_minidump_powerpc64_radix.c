/*
 * Copyright (c) 2026 FreeBSD Foundation
 *
 * This software was developed by Minsoo Choo under sponsorship from the
 * FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <vm/vm.h>

#include <kvm.h>

#include <limits.h>
#include <stdint.h>

#include "../../sys/powerpc/include/minidump.h"
#include "kvm_private.h"
#include "kvm_powerpc64.h"

/* Radix tree geometry. */
#define	RADIX_ROOT_SHIFT	16
#define	RADIX_ROOT_SIZE		(1UL << RADIX_ROOT_SHIFT)
#define	RADIX_ROOT_ENTRIES	(RADIX_ROOT_SIZE / sizeof(uint64_t))
#define	RADIX_ENTRIES		(PPC64_PAGE_SIZE / sizeof(uint64_t))
#define	RADIX_INDEX_MASK	(RADIX_ENTRIES - 1)

#define	RADIX_L1_SHIFT		39
#define	RADIX_L2_SHIFT		30
#define	RADIX_L3_SHIFT		21

/* Radix PTE fields.  Radix PTEs are stored in big-endian byte order. */
#define	RPTE_VALID		0x8000000000000000ULL
#define	RPTE_LEAF		0x4000000000000000ULL
#define	RPTE_RPN_MASK		0x00fffffffffff000ULL
#define	RPDE_NLB_MASK		0x00ffffffffffff00ULL
#define	RPTE_EAA_R		0x0000000000000004ULL
#define	RPTE_EAA_W		0x0000000000000002ULL
#define	RPTE_EAA_X		0x0000000000000001ULL

static int
radix_root_entry(kvm_t *kd, u_long index, uint64_t *entry)
{
	uint64_t *p;

	p = _kvm_pmap_get(kd, index, sizeof(*p));
	if (p == NULL)
		return (-1);
	*entry = be64toh(*p);
	return (0);
}

static int
radix_table_entry(kvm_t *kd, uint64_t pa, u_long index, uint64_t *entry)
{
	uint64_t *table;

	table = _kvm_map_get(kd, pa, PPC64_PAGE_SIZE);
	if (table == NULL) {
		_kvm_err(kd, kd->program,
		    "radix page table page at 0x%jx was not dumped",
		    (uintmax_t)pa);
		return (-1);
	}
	*entry = be64toh(table[index]);
	return (0);
}

static int
radix_lookup(kvm_t *kd, kvaddr_t va, uint64_t *pap, uint64_t *entryp,
    uint64_t *pageszp)
{
	static const int shifts[] = {
		RADIX_L1_SHIFT, RADIX_L2_SHIFT, RADIX_L3_SHIFT,
		PPC64_PAGE_SHIFT
	};
	uint64_t entry, pa, pagesz;
	u_long index;
	u_int level;

	index = (va >> RADIX_L1_SHIFT) & (RADIX_ROOT_ENTRIES - 1);
	if (radix_root_entry(kd, index, &entry) != 0)
		return (-1);
	for (level = 0; level < nitems(shifts); level++) {
		if ((entry & RPTE_VALID) == 0)
			return (0);
		pagesz = 1ULL << shifts[level];
		if ((entry & RPTE_LEAF) != 0) {
			pa = (entry & RPTE_RPN_MASK) & ~(pagesz - 1);
			*pap = pa | (va & (pagesz - 1));
			*entryp = entry;
			*pageszp = pagesz;
			return (1);
		}
		if (level == nitems(shifts) - 1)
			return (0);
		pa = entry & RPDE_NLB_MASK;
		index = (va >> shifts[level + 1]) & RADIX_INDEX_MASK;
		if (radix_table_entry(kd, pa, index, &entry) != 0)
			return (-1);
	}
	return (0);
}

static int
ppc64mmu_radix_init(kvm_t *kd)
{
	struct minidumphdr *hdr;

	hdr = &kd->vmst->hdr;
	if (hdr->pmapsize == 0) {
		_kvm_err(kd, kd->program,
		    "radix minidump does not contain a page map");
		return (-1);
	}
	if (hdr->pmapsize != RADIX_ROOT_SIZE) {
		_kvm_err(kd, kd->program,
		    "unexpected radix page map size: %u", hdr->pmapsize);
		return (-1);
	}
	return (0);
}

static void
ppc64mmu_radix_cleanup(kvm_t *kd __unused)
{
}

static int
ppc64mmu_radix_kvatop(kvm_t *kd, kvaddr_t va, off_t *offp)
{
	struct minidumphdr *hdr;
	uint64_t entry, pa, pagesz;
	off_t off;
	int found;

	hdr = &kd->vmst->hdr;
	if (va < hdr->dmapbase)
		va += hdr->startkernel - PPC64_KERNBASE;

	if (va >= hdr->dmapbase && va <= hdr->dmapend)
		pa = va - hdr->dmapbase;
	else if (va >= hdr->kernbase) {
		found = radix_lookup(kd, va, &pa, &entry, &pagesz);
		if (found < 0)
			return (0);
		if (found == 0)
			goto invalid;
	} else
		goto invalid;

	off = _kvm_pt_find(kd, trunc_page(pa), PPC64_PAGE_SIZE);
	if (off == -1)
		goto invalid;
	*offp = off + (pa & PPC64_PAGE_MASK);
	return (PPC64_PAGE_SIZE - (pa & PPC64_PAGE_MASK));

invalid:
	_kvm_err(kd, 0, "invalid address (0x%jx)", (uintmax_t)va);
	return (0);
}

#if ULONG_MAX > UINT32_MAX
static vm_prot_t
radix_entry_to_prot(uint64_t entry)
{
	vm_prot_t prot;

	prot = 0;
	if ((entry & (RPTE_EAA_R | RPTE_EAA_W)) != 0)
		prot |= VM_PROT_READ;
	if ((entry & RPTE_EAA_W) != 0)
		prot |= VM_PROT_WRITE;
	if ((entry & RPTE_EAA_X) != 0)
		prot |= VM_PROT_EXECUTE;
	return (prot);
}

static int
radix_walk_table(kvm_t *kd, kvm_walk_pages_cb_t *cb, void *arg,
    uint64_t table_pa, uint64_t va, u_int level)
{
	static const int shifts[] = {
		RADIX_L2_SHIFT, RADIX_L3_SHIFT, PPC64_PAGE_SHIFT
	};
	struct minidumphdr *hdr;
	uint64_t dva, entry, pa, pagesz, subva;
	u_long i;

	hdr = &kd->vmst->hdr;
	for (i = 0; i < RADIX_ENTRIES; i++) {
		if (radix_table_entry(kd, table_pa, i, &entry) != 0)
			return (0);
		if ((entry & RPTE_VALID) == 0)
			continue;
		subva = va | (i << shifts[level]);
		if ((entry & RPTE_LEAF) == 0) {
			if (level == nitems(shifts) - 1)
				continue;
			if (!radix_walk_table(kd, cb, arg,
			    entry & RPDE_NLB_MASK, subva, level + 1))
				return (0);
			continue;
		}
		if (subva < hdr->kernbase)
			continue;
		pagesz = 1ULL << shifts[level];
		pa = (entry & RPTE_RPN_MASK) & ~(pagesz - 1);
		dva = hdr->dmapbase + pa;
		if (!_kvm_visit_cb(kd, cb, arg, pa, subva, dva,
		    radix_entry_to_prot(entry), pagesz, PPC64_PAGE_SIZE))
			return (0);
	}
	return (1);
}

static int
ppc64mmu_radix_walk_pages(kvm_t *kd, kvm_walk_pages_cb_t *cb, void *arg)
{
	struct minidumphdr *hdr;
	uint64_t entry, pa, va;
	u_long i;

	hdr = &kd->vmst->hdr;
	for (i = 0; i < RADIX_ROOT_ENTRIES; i++) {
		if (radix_root_entry(kd, i, &entry) != 0)
			return (0);
		if ((entry & RPTE_VALID) == 0)
			continue;
		va = hdr->dmapbase | ((uint64_t)i << RADIX_L1_SHIFT);
		if (va < hdr->kernbase)
			continue;
		if ((entry & RPTE_LEAF) != 0) {
			pa = (entry & RPTE_RPN_MASK) &
			    ~((1ULL << RADIX_L1_SHIFT) - 1);
			if (!_kvm_visit_cb(kd, cb, arg,
			    pa, va, hdr->dmapbase + pa,
			    radix_entry_to_prot(entry),
			    1ULL << RADIX_L1_SHIFT, PPC64_PAGE_SIZE))
				return (0);
			continue;
		}
		if (!radix_walk_table(kd, cb, arg, entry & RPDE_NLB_MASK,
		    va, 0))
			return (0);
	}
	return (1);
}
#else
static int
ppc64mmu_radix_walk_pages(kvm_t *kd, kvm_walk_pages_cb_t *cb __unused,
    void *arg __unused)
{

	_kvm_err(kd, kd->program,
	    "walking powerpc64 radix pages requires a 64-bit host");
	return (0);
}
#endif

static struct ppc64_mmu_ops ops = {
	.init		= ppc64mmu_radix_init,
	.cleanup	= ppc64mmu_radix_cleanup,
	.kvatop		= ppc64mmu_radix_kvatop,
	.walk_pages	= ppc64mmu_radix_walk_pages,
};

struct ppc64_mmu_ops *ppc64_mmu_ops_radix = &ops;
