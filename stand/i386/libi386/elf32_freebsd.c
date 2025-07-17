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

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <stand.h>

#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

static int	elf32_exec(struct preloaded_file *amp);
static int	elf32_obj_exec(struct preloaded_file *amp);

struct file_format i386_elf = { elf32_loadfile, elf32_exec };
struct file_format i386_elf_obj = { elf32_obj_loadfile, elf32_obj_exec };

/*
 * There is an ELF kernel and one or more ELF modules loaded.  
 * We wish to start executing the kernel image, so make such 
 * preparations as are required, and do so.
 */
static int
elf32_exec(struct preloaded_file *fp)
{
    struct file_metadata	*md;
    Elf_Ehdr 			*ehdr;
    vm_offset_t			entry, bootinfop, modulep, kernend;
    int				boothowto, err, bootdev;

    if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
	return(EFTYPE);
    ehdr = (Elf_Ehdr *)&(md->md_data);

    err = bi_load32(fp->f_args, &boothowto, &bootdev, &bootinfop, &modulep, &kernend);
    if (err != 0)
	return(err);
    entry = ehdr->e_entry & 0xffffff;

#ifdef DEBUG
    printf("Start @ 0x%lx ...\n", entry);
#endif

    dev_cleanup();
    __exec((void *)entry, boothowto, bootdev, 0, 0, 0, bootinfop, modulep, kernend);

    panic("exec returned");
}

static int
elf32_obj_exec(struct preloaded_file *fp)
{
	return (EFTYPE);
}
