/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
#ifdef DEBUG
#include <machine/_inttypes.h>
#endif
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/elf.h>
#include <machine/segments.h>
#include <stand.h>

#include "bootstrap.h"
#include "libuserboot.h"

static int	elf64_exec(struct preloaded_file *amp);
static int	elf64_obj_exec(struct preloaded_file *amp);

struct file_format amd64_elf = { elf64_loadfile, elf64_exec };
struct file_format amd64_elf_obj = { elf64_obj_loadfile, elf64_obj_exec };

#define	GUEST_NULL_SEL		0
#define	GUEST_CODE_SEL		1
#define	GUEST_DATA_SEL		2
#define	GUEST_GDTR_LIMIT	(3 * 8 - 1)

static void
setup_freebsd_gdt(struct user_segment_descriptor *gdt)
{
	gdt[GUEST_NULL_SEL] = (struct user_segment_descriptor) { 0 };
	gdt[GUEST_CODE_SEL] = (struct user_segment_descriptor) {
	    .sd_p = 1, .sd_long = 1, .sd_type = SDT_MEME
	};
	gdt[GUEST_DATA_SEL] = (struct user_segment_descriptor) {
	    .sd_p = 1, .sd_type = SDT_MEMRO
	};
}

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
	vm_offset_t		modulep, kernend;
	int			err;
	int			i;
	uint32_t		stack[1024];
	pml4_entry_t		PT4[512];
	pdp_entry_t		PT3[512];
	pd_entry_t		PT2[512];
	struct user_segment_descriptor gdt[3];

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return(EFTYPE);
	ehdr = (Elf_Ehdr *)&(md->md_data);

	err = bi_load64(fp->f_args, &modulep, &kernend);
	if (err != 0)
		return(err);

	bzero(PT4, PAGE_SIZE);
	bzero(PT3, PAGE_SIZE);
	bzero(PT2, PAGE_SIZE);

	/*
	 * Build a scratch stack at physical 0x1000, page tables:
	 *	PT4 at 0x2000,
	 *	PT3 at 0x3000,
	 *	PT2 at 0x4000,
	 *      gdtr at 0x5000,
	 */

	/*
	 * This is kinda brutal, but every single 1GB VM memory segment
	 * points to the same first 1GB of physical memory.  But it is
	 * more than adequate.
	 */
	for (i = 0; i < 512; i++) {
		/* Each slot of the level 4 pages points to the same level 3 page */
		PT4[i] = (pml4_entry_t) 0x3000;
		PT4[i] |= PG_V | PG_RW;

		/* Each slot of the level 3 pages points to the same level 2 page */
		PT3[i] = (pdp_entry_t) 0x4000;
		PT3[i] |= PG_V | PG_RW;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		PT2[i] = i * (2 * 1024 * 1024);
		PT2[i] |= PG_V | PG_RW | PG_PS;
	}

#ifdef DEBUG
	printf("Start @ %#"PRIx64" ...\n", ehdr->e_entry);
#endif

	dev_cleanup();

	stack[0] = 0;		/* return address */
	stack[1] = modulep;
	stack[2] = kernend;
	CALLBACK(copyin, stack, 0x1000, sizeof(stack));
	CALLBACK(copyin, PT4, 0x2000, sizeof(PT4));
	CALLBACK(copyin, PT3, 0x3000, sizeof(PT3));
	CALLBACK(copyin, PT2, 0x4000, sizeof(PT2));
	CALLBACK(setreg, 4, 0x1000);

	CALLBACK(setmsr, MSR_EFER, EFER_LMA | EFER_LME);
	CALLBACK(setcr, 4, CR4_PAE | CR4_VMXE);
	CALLBACK(setcr, 3, 0x2000);
	CALLBACK(setcr, 0, CR0_PG | CR0_PE | CR0_NE);

	setup_freebsd_gdt(gdt);
	CALLBACK(copyin, gdt, 0x5000, sizeof(gdt));
	CALLBACK(setgdt, 0x5000, sizeof(gdt));

	CALLBACK(exec, ehdr->e_entry);

	panic("exec returned");
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	return (EFTYPE);
}
