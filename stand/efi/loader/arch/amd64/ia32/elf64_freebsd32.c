/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2014 The FreeBSD Foundation
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

#define __ELF_WORD_SIZE 64
#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

#include "loader_efi.h"

extern int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp,
    bool exit_bs);

static int	elf64_exec(struct preloaded_file *amp);
static int	elf64_obj_exec(struct preloaded_file *amp);

static struct file_format amd64_elf = {
	.l_load = elf64_loadfile,
	.l_exec = elf64_exec
};

static struct file_format amd64_elf_obj = {
	.l_load = elf64_obj_loadfile,
	.l_exec = elf64_obj_exec
};

struct file_format *file_formats[] = {
	&amd64_elf,
	&amd64_elf_obj,
	NULL
};

#define PG_V	0x001
#define PG_RW	0x002
#define PG_PS	0x080

#define GDT_P	0x00800000000000
#define GDT_E	0x00080000000000
#define GDT_S	0x00100000000000
#define GDT_RW	0x00020000000000
#define GDT_L	0x20000000000000

#define M(x)	((x) * 1024 * 1024)

typedef uint64_t p4_entry_t;
typedef uint64_t p3_entry_t;
typedef uint64_t p2_entry_t;
typedef uint64_t gdt_t;

static p4_entry_t *PT4;
static p3_entry_t *PT3;
static p3_entry_t *PT3_l, *PT3_u;
static p2_entry_t *PT2;
static p2_entry_t *PT2_l0, *PT2_l1, *PT2_l2, *PT2_l3, *PT2_u0, *PT2_u1;
static gdt_t *GDT;

extern EFI_PHYSICAL_ADDRESS staging;

static void (*trampoline)(uint32_t stack, void *copy_finish, uint32_t kernend,
    uint32_t modulep, uint64_t *pagetable, struct GDTR *gdtr, uint64_t entry);

extern uintptr_t amd64_tramp;
extern uint32_t amd64_tramp_size;

/*
 * There is an ELF kernel and one or more ELF modules loaded.
 * We wish to start executing the kernel image, so make such
 * preparations as are required, and do so.
 */
