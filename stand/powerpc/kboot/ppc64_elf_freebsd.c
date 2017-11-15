/*-
 * Copyright (c) 2001 Benno Rice <benno@FreeBSD.org>
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

#define __ELF_WORD_SIZE 64

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/metadata.h>
#include <machine/elf.h>

#include <stand.h>

#include "bootstrap.h"
#include "host_syscall.h"

extern char		end[];
extern void		*kerneltramp;
extern size_t		szkerneltramp;
extern int		nkexec_segments;
extern void *		loaded_segments;

int
ppc64_elf_loadfile(char *filename, u_int64_t dest,
    struct preloaded_file **result)
{
	int	r;

	r = __elfN(loadfile)(filename, dest, result);
	if (r != 0)
		return (r);

	return (0);
}

int
ppc64_elf_exec(struct preloaded_file *fp)
{
	struct file_metadata	*fmp;
	vm_offset_t		mdp, dtb;
	Elf_Ehdr		*e;
	int			error;
	uint32_t		*trampoline;
	uint64_t		entry;
	vm_offset_t		trampolinebase;

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL) {
		return(EFTYPE);
	}
	e = (Elf_Ehdr *)&fmp->md_data;

	/* Figure out where to put it */
	trampolinebase = archsw.arch_loadaddr(LOAD_RAW, NULL, 0);
	
	/* Set up loader trampoline */
	trampoline = malloc(szkerneltramp);
	memcpy(trampoline, &kerneltramp, szkerneltramp);
	/* Parse function descriptor for ELFv1 kernels */
	if ((e->e_flags & 3) == 2)
		entry = e->e_entry;
	else
		archsw.arch_copyout(e->e_entry + elf64_relocation_offset,
		    &entry, 8);
	trampoline[2] = entry + elf64_relocation_offset;
	trampoline[4] = 0; /* Phys. mem offset */
	trampoline[5] = 0; /* OF entry point */

	if ((error = md_load64(fp->f_args, &mdp, &dtb)) != 0)
		return (error);

	trampoline[3] = dtb;
	trampoline[6] = mdp;
	trampoline[7] = sizeof(mdp);
	printf("Kernel entry at %#jx (%#x) ...\n", e->e_entry, trampoline[2]);
	printf("DTB at %#x, mdp at %#x\n", dtb, mdp);

	dev_cleanup();

	archsw.arch_copyin(trampoline, trampolinebase, szkerneltramp);
	free(trampoline);

	error = kexec_load(trampolinebase, nkexec_segments, &loaded_segments);
	if (error != 0)
		panic("kexec_load returned error: %d", error);
	error = host_reboot(0xfee1dead, 672274793,
	    0x45584543 /* LINUX_REBOOT_CMD_KEXEC */, NULL);
	if (error != 0)
		panic("reboot returned error: %d", error);
	while (1) {}

	panic("exec returned");
}

struct file_format	ppc_elf64 =
{
	ppc64_elf_loadfile,
	ppc64_elf_exec
};
