/* $NetBSD: loadfile.c,v 1.10 1998/06/25 06:45:46 ross Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>
#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/bootinfo.h>

#include "bootstrap.h"

#define _KERNEL

static int	elf64_exec(struct preloaded_file *afp);
int		bi_load(struct bootinfo_v1 *, vm_offset_t *,
			struct preloaded_file *);

struct file_format alpha_elf = { elf64_loadfile, elf64_exec };

vm_offset_t ffp_save, ptbr_save;

static int
elf64_exec(struct preloaded_file *fp)
{
    static struct bootinfo_v1	bootinfo_v1;
    struct file_metadata	*md;
    Elf_Ehdr			*hdr;
    int				err;
    int				flen;

    if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
	return(EFTYPE);			/* XXX actually EFUCKUP */
    hdr = (Elf_Ehdr *)&(md->md_data);

    /* XXX ffp_save does not appear to be used in the kernel.. */
    bzero(&bootinfo_v1, sizeof(bootinfo_v1));
    err = bi_load(&bootinfo_v1, &ffp_save, fp);
    if (err)
	return(err);

    /*
     * Fill in rest of bootinfo for the kernel.
     */
    flen = prom_getenv(PROM_E_BOOTED_OSFLAGS, bootinfo_v1.boot_flags,
	sizeof(bootinfo_v1.boot_flags));
    bootinfo_v1.hwrpb = (void *)HWRPB_ADDR;
    bootinfo_v1.hwrpbsize = ((struct rpb *)HWRPB_ADDR)->rpb_size;
    bootinfo_v1.cngetc = NULL;
    bootinfo_v1.cnputc = NULL;
    bootinfo_v1.cnpollc = NULL;

    /*
     * Append the boot command flags.
     */
    if (fp->f_args != NULL && *fp->f_args != '\0') {
	const char *p = fp->f_args;

	do {
	    if (*p == '-') {
		while (*++p != ' ' && *p != '\0')
		    if (flen < sizeof(bootinfo_v1.boot_flags) - 1)
			bootinfo_v1.boot_flags[flen++] = *p;
	    } else
		while (*p != ' ' && *p != '\0')
		    p++;
	    while (*p == ' ')
		p++;
	} while (*p != '\0');
	bootinfo_v1.boot_flags[flen] = '\0';
    }

    printf("Entering %s at 0x%lx...\n", fp->f_name, hdr->e_entry);
    closeall();
    dev_cleanup();
    alpha_pal_imb();
    (*(void (*)())hdr->e_entry)(ffp_save, ptbr_save,
			       BOOTINFO_MAGIC, &bootinfo_v1, 1, 0);
}



