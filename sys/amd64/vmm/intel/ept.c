/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/param.h>
#include <machine/cpufunc.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include "vmx_cpufunc.h"
#include "vmx_msr.h"
#include "vmx.h"
#include "ept.h"

#define	EPT_PWL4(cap)			((cap) & (1UL << 6))
#define	EPT_MEMORY_TYPE_WB(cap)		((cap) & (1UL << 14))
#define	EPT_PDE_SUPERPAGE(cap)		((cap) & (1UL << 16))	/* 2MB pages */
#define	EPT_PDPTE_SUPERPAGE(cap)	((cap) & (1UL << 17))	/* 1GB pages */
#define	INVVPID_SUPPORTED(cap)		((cap) & (1UL << 32))
#define	INVEPT_SUPPORTED(cap)		((cap) & (1UL << 20))

#define	INVVPID_ALL_TYPES_MASK		0xF0000000000UL
#define	INVVPID_ALL_TYPES_SUPPORTED(cap)	\
	(((cap) & INVVPID_ALL_TYPES_MASK) == INVVPID_ALL_TYPES_MASK)

#define	INVEPT_ALL_TYPES_MASK		0x6000000UL
#define	INVEPT_ALL_TYPES_SUPPORTED(cap)		\
	(((cap) & INVEPT_ALL_TYPES_MASK) == INVEPT_ALL_TYPES_MASK)

#define	EPT_PG_RD			(1 << 0)
#define	EPT_PG_WR			(1 << 1)
#define	EPT_PG_EX			(1 << 2)
#define	EPT_PG_MEMORY_TYPE(x)		((x) << 3)
#define	EPT_PG_IGNORE_PAT		(1 << 6)
#define	EPT_PG_SUPERPAGE		(1 << 7)

#define	EPT_ADDR_MASK			((uint64_t)-1 << 12)

MALLOC_DECLARE(M_VMX);

static uint64_t page_sizes_mask;

/*
 * Set this to 1 to have the EPT tables respect the guest PAT settings
 */
static int ept_pat_passthru;

int
ept_init(void)
{
	int page_shift;
	uint64_t cap;

	cap = rdmsr(MSR_VMX_EPT_VPID_CAP);

	/*
	 * Verify that:
	 * - page walk length is 4 steps
	 * - extended page tables can be laid out in write-back memory
	 * - invvpid instruction with all possible types is supported
	 * - invept instruction with all possible types is supported
	 */
	if (!EPT_PWL4(cap) ||
	    !EPT_MEMORY_TYPE_WB(cap) ||
	    !INVVPID_SUPPORTED(cap) ||
	    !INVVPID_ALL_TYPES_SUPPORTED(cap) ||
	    !INVEPT_SUPPORTED(cap) ||
	    !INVEPT_ALL_TYPES_SUPPORTED(cap))
		return (EINVAL);

	/* Set bits in 'page_sizes_mask' for each valid page size */
	page_shift = PAGE_SHIFT;
	page_sizes_mask = 1UL << page_shift;		/* 4KB page */

	page_shift += 9;
	if (EPT_PDE_SUPERPAGE(cap))
		page_sizes_mask |= 1UL << page_shift;	/* 2MB superpage */

	page_shift += 9;
	if (EPT_PDPTE_SUPERPAGE(cap))
		page_sizes_mask |= 1UL << page_shift;	/* 1GB superpage */

	return (0);
}

#if 0
static void
ept_dump(uint64_t *ptp, int nlevels)
{
	int i, t, tabs;
	uint64_t *ptpnext, ptpval;

	if (--nlevels < 0)
		return;

	tabs = 3 - nlevels;
	for (t = 0; t < tabs; t++)
		printf("\t");
	printf("PTP = %p\n", ptp);

	for (i = 0; i < 512; i++) {
		ptpval = ptp[i];

		if (ptpval == 0)
			continue;
		
		for (t = 0; t < tabs; t++)
			printf("\t");
		printf("%3d 0x%016lx\n", i, ptpval);

		if (nlevels != 0 && (ptpval & EPT_PG_SUPERPAGE) == 0) {
			ptpnext = (uint64_t *)
				  PHYS_TO_DMAP(ptpval & EPT_ADDR_MASK);
			ept_dump(ptpnext, nlevels);
		}
	}
}
#endif

