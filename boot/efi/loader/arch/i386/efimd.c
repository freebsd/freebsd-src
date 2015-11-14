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

#include <libi386.h>
#include <machine/bootinfo.h>

#define EFI_INTEL_FPSWA		\
    {0xc41b6531,0x97b9,0x11d3,{0x9a,0x29,0x00,0x90,0x27,0x3f,0xc1,0x4d}}

static EFI_GUID fpswa_guid = EFI_INTEL_FPSWA;

/* DIG64 Headless Console & Debug Port Table. */
#define	HCDP_TABLE_GUID		\
    {0xf951938d,0x620b,0x42ef,{0x82,0x79,0xa8,0x4b,0x79,0x61,0x78,0x98}}

static EFI_GUID hcdp_guid = HCDP_TABLE_GUID;

static UINTN mapkey;

uint64_t
ldr_alloc(vm_offset_t va)
{

	return (0);
}

int
ldr_bootinfo(struct bootinfo *bi, uint64_t *bi_addr)
{
	VOID *fpswa;
	EFI_MEMORY_DESCRIPTOR *mm;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_HANDLE handle;
	EFI_STATUS status;
	size_t bisz;
	UINTN mmsz, pages, sz;
	UINT32 mmver;

	bi->bi_systab = (uint64_t)ST;
	bi->bi_hcdp = (uint64_t)efi_get_table(&hcdp_guid);

	sz = sizeof(EFI_HANDLE);
	status = BS->LocateHandle(ByProtocol, &fpswa_guid, 0, &sz, &handle);
	if (status == 0)
		status = BS->HandleProtocol(handle, &fpswa_guid, &fpswa);
	bi->bi_fpswa = (status == 0) ? (uint64_t)fpswa : 0;

	bisz = (sizeof(struct bootinfo) + 0x0f) & ~0x0f;

	/*
	 * Allocate enough pages to hold the bootinfo block and the memory
	 * map EFI will return to us. The memory map has an unknown size,
	 * so we have to determine that first. Note that the AllocatePages
	 * call can itself modify the memory map, so we have to take that
	 * into account as well. The changes to the memory map are caused
	 * by splitting a range of free memory into two (AFAICT), so that
	 * one is marked as being loader data.
	 */
	sz = 0;
	BS->GetMemoryMap(&sz, NULL, &mapkey, &mmsz, &mmver);
	sz += mmsz;
	sz = (sz + 15) & ~15;
	pages = EFI_SIZE_TO_PAGES(sz + bisz);
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages,
	    &addr);
	if (EFI_ERROR(status)) {
		printf("%s: AllocatePages() returned 0x%lx\n", __func__,
		    (long)status);
		return (ENOMEM);
	}

	/*
	 * Read the memory map and stash it after bootinfo. Align the
	 * memory map on a 16-byte boundary (the bootinfo block is page
	 * aligned).
	 */
	*bi_addr = addr;
	mm = (void *)(addr + bisz);
	sz = (EFI_PAGE_SIZE * pages) - bisz;
	status = BS->GetMemoryMap(&sz, mm, &mapkey, &mmsz, &mmver);
	if (EFI_ERROR(status)) {
		printf("%s: GetMemoryMap() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}
	bi->bi_memmap = (uint64_t)mm;
	bi->bi_memmap_size = sz;
	bi->bi_memdesc_size = mmsz;
	bi->bi_memdesc_version = mmver;

	bcopy(bi, (void *)(*bi_addr), sizeof(*bi));
	return (0);
}

int
ldr_enter(const char *kernel)
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
