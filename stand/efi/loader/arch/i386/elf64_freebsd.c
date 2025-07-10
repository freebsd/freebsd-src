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

#define __ELF_WORD_SIZE 64
#include <sys/param.h>
#include <sys/linker.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/elf.h>
#include <machine/pmap_pae.h>
#include <machine/segments.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

#include "loader_efi.h"

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

/*
 * i386's pmap_pae.h doesn't provide this, so
 * just typedef our own.
 */
typedef pdpt_entry_t pml4_entry_t;

static void (*trampoline)(uint32_t stack, void *copy_finish, uint32_t kernend,
    uint32_t modulep, uint64_t *pagetable, void *gdtr, uint64_t entry);

extern void *amd64_tramp;
extern uint32_t amd64_tramp_size;

/*
 * There is an ELF kernel and one or more ELF modules loaded.
 * We wish to start executing the kernel image, so make such
 * preparations as are required, and do so.
 */
static int
elf64_exec(struct preloaded_file *fp)
{
	/*
	 * segments.h gives us a 32-bit gdtr, but
	 * we want a 64-bit one, so define our own.
	 */
	struct {
		uint16_t rd_limit;
		uint64_t rd_base;
	} __packed *gdtr;
	EFI_PHYSICAL_ADDRESS	ptr;
	EFI_ALLOCATE_TYPE	type;
	EFI_STATUS		err;
	struct file_metadata	*md;
	Elf_Ehdr 		*ehdr;
	pml4_entry_t		*PT4;
	pdpt_entry_t		*PT3;
	pd_entry_t		*PT2;
	struct user_segment_descriptor *gdt;
	vm_offset_t		modulep, kernend, trampstack;
	int i;

	switch (copy_staging) {
	case COPY_STAGING_ENABLE:
		type = AllocateMaxAddress;
		break;
	case COPY_STAGING_DISABLE:
		type = AllocateAnyPages;
		break;
	case COPY_STAGING_AUTO:
		type = fp->f_kernphys_relocatable ?
		    AllocateAnyPages : AllocateMaxAddress;
		break;
	}

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return (EFTYPE);
	ehdr = (Elf_Ehdr *)&(md->md_data);

	ptr = G(1);
	err = BS->AllocatePages(type, EfiLoaderCode,
	    EFI_SIZE_TO_PAGES(amd64_tramp_size), &ptr);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate trampoline\n");
		return (ENOMEM);
	}

	trampoline = (void *)(uintptr_t)ptr;
	bcopy(&amd64_tramp, trampoline, amd64_tramp_size);

	/*
	 * Allocate enough space for the GDTR + two GDT segments +
	 * our temporary stack (28 bytes).
	 */
#define DATASZ (sizeof(*gdtr) + \
	    sizeof(struct user_segment_descriptor) * 2 + 28)

	ptr = G(1);
	err = BS->AllocatePages(type, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(DATASZ), &ptr);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate GDT and stack\n");
		BS->FreePages((uintptr_t)trampoline, 1);
		return (ENOMEM);
	}

	trampstack = ptr + DATASZ;

#undef DATASZ

	gdt = (void *)(uintptr_t)ptr;
	gdt[0] = (struct user_segment_descriptor) { 0 };
	gdt[1] = (struct user_segment_descriptor) {
	    .sd_p = 1, .sd_long = 1, .sd_type = SDT_MEMERC
	};

	gdtr = (void *)(uintptr_t)(ptr +
	    sizeof(struct user_segment_descriptor) * 2);
	gdtr->rd_limit = sizeof(struct user_segment_descriptor) * 2 - 1;
	gdtr->rd_base = (uintptr_t)gdt;

	if (type == AllocateMaxAddress) {
		/* Copy staging enabled */

		ptr = G(1);
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData,
		    EFI_SIZE_TO_PAGES(512 * 3 * sizeof(uint64_t)), &ptr);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages((uintptr_t)trampoline, 1);
			BS->FreePages((uintptr_t)gdt, 1);
			return (ENOMEM);
		}
		PT4 = (pml4_entry_t *)(uintptr_t)ptr;

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
			PT4[i] = (uintptr_t)PT3 | PG_V | PG_RW;

			/*
			 * Each slot of the L3 pages points to the
			 * same L2 page.
			 */
			PT3[i] = (uintptr_t)PT2 | PG_V | PG_RW;

			/*
			 * The L2 page slots are mapped with 2MB pages for 1GB.
			 */
			PT2[i] = (i * M(2)) | PG_V | PG_RW | PG_PS;
		}
	} else {
		pdpt_entry_t	*PT3_l, *PT3_u;
		pd_entry_t	*PT2_l0, *PT2_l1, *PT2_l2, *PT2_l3, *PT2_u0, *PT2_u1;

		err = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
		    EFI_SIZE_TO_PAGES(512 * 9 * sizeof(uint64_t)), &ptr);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages((uintptr_t)trampoline, 1);
			BS->FreePages((uintptr_t)gdt, 1);
			return (ENOMEM);
		}
		PT4 = (pml4_entry_t *)(uintptr_t)ptr;

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
			PT2_l0[i] = ((pd_entry_t)i * M(2)) | PG_V | PG_RW | PG_PS;
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

	printf(
	    "staging %#llx (%scopying) tramp %p PT4 %p GDT %p\n"
	    "Start @ %#llx ...\n", staging,
	    type == AllocateMaxAddress ? "" : "not ", trampoline, PT4, gdt,
	    ehdr->e_entry
	);


	/*
	 * we have to cleanup here because net_cleanup() doesn't work after
	 * we call ExitBootServices
	 */
	dev_cleanup();

	efi_time_fini();
	err = bi_load(fp->f_args, &modulep, &kernend, true);
	if (err != 0) {
		efi_time_init();
		return (err);
	}

	trampoline(trampstack, type == AllocateMaxAddress ? efi_copy_finish :
	    efi_copy_finish_nop, kernend, modulep, PT4, gdtr, ehdr->e_entry);

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{
	return (EFTYPE);
}
