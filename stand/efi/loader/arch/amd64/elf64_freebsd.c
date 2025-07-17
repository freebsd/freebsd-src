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
#include <sys/exec.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/elf.h>
#include <stand.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

#include "loader_efi.h"

static int	elf64_exec(struct preloaded_file *amp);
static int	elf64_obj_exec(struct preloaded_file *amp);

static struct file_format amd64_elf = {
	.l_load = elf64_loadfile,
	.l_exec = elf64_exec,
};
static struct file_format amd64_elf_obj = {
	.l_load = elf64_obj_loadfile,
	.l_exec = elf64_obj_exec,
};

extern struct file_format multiboot2;
extern struct file_format multiboot2_obj;

struct file_format *file_formats[] = {
	&multiboot2,
	&multiboot2_obj,
	&amd64_elf,
	&amd64_elf_obj,
	NULL
};

static pml4_entry_t *PT4;
static pdp_entry_t *PT3;
static pdp_entry_t *PT3_l, *PT3_u;
static pd_entry_t *PT2;
static pd_entry_t *PT2_l0, *PT2_l1, *PT2_l2, *PT2_l3, *PT2_u0, *PT2_u1;

static void (*trampoline)(uint64_t stack, void *copy_finish, uint64_t kernend,
    uint64_t modulep, pml4_entry_t *pagetable, uint64_t entry);

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
	Elf_Ehdr 		*ehdr;
	vm_offset_t		modulep, kernend, trampcode, trampstack;
	int			err, i;
	bool			copy_auto;

	copy_auto = copy_staging == COPY_STAGING_AUTO;
	if (copy_auto)
		copy_staging = fp->f_kernphys_relocatable ?
		    COPY_STAGING_DISABLE : COPY_STAGING_ENABLE;

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return (EFTYPE);
	ehdr = (Elf_Ehdr *)&(md->md_data);

	trampcode = copy_staging == COPY_STAGING_ENABLE ?
	    (vm_offset_t)G(1) : (vm_offset_t)G(4);
	err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 1,
	    (EFI_PHYSICAL_ADDRESS *)&trampcode);
	if (EFI_ERROR(err)) {
		printf("Unable to allocate trampoline\n");
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (ENOMEM);
	}
	bzero((void *)trampcode, EFI_PAGE_SIZE);
	trampstack = trampcode + EFI_PAGE_SIZE - 8;
	bcopy((void *)&amd64_tramp, (void *)trampcode, amd64_tramp_size);
	trampoline = (void *)trampcode;

	if (copy_staging == COPY_STAGING_ENABLE) {
		PT4 = (pml4_entry_t *)G(1);
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 3,
		    (EFI_PHYSICAL_ADDRESS *)&PT4);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 1);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}
		bzero(PT4, 3 * EFI_PAGE_SIZE);
		PT3 = &PT4[512];
		PT2 = &PT3[512];

		/*
		 * This is kinda brutal, but every single 1GB VM
		 * memory segment points to the same first 1GB of
		 * physical memory.  But it is more than adequate.
		 */
		for (i = 0; i < NPTEPG; i++) {
			/*
			 * Each slot of the L4 pages points to the
			 * same L3 page.
			 */
			PT4[i] = (pml4_entry_t)PT3;
			PT4[i] |= PG_V | PG_RW;

			/*
			 * Each slot of the L3 pages points to the
			 * same L2 page.
			 */
			PT3[i] = (pdp_entry_t)PT2;
			PT3[i] |= PG_V | PG_RW;

			/*
			 * The L2 page slots are mapped with 2MB pages for 1GB.
			 */
			PT2[i] = (pd_entry_t)i * M(2);
			PT2[i] |= PG_V | PG_RW | PG_PS;
		}
	} else {
		PT4 = (pml4_entry_t *)G(4);
		err = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, 9,
		    (EFI_PHYSICAL_ADDRESS *)&PT4);
		if (EFI_ERROR(err)) {
			printf("Unable to allocate trampoline page table\n");
			BS->FreePages(trampcode, 9);
			if (copy_auto)
				copy_staging = COPY_STAGING_AUTO;
			return (ENOMEM);
		}

		bzero(PT4, 9 * EFI_PAGE_SIZE);

		PT3_l = &PT4[NPML4EPG * 1];
		PT3_u = &PT4[NPML4EPG * 2];
		PT2_l0 = &PT4[NPML4EPG * 3];
		PT2_l1 = &PT4[NPML4EPG * 4];
		PT2_l2 = &PT4[NPML4EPG * 5];
		PT2_l3 = &PT4[NPML4EPG * 6];
		PT2_u0 = &PT4[NPML4EPG * 7];
		PT2_u1 = &PT4[NPML4EPG * 8];

		/* 1:1 mapping of lower 4G */
		PT4[0] = (pml4_entry_t)PT3_l | PG_V | PG_RW;
		PT3_l[0] = (pdp_entry_t)PT2_l0 | PG_V | PG_RW;
		PT3_l[1] = (pdp_entry_t)PT2_l1 | PG_V | PG_RW;
		PT3_l[2] = (pdp_entry_t)PT2_l2 | PG_V | PG_RW;
		PT3_l[3] = (pdp_entry_t)PT2_l3 | PG_V | PG_RW;
		for (i = 0; i < 4 * NPDEPG; i++) {
			PT2_l0[i] = ((pd_entry_t)i << PDRSHIFT) | PG_V |
			    PG_RW | PG_PS;
		}

		/* mapping of kernel 2G below top */
		PT4[NPML4EPG - 1] = (pml4_entry_t)PT3_u | PG_V | PG_RW;
		PT3_u[NPDPEPG - 2] = (pdp_entry_t)PT2_u0 | PG_V | PG_RW;
		PT3_u[NPDPEPG - 1] = (pdp_entry_t)PT2_u1 | PG_V | PG_RW;
		/* compat mapping of phys @0 */
		PT2_u0[0] = PG_PS | PG_V | PG_RW;
		/* this maps past staging area */
		for (i = 1; i < 2 * NPDEPG; i++) {
			PT2_u0[i] = ((pd_entry_t)staging +
			    ((pd_entry_t)i - 1) * NBPDR) |
			    PG_V | PG_RW | PG_PS;
		}
	}

	printf("staging %#lx (%scopying) tramp %p PT4 %p\n",
	    staging, copy_staging == COPY_STAGING_ENABLE ? "" : "not ",
	    trampoline, PT4);
	printf("Start @ 0x%lx ...\n", ehdr->e_entry);

	/*
	 * we have to cleanup here because net_cleanup() doesn't work after
	 * we call ExitBootServices
	 */
	dev_cleanup();

	efi_time_fini();
	err = bi_load(fp->f_args, &modulep, &kernend, true);
	if (err != 0) {
		efi_time_init();
		if (copy_auto)
			copy_staging = COPY_STAGING_AUTO;
		return (err);
	}

	trampoline(trampstack, copy_staging == COPY_STAGING_ENABLE ?
	    efi_copy_finish : efi_copy_finish_nop, kernend, modulep,
	    PT4, ehdr->e_entry);

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}
