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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/linker.h>

#if 0
#include <machine/bootinfo.h>
#endif
#include <machine/elf.h>

#include <stand.h>

#include "bootstrap.h"
#include "libofw.h"
#include "openfirm.h"

extern char		end[];
extern vm_offset_t	reloc;	/* From <arch>/conf.c */

int
ofw_elf_loadfile(char *filename, vm_offset_t dest,
    struct preloaded_file **result)
{
	int	r;
	void	*addr;

	r = elf_loadfile(filename, dest, result);
	if (r != 0)
		return (r);

	addr = OF_claim((void *)(*result)->f_addr, (*result)->f_size, 0);

	if (addr == (void *)-1 || addr != (void *)(*result)->f_addr)
		return (ENOMEM);

	return (0);
}

int
ofw_elf_exec(struct preloaded_file *fp)
{
	struct file_metadata	*md;
	Elf_Ehdr		*ehdr;
	vm_offset_t		entry, bootinfop;
	int			boothowto, err, bootdev;
#if 0
	struct bootinfo		*bi;
#endif
	vm_offset_t		ssym, esym;

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL) {
		return(EFTYPE);			/* XXX actually EFUCKUP */
	}
	ehdr = (Elf_Ehdr *)&(md->md_data);

#if 0
	if ((err = bi_load(fp->f_args, &boothowto, &bootdev, &bootinfop)) != 0)
		return(err);
#endif
	entry = ehdr->e_entry & 0xffffff;

	ssym = esym = 0;
	if ((md = file_findmetadata(fp, MODINFOMD_SSYM)) != NULL)
		ssym = *((vm_offset_t *)&(md->md_data));
	if ((md = file_findmetadata(fp, MODINFOMD_ESYM)) != NULL)
		esym = *((vm_offset_t *)&(md->md_data));
	if (ssym == 0 || esym == 0)
		ssym = esym = 0;		/* sanity */
#if 0
	bi = (struct bootinfo *)PTOV(bootinfop);
	bi->bi_symtab = ssym;
	bi->bi_esymtab = esym;
#endif

/*
#ifdef DEBUG
*/
	printf("Start @ 0x%lx ...\n", entry);
/*
#endif
*/

	dev_cleanup();
	ofw_release_heap();
/*	OF_chain(0, 0, entry, bootinfop, sizeof(struct bootinfo));*/
	OF_chain((void *)reloc, end - (char *)reloc, (void *)entry, NULL, 0);

	panic("exec returned");
}

struct file_format	ofw_elf =
{
	ofw_elf_loadfile,
	ofw_elf_exec
};
