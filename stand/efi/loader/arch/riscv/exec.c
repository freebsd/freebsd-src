/*-
 * Copyright (c) 2001 Benno Rice <benno@FreeBSD.org>
 * Copyright (c) 2007 Semihalf, Rafal Jaworowski <raj@semihalf.com>
 * All rights reserved.
 * Copyright (c) 2024 The FreeBSD Foundation
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

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/elf.h>

#include <stand.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "loader_efi.h"

static void
riscv_set_boot_hart(struct preloaded_file *fp)
{
	EFI_GUID riscvboot = RISCV_EFI_BOOT_PROTOCOL_GUID;
	RISCV_EFI_BOOT_PROTOCOL *proto;
	EFI_STATUS status = 0;
	uint64_t boot_hartid = ULONG_MAX;

	status = BS->LocateProtocol(&riscvboot, NULL, (void **)&proto);
	if (EFI_ERROR(status)) {
		return;
	}

	status = proto->GetBootHartId(proto, &boot_hartid);
	if (EFI_ERROR(status)) {
		return;
	}

	file_addmetadata(fp, MODINFOMD_BOOT_HARTID, sizeof(boot_hartid),
	    &boot_hartid);
}

static int
__elfN(exec)(struct preloaded_file *fp)
{
	struct file_metadata *fmp;
	vm_offset_t modulep, kernend;
	Elf_Ehdr *e;
	int error;
	void (*entry)(void *);

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return (EFTYPE);

	riscv_set_boot_hart(fp);

	e = (Elf_Ehdr *)&fmp->md_data;

	efi_time_fini();

	entry = efi_translate(e->e_entry);

	printf("Kernel entry at %p...\n", entry);
	printf("Kernel args: %s\n", fp->f_args);

	/*
	 * we have to cleanup here because net_cleanup() doesn't work after
	 * we call ExitBootServices
	 */
	dev_cleanup();

	if ((error = bi_load(fp->f_args, &modulep, &kernend, true)) != 0) {
		efi_time_init();
		return (error);
	}

	(*entry)((void *)modulep);
	panic("exec returned");
}

static struct file_format riscv_elf = {
	__elfN(loadfile),
	__elfN(exec)
};

struct file_format *file_formats[] = {
	&riscv_elf,
	NULL
};
