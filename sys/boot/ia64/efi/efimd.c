/*-
 * Copyright (c) 2004, 2006 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>

#include <efi.h>
#include <efilib.h>

#include <libia64.h>

#define EFI_INTEL_FPSWA		\
    {0xc41b6531,0x97b9,0x11d3,{0x9a,0x29,0x00,0x90,0x27,0x3f,0xc1,0x4d}}

static EFI_GUID fpswa_guid = EFI_INTEL_FPSWA;

/* DIG64 Headless Console & Debug Port Table. */
#define	HCDP_TABLE_GUID		\
    {0xf951938d,0x620b,0x42ef,{0x82,0x79,0xa8,0x4b,0x79,0x61,0x78,0x98}}

static EFI_GUID hcdp_guid = HCDP_TABLE_GUID;

static EFI_MEMORY_DESCRIPTOR *memmap;
static UINTN memmapsz;
static UINTN mapkey;
static UINTN descsz;
static UINT32 descver;

#define	IA64_EFI_CHUNK_SIZE	(32 * 1048576)
static vm_paddr_t ia64_efi_chunk;

#define	IA64_EFI_PGTBLSZ_MAX	1048576
static vm_paddr_t ia64_efi_pgtbl;
static vm_size_t ia64_efi_pgtblsz;

/* Don't allocate memory below the boundary */
#define	IA64_EFI_ALLOC_BOUNDARY	1048576

static int
ia64_efi_memmap_update(void)
{
	EFI_STATUS status;

	if (memmap != NULL) {
		free(memmap);
		memmap = NULL;
	}

	memmapsz = 0;
	BS->GetMemoryMap(&memmapsz, NULL, &mapkey, &descsz, &descver);
	if (memmapsz == 0)
		return (FALSE);
	memmap = malloc(memmapsz);
	if (memmap == NULL)
		return (FALSE);

	status = BS->GetMemoryMap(&memmapsz, memmap, &mapkey, &descsz,
	    &descver);
	if (EFI_ERROR(status)) {
		free(memmap);
		memmap = NULL;
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Returns 0 on failure. Successful allocations return an address
 * larger or equal to IA64_EFI_ALLOC_BOUNDARY.
 */
static vm_paddr_t
ia64_efi_alloc(vm_size_t sz)
{
	EFI_PHYSICAL_ADDRESS pa;
	EFI_MEMORY_DESCRIPTOR *mm;
	uint8_t *mmiter, *mmiterend;
	vm_size_t memsz;
	UINTN npgs;
	EFI_STATUS status;

	/* We can't allocate less than a page */
	if (sz < EFI_PAGE_SIZE)
		return (0);

	/* The size must be a power of 2. */
	if (sz & (sz - 1))
		return (0);

	if (!ia64_efi_memmap_update())
		return (0);

	mmiter = (void *)memmap;
	mmiterend = mmiter + memmapsz;
	for (; mmiter < mmiterend; mmiter += descsz) {
		mm = (void *)mmiter;
		if (mm->Type != EfiConventionalMemory)
			continue;
		memsz = mm->NumberOfPages * EFI_PAGE_SIZE;
		if (mm->PhysicalStart + memsz <= IA64_EFI_ALLOC_BOUNDARY)
			continue;
		/*
		 * XXX We really should make sure the memory is local to the
		 * BSP.
		 */
		pa = (mm->PhysicalStart < IA64_EFI_ALLOC_BOUNDARY) ?
		    IA64_EFI_ALLOC_BOUNDARY : mm->PhysicalStart;
		pa  = (pa + sz - 1) & ~(sz - 1);
		if (pa + sz > mm->PhysicalStart + memsz)
			continue;

		npgs = EFI_SIZE_TO_PAGES(sz);
		status = BS->AllocatePages(AllocateAddress, EfiLoaderData,
		    npgs, &pa);
		if (!EFI_ERROR(status))
			return (pa);
	}

	printf("%s: unable to allocate %lx bytes\n", __func__, sz);
	return (0);
}

vm_paddr_t
ia64_platform_alloc(vm_offset_t va, vm_size_t sz)
{
	vm_paddr_t pa;

	if (va == 0) {
		/* Page table itself. */
		if (sz > IA64_EFI_PGTBLSZ_MAX)
			return (~0UL);
		if (ia64_efi_pgtbl == 0)
			ia64_efi_pgtbl = ia64_efi_alloc(IA64_EFI_PGTBLSZ_MAX);
		if (ia64_efi_pgtbl != 0)
			ia64_efi_pgtblsz = sz;
		return (ia64_efi_pgtbl);
	} else if (va < IA64_PBVM_BASE) {
		/* Should not happen. */
		return (~0UL);
	}

	/* Loader virtual memory page. */
	va -= IA64_PBVM_BASE;

	/* Allocate a big chunk that can be wired with a single PTE. */
	if (ia64_efi_chunk == 0)
		ia64_efi_chunk = ia64_efi_alloc(IA64_EFI_CHUNK_SIZE);
	if (va < IA64_EFI_CHUNK_SIZE)
		return (ia64_efi_chunk + va);

	/* Allocate a page at a time when we go beyond the chunk. */
	pa = ia64_efi_alloc(sz);
	return ((pa == 0) ? ~0UL : pa);
}

void
ia64_platform_free(vm_offset_t va, vm_paddr_t pa, vm_size_t sz)
{

	BS->FreePages(pa, sz >> EFI_PAGE_SHIFT);
}

int
ia64_platform_bootinfo(struct bootinfo *bi, struct bootinfo **res)
{
	VOID *fpswa;
	EFI_HANDLE handle;
	EFI_STATUS status;
	UINTN sz;

	bi->bi_systab = (uint64_t)ST;
	bi->bi_hcdp = (uint64_t)efi_get_table(&hcdp_guid);

	sz = sizeof(EFI_HANDLE);
	status = BS->LocateHandle(ByProtocol, &fpswa_guid, 0, &sz, &handle);
	if (status == 0)
		status = BS->HandleProtocol(handle, &fpswa_guid, &fpswa);
	bi->bi_fpswa = (status == 0) ? (uint64_t)fpswa : 0;

	if (!ia64_efi_memmap_update())
		return (ENOMEM);

	bi->bi_memmap = (uint64_t)memmap;
	bi->bi_memmap_size = memmapsz;
	bi->bi_memdesc_size = descsz;
	bi->bi_memdesc_version = descver;

	if (IS_LEGACY_KERNEL())
		*res = malloc(sizeof(**res));

	return (0);
}

int
ia64_platform_enter(const char *kernel)
{
	EFI_STATUS status;

	status = BS->ExitBootServices(IH, mapkey);
	if (EFI_ERROR(status)) {
		printf("%s: ExitBootServices() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	return (0);
}