static size_t
ept_create_mapping(uint64_t *ptp, vm_paddr_t gpa, vm_paddr_t hpa, size_t length,
		   vm_memattr_t attr, vm_prot_t prot, boolean_t spok)
{
	int spshift, ptpshift, ptpindex, nlevels;

	/*
	 * Compute the size of the mapping that we can accomodate.
	 *
	 * This is based on three factors:
	 * - super page sizes supported by the processor
	 * - alignment of the region starting at 'gpa' and 'hpa'
	 * - length of the region 'len'
	 */
	spshift = PAGE_SHIFT;
	if (spok)
		spshift += (EPT_PWLEVELS - 1) * 9;
	while (spshift >= PAGE_SHIFT) {
		uint64_t spsize = 1UL << spshift;
		if ((page_sizes_mask & spsize) != 0 &&
		    (gpa & (spsize - 1)) == 0 &&
		    (hpa & (spsize - 1)) == 0 &&
		    length >= spsize) {
			break;
		}
		spshift -= 9;
	}

	if (spshift < PAGE_SHIFT) {
		panic("Invalid spshift for gpa 0x%016lx, hpa 0x%016lx, "
		      "length 0x%016lx, page_sizes_mask 0x%016lx",
		      gpa, hpa, length, page_sizes_mask);
	}

	nlevels = EPT_PWLEVELS;
	while (--nlevels >= 0) {
		ptpshift = PAGE_SHIFT + nlevels * 9;
		ptpindex = (gpa >> ptpshift) & 0x1FF;

		/* We have reached the leaf mapping */
		if (spshift >= ptpshift)
			break;

		/*
		 * We are working on a non-leaf page table page.
		 *
		 * Create the next level page table page if necessary and point
		 * to it from the current page table.
		 */
		if (ptp[ptpindex] == 0) {
			void *nlp = malloc(PAGE_SIZE, M_VMX, M_WAITOK | M_ZERO);
			ptp[ptpindex] = vtophys(nlp);
			ptp[ptpindex] |= EPT_PG_RD | EPT_PG_WR | EPT_PG_EX;
		}

		/* Work our way down to the next level page table page */
		ptp = (uint64_t *)PHYS_TO_DMAP(ptp[ptpindex] & EPT_ADDR_MASK);
	}

	if ((gpa & ((1UL << ptpshift) - 1)) != 0) {
		panic("ept_create_mapping: gpa 0x%016lx and ptpshift %d "
		      "mismatch\n", gpa, ptpshift);
	}

	if (prot != VM_PROT_NONE) {
		/* Do the mapping */
		ptp[ptpindex] = hpa;

		/* Apply the access controls */
		if (prot & VM_PROT_READ)
			ptp[ptpindex] |= EPT_PG_RD;
		if (prot & VM_PROT_WRITE)
			ptp[ptpindex] |= EPT_PG_WR;
		if (prot & VM_PROT_EXECUTE)
			ptp[ptpindex] |= EPT_PG_EX;

		/*
		 * By default the PAT type is ignored - this appears to
		 * be how other hypervisors handle EPT. Allow this to be
		 * overridden.
		 */
		ptp[ptpindex] |= EPT_PG_MEMORY_TYPE(attr);
		if (!ept_pat_passthru)
			ptp[ptpindex] |= EPT_PG_IGNORE_PAT;

		if (nlevels > 0)
			ptp[ptpindex] |= EPT_PG_SUPERPAGE;
	} else {
		/* Remove the mapping */
		ptp[ptpindex] = 0;
	}

	return (1UL << ptpshift);
}

