/*-
 * Copyright (c) 2006 Marcel Moolenaar
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
#include <string.h>

#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>

#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>

#include "libarm64.h"
#include <machine/bootinfo.h>

static int elf64_exec(struct preloaded_file *amp);
static int elf64_obj_exec(struct preloaded_file *amp);

static struct file_format arm64_elf = {
	elf64_loadfile,
	elf64_exec
};

struct file_format *file_formats[] = {
	&arm64_elf,
	NULL
};

static int
elf64_exec(struct preloaded_file *fp)
{
	struct file_metadata *md;
	struct bootinfo *bi;
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *memmap;
	EFI_PHYSICAL_ADDRESS addr;
	UINTN descsz, memmapsz, mapkey, pages;
	UINT32 descver;
	Elf_Ehdr *ehdr;
	void (*entry)(void *);

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
        	return(EFTYPE);

	ehdr = (Elf_Ehdr *)&(md->md_data);

	entry = arm64_efi_translate(ehdr->e_entry);

	memmapsz = 0;
	status = BS->GetMemoryMap(&memmapsz, NULL, &mapkey, &descsz, &descver);
        if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
		printf("%s: GetMemoryMap() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	memmapsz = roundup2(memmapsz, 16);
	pages = EFI_SIZE_TO_PAGES(memmapsz + sizeof(*bi));
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages,
	    &addr);
        if (EFI_ERROR(status)) {
		free(memmap);
		printf("%s: AllocatePages() returned 0x%lx\n", __func__,
		    (long)status);
		return (ENOMEM);
	}

	memmap = (void *)(addr + sizeof(*bi));
	status = BS->GetMemoryMap(&memmapsz, memmap, &mapkey, &descsz,
		&descver);
        if (EFI_ERROR(status)) {
		free(memmap);
		printf("%s: GetMemoryMap() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	bi = (void *)addr;

	bi->bi_magic = BOOTINFO_MAGIC;
	bi->bi_version = BOOTINFO_VERSION;

	bi->bi_memmap = (uint64_t)memmap;
	bi->bi_memmap_size = memmapsz;
	bi->bi_memdesc_size = descsz;
	bi->bi_memdesc_version = descver;

#if 0
	/* Find a location for the bootinfo after the last module */
	addr = 0;
	for (md = file_findfile(NULL, NULL); md != NULL; md = md->f_next) {
		if (addr < (md->f_addr + md->f_size))
			addr = md->f_addr + md->f_size;
	}
	addr = roundup2(addr, 16);
	arm64_copyin(&bi, addr, sizeof(bi));
#endif

	status = BS->ExitBootServices(IH, mapkey);
        if (EFI_ERROR(status)) {
		printf("%s: ExitBootServices() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	/* TODO: Pass the required metadata to the kernel */
	(*entry)(bi);
	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	printf("%s called for preloaded file %p (=%s):\n", __func__, fp,
	    fp->f_name);
	return (ENOSYS);
}

