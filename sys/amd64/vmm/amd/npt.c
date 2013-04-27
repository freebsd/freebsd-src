/*-
 * Copyright (c) 2013 Anish Gupta (akgupt3@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>

#include "svm.h"
#include "vmcb.h"
#include "svm_softc.h"
#include "npt.h"

/*
 * "Nested Paging" is an optional SVM feature that provides two levels of
 * address translation, thus eliminating the need for the VMM to maintain
 * shadow page tables.
 *
 * Documented in APMv2, section 15.25, Nested Paging.
 */

#define PAGE_4KB		(4 * 1024)
#define PAGE_2MB		(2 * 1024 * 1024UL)
#define PAGE_1GB		(1024 * 1024 * 1024UL)

#define GPA_UNMAPPED		((vm_paddr_t)~0)

/* Get page entry to physical address. */
#define PTE2PA(x)		((uint64_t)(x) & ~PAGE_MASK)

MALLOC_DECLARE(M_SVM);

static uint64_t svm_npt_create(pml4_entry_t *pml4, vm_paddr_t gpa, 
				vm_paddr_t hpa, vm_memattr_t attr, 
				int prot, uint64_t size);

static const int PT_INDEX_MASK = 0x1FF;
static const int PT_SHIFT = 9;

/*
 * Helper function to create nested page table entries for a page
 * of size 1GB, 2MB or 4KB.
 *
 * Starting from PML4 create a PDPTE, PDE or PTE depending on 'pg_size'
 * value of 1GB, 2MB or 4KB respectively.
 *
 * Return size of the mapping created on success and 0 on failure.
 *
 * XXX: NPT PAT settings. 
 */
static  uint64_t
svm_npt_create(pml4_entry_t * pml4, vm_paddr_t gpa, vm_paddr_t hpa,
    		vm_memattr_t attr, int prot, uint64_t pg_size)
{
	uint64_t *pt, *page, pa;
	pt_entry_t mode;
	int shift, index;

	KASSERT(pg_size, ("Size of page must be 1GB, 2MB or 4KB"));
	if (hpa & (pg_size - 1)) {
		ERR("HPA(0x%lx) is not aligned, size:0x%lx\n", hpa, pg_size);
		return (0);
	}

	if (gpa & (pg_size - 1)) {
		ERR("GPA(0x%lx) is not aligned, size (0x%lx)\n", gpa, pg_size);
		return (0);
	}

	/* Find out mode bits for PTE */
	mode = PG_U;
	if (prot & VM_PROT_WRITE)
		mode |= PG_RW;
	if ((prot & VM_PROT_EXECUTE) == 0) 	
		mode |= pg_nx;
	if (prot != VM_PROT_NONE) 
		mode |= PG_V;
		
	pt = (uint64_t *)pml4;
	shift = PML4SHIFT;

	while ((shift > PAGE_SHIFT) && (pg_size < (1UL << shift))) {
		/* Get PDP, PD or PT index from guest physical address. */
		index = (gpa >> shift) & PT_INDEX_MASK;

		/* If page entry is missing, allocate new page for table.*/
		if (pt[index] == 0) {
			page = malloc(PAGE_SIZE, M_SVM, M_WAITOK | M_ZERO);
			pt[index] = vtophys(page) | mode;
		}

		pa = PTE2PA(pt[index]);;
		pt = (uint64_t *)PHYS_TO_DMAP(pa);
		shift -= PT_SHIFT;
	}

	/* Create leaf entry mapping. */
	index = (gpa >> shift) & PT_INDEX_MASK;
	if (pt[index] != 0) {
		ERR("Mapping already exists.\n");
		return (0);
	}

	pt[index] = hpa | mode;
	
	/* If its not last level page table entry, set PS bit. */
	if (pg_size > PAGE_SIZE) {
		pt[index] |= PG_PS;
	}

	return (1UL << shift);
}

/*
 * Map guest physical address to host physical address.
 */
