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

static int	aout_loadimage(int fd, vm_offset_t loadaddr, struct exec *ehdr);

char	*aout_kerneltype = "a.out kernel";
char	*aout_moduletype = "a.out module";

/*
 * Attempt to load the file (file) as an a.out module.  It will be stored at
 * (dest), and a pointer to a module structure describing the loaded object
 * will be saved in (result).
 */
int
aout_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result)
{
    struct loaded_module	*mp;
    struct exec			ehdr;
    int				fd;
    vm_offset_t			addr;
    int				err, kernel;

    /*
     * Open the image, read and validate the a.out header 
     */
    if (filename == NULL)	/* can't handle nameless */
	return(EFTYPE);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(errno);
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
	return(EFTYPE);		/* could be EIO, but may be small file */
    if (N_BADMAG(ehdr))
	return(EFTYPE);

    /*
     * Check to see what sort of module we are.
     *
     * XXX should check N_GETMID()
     */
    mp = mod_findmodule(NULL, NULL);
    if (N_GETFLAG(ehdr) == (EX_DYNAMIC | EX_PIC)) {
	/* Looks like a kld module */
	if (mp == NULL) {
	    printf("aout_loadmodule: can't load module before kernel\n");
	    return(EPERM);
	}
	if (strcmp(aout_kerneltype, mp->m_type)) {
	    printf("out_loadmodule: can't load module with kernel type '%s'\n", mp->m_type);
	    return(EPERM);
	}
	/* Looks OK, got ahead */
	kernel = 0;

    } else if (N_GETFLAG(ehdr) == 0) {
	/* Looks like a kernel */
	if (mp != NULL) {
	    printf("aout_loadmodule: kernel already loaded\n");
	    return(EPERM);
	}
	/* 
	 * Calculate destination address based on kernel entrypoint 	
	 * XXX this is i386-freebsd-aout specific
	 */
	dest = ehdr.a_entry & 0x100000;
	if (dest == 0) {
	    printf("aout_loadmodule: not a kernel (maybe static binary?)\n");
	    return(EPERM);
	}
	kernel = 1;
    } else {
	return(EFTYPE);
    }

    /* 
     * Ok, we think we should handle this.
     */
    mp = malloc(sizeof(struct loaded_module));
    mp->m_name = strdup(filename);		/* XXX should we prune the name? */
    mp->m_type = strdup(kernel ? aout_kerneltype : aout_moduletype);
    mp->m_args = NULL;				/* XXX should we put the bootstrap args here and parse later? */
    mp->m_metadata = NULL;
    mp->m_addr = addr = dest;
    printf("%s at 0x%x\n", filename, addr);

    mp->m_size = aout_loadimage(fd, addr, &ehdr);
    if (mp->m_size == 0)
	goto ioerr;

    /* save exec header as metadata */
    mod_addmetadata(mp, MODINFOMD_AOUTEXEC, sizeof(struct exec), &ehdr);

    /* Load OK, return module pointer */
    *result = (struct loaded_module *)mp;
    return(0);

 ioerr:
    err = EIO;
    close(fd);
    free(mp);
    return(err);
}

/*
 * With the file (fd) open on the image, and (ehdr) containing
 * the exec header, load the image at (addr)
 *
 * Fixup the a_bss field in (ehdr) to reflect the padding added to
 * align the symbol table.
 */
static int
aout_loadimage(int fd, vm_offset_t loadaddr, struct exec *ehdr)
{
    u_int		pad;
    vm_offset_t		addr;
    int			ss;

    addr = loadaddr;
    lseek(fd, N_TXTOFF(*ehdr), SEEK_SET);
    
    /* text segment */
    printf("text=0x%lx ", ehdr->a_text);
    if (archsw.arch_readin(fd, addr, ehdr->a_text) != ehdr->a_text)
	return(0);
    addr += ehdr->a_text;

    /* data segment */
    printf("data=0x%lx ", ehdr->a_data);
    if (archsw.arch_readin(fd, addr, ehdr->a_data) != ehdr->a_data)
	return(0);
    addr += ehdr->a_data;

    /* skip the BSS */
    printf("bss=0x%lx ", ehdr->a_bss);
    addr += ehdr->a_bss;
    
    /* pad to a page boundary */
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
	ehdr->a_bss += pad;
    }

    /* symbol table size */
    archsw.arch_copyin(&ehdr->a_syms, addr, sizeof(ehdr->a_syms));
    addr += sizeof(ehdr->a_syms);

    /* symbol table */
    printf("symbols=[0x%x+0x%x+0x%lx", pad, sizeof(ehdr->a_syms), ehdr->a_syms);
    if (archsw.arch_readin(fd, addr, ehdr->a_syms) != ehdr->a_syms)
	return(0);
    addr += ehdr->a_syms;

    /* string table */
    read(fd, &ss, sizeof(ss));
    archsw.arch_copyin(&ss, addr, sizeof(ss));
    addr += sizeof(ss);
    ss -= sizeof(ss);
    printf("+0x%x+0x%x]", sizeof(ss), ss);
    if (archsw.arch_readin(fd, addr, ss) != ss)
	return(0);
    printf(" \n");
    addr += ss;

    return(addr - loadaddr);
}

