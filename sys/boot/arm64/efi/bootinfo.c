/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2004, 2006 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/metadata.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "libarm64.h"

UINTN arm64_efi_mapkey;

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceded by a 32-bit identifier and a 32-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ARGS	(variable)		module parameters (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define COPY32(v, a, c) {				\
	uint32_t x = (v);				\
	if (c)						\
		arm64_efi_copyin(&x, a, sizeof(x));	\
	a += sizeof(x);					\
}

#define MOD_STR(t, a, s, c) {				\
	COPY32(t, a, c);				\
	COPY32(strlen(s) + 1, a, c);			\
	if (c)						\
		arm64_efi_copyin(s, a, strlen(s) + 1);	\
	a += roundup(strlen(s) + 1, sizeof(u_int64_t));	\
}

#define MOD_NAME(a, s, c)	MOD_STR(MODINFO_NAME, a, s, c)
#define MOD_TYPE(a, s, c)	MOD_STR(MODINFO_TYPE, a, s, c)
#define MOD_ARGS(a, s, c)	MOD_STR(MODINFO_ARGS, a, s, c)

#define MOD_VAR(t, a, s, c) {				\
	COPY32(t, a, c);				\
	COPY32(sizeof(s), a, c);			\
	if (c)						\
		arm64_efi_copyin(&s, a, sizeof(s));	\
	a += roundup(sizeof(s), sizeof(u_int64_t));	\
}

#define MOD_ADDR(a, s, c)	MOD_VAR(MODINFO_ADDR, a, s, c)
#define MOD_SIZE(a, s, c)	MOD_VAR(MODINFO_SIZE, a, s, c)

#define MOD_METADATA(a, mm, c) {			\
	COPY32(MODINFO_METADATA | mm->md_type, a, c);	\
	COPY32(mm->md_size, a, c);			\
	if (c)						\
		arm64_efi_copyin(mm->md_data, a, mm->md_size);	\
	a += roundup(mm->md_size, sizeof(u_int64_t));	\
}

#define MOD_END(a, c) {					\
	COPY32(MODINFO_END, a, c);			\
	COPY32(0, a, c);				\
}

static vm_offset_t
bi_copymodules(vm_offset_t addr)
{
	struct preloaded_file	*fp;
	struct file_metadata	*md;
	int			c;
	u_int64_t		v;

	c = addr != 0;
	/* start with the first module on the list, should be the kernel */
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {
		MOD_NAME(addr, fp->f_name, c);	/* this field must come first */
		MOD_TYPE(addr, fp->f_type, c);
		if (fp->f_args)
			MOD_ARGS(addr, fp->f_args, c);
		v = fp->f_addr;
		MOD_ADDR(addr, v, c);
		v = fp->f_size;
		MOD_SIZE(addr, v, c);
		for (md = fp->f_metadata; md != NULL; md = md->md_next)
			if (!(md->md_type & MODINFOMD_NOCOPY))
				MOD_METADATA(addr, md, c);
	}
	MOD_END(addr, c);
	return(addr);
}

static int
bi_load_efi_data(struct preloaded_file *kfp)
{
	EFI_MEMORY_DESCRIPTOR *mm;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;
	size_t efisz;
	UINTN mmsz, pages, sz;
	UINT32 mmver;
	struct efi_map_header *efihdr;

        efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;

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
	BS->GetMemoryMap(&sz, NULL, &arm64_efi_mapkey, &mmsz, &mmver);
	sz += mmsz;
	sz = (sz + 0xf) & ~0xf;
	pages = EFI_SIZE_TO_PAGES(sz + efisz);
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
	efihdr = (struct efi_map_header *)addr;
	mm = (void *)((uint8_t *)efihdr + efisz);
	sz = (EFI_PAGE_SIZE * pages) - efisz;
	status = BS->GetMemoryMap(&sz, mm, &arm64_efi_mapkey, &mmsz, &mmver);
	if (EFI_ERROR(status)) {
		printf("%s: GetMemoryMap() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	efihdr->memory_size = sz;
	efihdr->descriptor_size = mmsz;
	efihdr->descriptor_version = mmver;

	file_addmetadata(kfp, MODINFOMD_EFI_MAP, efisz + sz, efihdr);

	return (0);
}

/*
 * Load the information expected by an arm64 kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The 'bootinfo' struct is constructed, and copied into the kernel space.
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp)
{
	struct preloaded_file *xp, *kfp;
	uint64_t kernend;
	vm_offset_t addr, size;

	/* find the last module in the chain */
	addr = 0;
	for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
	if (addr < (xp->f_addr + xp->f_size))
		addr = xp->f_addr + xp->f_size;
	}
	/* pad to a page boundary */
	addr = roundup(addr, PAGE_SIZE);

	kfp = file_findfile(NULL, "elf kernel");
	if (kfp == NULL)
		kfp = file_findfile(NULL, "elf64 kernel");
	if (kfp == NULL)
		panic("can't find kernel file");
	kernend = 0;	/* fill it in later */
	file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);

	bi_load_efi_data(kfp);

	/* Figure out the size and location of the metadata */
	*modulep = addr;
	size = bi_copymodules(0);
	kernend = roundup(addr + size, PAGE_SIZE);
	*kernendp = kernend;

	/* copy module list and metadata */
	(void)bi_copymodules(addr);

	return (0);
}