int
svm_npt_vmmap_set(void *arg, vm_paddr_t gpa, vm_paddr_t hpa,
	size_t size, vm_memattr_t attr, int prot, boolean_t spok)
{
	pml4_entry_t *pml4;
	struct svm_softc *svm_sc;
	uint64_t len, mapped, pg_size;

	svm_sc = arg;
	pml4 = svm_sc->np_pml4;

	pg_size = PAGE_4KB;
	if (spok) {
		pg_size = PAGE_2MB;
		if (amd_feature & AMDID_PAGE1GB)
			pg_size = PAGE_1GB;
	}

	/* Compute the largest page mapping that can be used */
	while (pg_size > PAGE_4KB) {
		if (size >= pg_size &&
		    (gpa & (pg_size - 1)) == 0 &&
		    (hpa & (pg_size - 1)) == 0) {
			break;
		}
		pg_size >>= PT_SHIFT;
	}

	len = 0;
	while (len < size) {
		mapped = svm_npt_create(pml4, gpa + len, hpa + len, attr, prot,
					pg_size);
		if (mapped == 0) {
			panic("Couldn't map GPA:0x%lx, size:0x%lx", gpa, 
				pg_size);
		}

		len += mapped;
	}

	return (0);
}

/*
 * Get HPA for a given GPA.
 */
vm_paddr_t
svm_npt_vmmap_get(void *arg, vm_paddr_t gpa)
{
	struct svm_softc *svm_sc;
	pml4_entry_t *pml4;
	uint64_t *pt, pa, hpa, pgmask;
	int shift, index;

	svm_sc = arg;
	pml4 = svm_sc->np_pml4;

	pt = (uint64_t *)pml4;
	shift = PML4SHIFT;
	
	while (shift > PAGE_SHIFT) {
		 /* Get PDP, PD or PT index from GPA */
		index = (gpa >> shift) & PT_INDEX_MASK;
		if (pt[index] == 0) {
			ERR("No entry for GPA:0x%lx.", gpa);
			return (GPA_UNMAPPED);
		}

		if (pt[index] & PG_PS) {
			break;
		}

		pa = PTE2PA(pt[index]);;
		pt = (uint64_t *)PHYS_TO_DMAP(pa);
		shift -= PT_SHIFT;
	}

	index = (gpa >> shift) & PT_INDEX_MASK;
	if (pt[index] == 0) {
		ERR("No mapping for GPA:0x%lx.\n", gpa);
		return (GPA_UNMAPPED);
	}

	/* Add GPA offset to HPA */
	pgmask = (1UL << shift) - 1;
	hpa = (PTE2PA(pt[index]) & ~pgmask) | (gpa & pgmask);

	return (hpa);
}

/*
 * AMD nested page table init.
 */
int
svm_npt_init(void)
{
	
	return (0);
}

/*
 * Free Page Table page.
 */
static void
free_pt(pd_entry_t pde)
{
	pt_entry_t *pt;

	pt = (pt_entry_t *)PHYS_TO_DMAP(PTE2PA(pde));
	free(pt, M_SVM);
}

/*
 * Free Page Directory page.
 */
static void
free_pd(pdp_entry_t pdpe)
{
	pd_entry_t *pd;
	int i;

	pd = (pd_entry_t *)PHYS_TO_DMAP(PTE2PA(pdpe));
	for (i = 0; i < NPDEPG; i++) {
		/* Skip not-present or superpage entries */
		if ((pd[i] == 0) || (pd[i] & PG_PS))
			continue;

		free_pt(pd[i]);
	}

	free(pd, M_SVM);
}

/*
 * Free Page Directory Pointer page.
 */
static void
free_pdp(pml4_entry_t pml4e)
{
	pdp_entry_t *pdp;
	int i;

	pdp = (pdp_entry_t *)PHYS_TO_DMAP(PTE2PA(pml4e));
	for (i = 0; i < NPDPEPG; i++) {
		/* Skip not-present or superpage entries */
		if ((pdp[i] == 0) || (pdp[i] & PG_PS))
			continue;

		free_pd(pdp[i]);
	}

	free(pdp, M_SVM);
}

/*
 * Free the guest's nested page table.
 */
int
svm_npt_cleanup(struct svm_softc *svm_sc)
{
	pml4_entry_t *pml4;
	int i;

	pml4 = svm_sc->np_pml4;

	for (i = 0; i < NPML4EPG; i++) {
		if (pml4[i] != 0) {
			free_pdp(pml4[i]);
			pml4[i] = 0;
		}
	}

	return (0);
}
