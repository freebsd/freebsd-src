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
 *	$Id: aout_freebsd.c,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact_aout.h>
#include <sys/reboot.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <stand.h>

#include "bootstrap.h"
#include "libi386.h"

static int	aout_exec(struct loaded_module *amp);

struct module_format i386_aout = { aout_loadmodule, aout_exec };

static struct bootinfo	bi;

/*
 * There is an a.out kernel and one or more a.out modules loaded.  
 * We wish to start executing the kernel image, so make such 
 * preparations as are required, and do so.
 */
static int
aout_exec(struct loaded_module *mp)
{
    struct loaded_module	*xp;
    struct i386_devdesc		*currdev;
    struct module_metadata	*md;
    struct exec			*ehdr;
    u_int32_t			argv[6];	/* kernel arguments */
    int				major, bootdevnr;
    vm_offset_t			addr, entry;
    u_int			pad;

    if ((md = mod_findmetadata(mp, MODINFOMD_AOUTEXEC)) == NULL)
	return(EFTYPE);			/* XXX actually EFUCKUP */
    ehdr = (struct exec *)&(md->md_data);

    /* Boot from whatever the current device is */
    i386_getdev((void **)(&currdev), NULL, NULL);
    switch(currdev->d_type) {
    case DEVT_DISK:	    
	major = 0;			/* XXX work out the best possible major here */
	bootdevnr = MAKEBOOTDEV(major, 
				currdev->d_kind.biosdisk.slice >> 4, 
				currdev->d_kind.biosdisk.slice & 0xf, 
				currdev->d_kind.biosdisk.unit,
				currdev->d_kind.biosdisk.partition);
	break;
    default:
	printf("aout_exec: WARNING - don't know how to boot from device type %d\n", currdev->d_type);
    }
    free(currdev);

    /* Device data is kept in the kernel argv array */
    argv[0] = bi_getboothowto(mp->m_args);
    argv[1] = bootdevnr;
    argv[2] = 0;
    argv[3] = 0;
    argv[4] = 0;
    argv[5] = (u_int32_t)vtophys(&bi);

    /* legacy bootinfo structure */
    bi.bi_version = BOOTINFO_VERSION;
    bi.bi_kernelname = 0;		/* XXX char * -> kernel name */
    bi.bi_nfs_diskless = 0;		/* struct nfs_diskless * */
    bi.bi_n_bios_used = 0;		/* XXX would have to hook biosdisk driver for these */
    /* bi.bi_bios_geom[] */
    bi.bi_size = sizeof(bi);
    bi.bi_memsizes_valid = 1;
    bi.bi_vesa = 0;			/* XXX correct value? */
    bi.bi_basemem = getbasemem();
    bi.bi_extmem = getextmem();
    bi.bi_symtab = mp->m_addr + ehdr->a_text + ehdr->a_data + ehdr->a_bss;
    bi.bi_esymtab = bi.bi_symtab + sizeof(ehdr->a_syms) + ehdr->a_syms;

    /* find the last module in the chain */
    for (xp = mp; xp->m_next != NULL; xp = xp->m_next)
	;
    addr = xp->m_addr + xp->m_size;
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy our environment  XXX save addr here as env pointer, store in bootinfo? */
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    /* copy module list and metadata  XXX save addr here as env pointer, store in bootinfo? */
    bi_copymodules(addr);
    
    entry = ehdr->a_entry & 0xffffff;

#ifdef DEBUG
    {
	int i;
	for (i = 0; i < 6; i++)
	    printf("argv[%d]=%lx\n", i, argv[i]);
    }

    printf("Start @ 0x%lx ...\n", entry);
#endif

    startprog(entry, 6, argv, (vm_offset_t)0x90000);
    panic("exec returned");
}
