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
 *
 * $FreeBSD: src/sys/boot/i386/libi386/aout_freebsd.c,v 1.12.2.1 2000/07/06 00:34:02 ps Exp $
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact_aout.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <stand.h>

#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

static int	aout_exec(struct loaded_module *amp);

struct module_format i386_aout = { aout_loadmodule, aout_exec };

/*
 * There is an a.out kernel and one or more a.out modules loaded.  
 * We wish to start executing the kernel image, so make such 
 * preparations as are required, and do so.
 */
static int
aout_exec(struct loaded_module *mp)
{
    struct module_metadata	*md;
    struct exec			*ehdr;
    vm_offset_t			entry, bootinfop;
    int				boothowto, err, bootdev;
    struct bootinfo		*bi;
    vm_offset_t			ssym, esym;

    if ((md = mod_findmetadata(mp, MODINFOMD_AOUTEXEC)) == NULL)
	return(EFTYPE);			/* XXX actually EFUCKUP */
    ehdr = (struct exec *)&(md->md_data);

    if ((err = bi_load(mp->m_args, &boothowto, &bootdev, &bootinfop)) != 0)
	return(err);
    entry = ehdr->a_entry & 0xffffff;

    ssym = esym = 0;
    if ((md = mod_findmetadata(mp, MODINFOMD_SSYM)) != NULL)
	ssym = *((vm_offset_t *)&(md->md_data));
    if ((md = mod_findmetadata(mp, MODINFOMD_ESYM)) != NULL)
	esym = *((vm_offset_t *)&(md->md_data));
    if (ssym == 0 || esym == 0) 
	ssym = esym = 0;	/* sanity */
    bi = (struct bootinfo *)PTOV(bootinfop);
    bi->bi_symtab = ssym;	/* XXX this is only the primary kernel symtab */
    bi->bi_esymtab = esym;

    dev_cleanup();
    __exec((void *)entry, boothowto, bootdev, 0, 0, 0, bootinfop);

    panic("exec returned");
}
