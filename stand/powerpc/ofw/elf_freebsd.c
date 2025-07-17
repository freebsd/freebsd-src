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

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/metadata.h>
#include <machine/elf.h>
#if defined(__powerpc__)
#include <machine/md_var.h>
#endif

#include <stand.h>

#include "bootstrap.h"
#include "libofw.h"
#include "openfirm.h"
#include "modinfo.h"

extern char		end[];
extern vm_offset_t	reloc;	/* From <arch>/conf.c */

int
__elfN(ofw_loadfile)(char *filename, uint64_t dest,
    struct preloaded_file **result)
{
	int	r;

	r = __elfN(loadfile)(filename, dest, result);
	if (r != 0)
		return (r);

#if defined(__powerpc__)
	/*
	 * No need to sync the icache for modules: this will
	 * be done by the kernel after relocation.
	 */
	if (!strcmp((*result)->f_type, md_kerntype))
		__syncicache((void *) (*result)->f_addr, (*result)->f_size);
#endif
	return (0);
}

int
__elfN(ofw_exec)(struct preloaded_file *fp)
{
	struct file_metadata	*fmp;
	vm_offset_t		mdp, dtbp;
	Elf_Ehdr		*e;
	int			error;
	intptr_t		entry;

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL) {
		return(EFTYPE);
	}
	e = (Elf_Ehdr *)&fmp->md_data;
	entry = e->e_entry;

	if ((error = md_load(fp->f_args, &mdp, &dtbp)) != 0)
		return (error);

	printf("Kernel entry at 0x%x ...\n", entry);

	dev_cleanup();
	if (dtbp != 0) {
		OF_quiesce();
		((int (*)(u_long, u_long, u_long, void *, u_long))entry)(dtbp,
		    0, 0, (void *)mdp, 0xfb5d104d);
	} else {
		OF_chain((void *)reloc, end - (char *)reloc, (void *)entry,
		    (void *)mdp, 0xfb5d104d);
	}

	panic("exec returned");
}

struct file_format	ofw_elf =
{
	__elfN(ofw_loadfile),
	__elfN(ofw_exec)
};