static int
elf64_exec(struct preloaded_file *fp)
{
	struct file_metadata	*md;
	struct GDTR	*gdtr;
	Elf_Ehdr 		*ehdr;
	vm_offset_t		modulep, kernend, trampcode, trampstack;
	int			err, i;
	bool			copy_auto;

	/*
	 * EFI_PHYSICAL_ADDRESS is 8 bytes wide on 32-bit systems too,
	 * so we cant just use a pointer.
	 */
	EFI_PHYSICAL_ADDRESS ptr64;

	copy_auto = copy_staging == COPY_STAGING_AUTO;
	if (copy_auto)
		copy_staging = fp->f_kernphys_relocatable ?
		    COPY_STAGING_DISABLE : COPY_STAGING_ENABLE;

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return (EFTYPE);
	ehdr = (Elf_Ehdr *)&(md->md_data);

	ptr64 = copy_staging == COPY_STAGING_ENABLE ?
	    0x0000000040000000 /* 1G */ :
	    0x0000000100000000; /* 4G */;
	err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 1,
	    &ptr64);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate trampoline\n");
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (ENOMEM);
	}
	trampcode = ptr64;
	bzero((void *)(uintptr_t)trampcode, EFI_PAGE_SIZE);
	trampstack = trampcode + EFI_PAGE_SIZE - 8;
	bcopy((void *)&amd64_tramp, (void *)(uintptr_t)trampcode,
		amd64_tramp_size);
	trampoline = (void *)(uintptr_t)trampcode;

	ptr64 = copy_staging == COPY_STAGING_ENABLE ?
	    0x0000000040000000 /* 1G */ :
	    0x0000000100000000; /* 4G */;
	err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 1,
	    &ptr64);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate GDT\n");
		BS->FreePages(trampcode, 1);
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (ENOMEM);
	}
	GDT = (gdt_t *)(uintptr_t)ptr64;
	bzero(GDT, EFI_PAGE_SIZE);
	GDT[1] = GDT_P | GDT_E | GDT_S | GDT_RW | GDT_L; /* CS */
	gdtr = (struct GDTR *)&GDT[2];
	gdtr->ptr = (uintptr_t)GDT;
	gdtr->size = 2 * sizeof(uint64_t) - 1;

	if (copy_staging == COPY_STAGING_ENABLE) {
		ptr64 = 0x0000000040000000; /* 1G */
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 3,
		&ptr64);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 1);
			BS->FreePages((uintptr_t)GDT, 1);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}
		PT4 = (p4_entry_t *)(uintptr_t)ptr64;

		bzero(PT4, 3 * EFI_PAGE_SIZE);
		PT3 = &PT4[512];
		PT2 = &PT3[512];

		/*
		 * This is kinda brutal, but every single 1GB VM
		 * memory segment points to the same first 1GB of
		 * physical memory.  But it is more than adequate.
		 */
		for (i = 0; i < 512; i++) {
			/*
			 * Each slot of the L4 pages points to the
			 * same L3 page.
			 */
			PT4[i] = (uintptr_t)PT3;
			PT4[i] |= PG_V | PG_RW;

			/*
			 * Each slot of the L3 pages points to the
			 * same L2 page.
			 */
			PT3[i] = (uintptr_t)PT2;
			PT3[i] |= PG_V | PG_RW;

			/*
			 * The L2 page slots are mapped with 2MB pages for 1GB.
			 */
			PT2[i] = i * M(2);
			PT2[i] |= PG_V | PG_RW | PG_PS;
		}
	} else {
		ptr64 = 0x0000000100000000; /* 4G */
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 9,
		&ptr64);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 1);
			BS->FreePages((uintptr_t)GDT, 1);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}
		PT4 = (p4_entry_t *)(uintptr_t)ptr64;

		bzero(PT4, 9 * EFI_PAGE_SIZE);
		PT3_l = &PT4[512];
		PT3_u = &PT3_l[512];
		PT2_l0 = &PT3_u[512];
		PT2_l1 = &PT2_l0[512];
		PT2_l2 = &PT2_l1[512];
		PT2_l3 = &PT2_l2[512];
		PT2_u0 = &PT2_l3[512];
		PT2_u1 = &PT2_u0[512];

		/* 1:1 mapping of lower 4G */
		PT4[0] = (uintptr_t)PT3_l | PG_V | PG_RW;
		PT3_l[0] = (uintptr_t)PT2_l0 | PG_V | PG_RW;
		PT3_l[1] = (uintptr_t)PT2_l1 | PG_V | PG_RW;
		PT3_l[2] = (uintptr_t)PT2_l2 | PG_V | PG_RW;
		PT3_l[3] = (uintptr_t)PT2_l3 | PG_V | PG_RW;
		for (i = 0; i < 2048; i++) {
			PT2_l0[i] = (i * M(2)) | PG_V | PG_RW | PG_PS;
		}

		/* mapping of kernel 2G below top */
		PT4[511] = (uintptr_t)PT3_u | PG_V | PG_RW;
		PT3_u[511] = (uintptr_t)PT2_u1 | PG_V | PG_RW;
		PT3_u[510] = (uintptr_t)PT2_u0 | PG_V | PG_RW;
		/* compat mapping of phys @0 */
		PT2_u0[0] = PG_PS | PG_V | PG_RW;
		/* this maps past staging area */
		for (i = 1; i < 1024; i++) {
			PT2_u0[i] = (staging + (i - 1) * M(2))
			| PG_V | PG_RW | PG_PS;
		}
	}

	printf("staging %#llx (%scopying) tramp %p PT4 %p GDT %p\n",
	    staging, copy_staging == COPY_STAGING_ENABLE ? "" : "not ",
	    trampoline, PT4, GDT);
	printf("Start @ %#llx ...\n", ehdr->e_entry);

	efi_time_fini();
	err = bi_load(fp->f_args, &modulep, &kernend, true);
	if (err != 0) {
		efi_time_init();
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (err);
	}

	dev_cleanup();

	trampoline(trampstack, copy_staging == COPY_STAGING_ENABLE ?
	    efi_copy_finish : efi_copy_finish_nop, kernend, modulep,
	    PT4, gdtr, ehdr->e_entry);

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}