static vm_paddr_t
ept_lookup_mapping(uint64_t *ptp, vm_paddr_t gpa)
{
	int nlevels, ptpshift, ptpindex;
	uint64_t ptpval, hpabase, pgmask;

	nlevels = EPT_PWLEVELS;
	while (--nlevels >= 0) {
		ptpshift = PAGE_SHIFT + nlevels * 9;
		ptpindex = (gpa >> ptpshift) & 0x1FF;

		ptpval = ptp[ptpindex];

		/* Cannot make progress beyond this point */
		if ((ptpval & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)) == 0)
			break;

		if (nlevels == 0 || (ptpval & EPT_PG_SUPERPAGE)) {
			pgmask = (1UL << ptpshift) - 1;
			hpabase = ptpval & ~pgmask;
			return (hpabase | (gpa & pgmask));
		}

		/* Work our way down to the next level page table page */
		ptp = (uint64_t *)PHYS_TO_DMAP(ptpval & EPT_ADDR_MASK);
	}

	return ((vm_paddr_t)-1);
}

static void
ept_free_pt_entry(pt_entry_t pte)
{
	if (pte == 0)
		return;

	/* sanity check */
	if ((pte & EPT_PG_SUPERPAGE) != 0)
		panic("ept_free_pt_entry: pte cannot have superpage bit");

	return;
}

static void
ept_free_pd_entry(pd_entry_t pde)
{
	pt_entry_t	*pt;
	int		i;

	if (pde == 0)
		return;

	if ((pde & EPT_PG_SUPERPAGE) == 0) {
		pt = (pt_entry_t *)PHYS_TO_DMAP(pde & EPT_ADDR_MASK);
		for (i = 0; i < NPTEPG; i++)
			ept_free_pt_entry(pt[i]);
		free(pt, M_VMX);	/* free the page table page */
	}
}

static void
ept_free_pdp_entry(pdp_entry_t pdpe)
{
	pd_entry_t 	*pd;
	int		 i;

	if (pdpe == 0)
		return;

	if ((pdpe & EPT_PG_SUPERPAGE) == 0) {
		pd = (pd_entry_t *)PHYS_TO_DMAP(pdpe & EPT_ADDR_MASK);
		for (i = 0; i < NPDEPG; i++)
			ept_free_pd_entry(pd[i]);
		free(pd, M_VMX);	/* free the page directory page */
	}
}

static void
ept_free_pml4_entry(pml4_entry_t pml4e)
{
	pdp_entry_t	*pdp;
	int		i;

	if (pml4e == 0)
		return;

	if ((pml4e & EPT_PG_SUPERPAGE) == 0) {
		pdp = (pdp_entry_t *)PHYS_TO_DMAP(pml4e & EPT_ADDR_MASK);
		for (i = 0; i < NPDPEPG; i++)
			ept_free_pdp_entry(pdp[i]);
		free(pdp, M_VMX);	/* free the page directory ptr page */
	}
}

void
ept_vmcleanup(struct vmx *vmx)
{
	int 		 i;

	for (i = 0; i < NPML4EPG; i++)
		ept_free_pml4_entry(vmx->pml4ept[i]);
}

int
ept_vmmmap_set(void *arg, vm_paddr_t gpa, vm_paddr_t hpa, size_t len,
		vm_memattr_t attr, int prot, boolean_t spok)
{
	size_t n;
	struct vmx *vmx = arg;

	while (len > 0) {
		n = ept_create_mapping(vmx->pml4ept, gpa, hpa, len, attr,
				       prot, spok);
		len -= n;
		gpa += n;
		hpa += n;
	}

	return (0);
}

vm_paddr_t
ept_vmmmap_get(void *arg, vm_paddr_t gpa)
{
	vm_paddr_t hpa;
	struct vmx *vmx;

	vmx = arg;
	hpa = ept_lookup_mapping(vmx->pml4ept, gpa);
	return (hpa);
}

static void
invept_single_context(void *arg)
{
	struct invept_desc desc = *(struct invept_desc *)arg;

	invept(INVEPT_TYPE_SINGLE_CONTEXT, desc);
}

void
ept_invalidate_mappings(u_long pml4ept)
{
	struct invept_desc invept_desc = { 0 };

	invept_desc.eptp = EPTP(pml4ept);

	smp_rendezvous(NULL, invept_single_context, NULL, &invept_desc);
}
