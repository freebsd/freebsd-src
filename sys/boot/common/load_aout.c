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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact_aout.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <stand.h>
#include <a.out.h>
#define FREEBSD_AOUT
#include <link.h>

#include "bootstrap.h"

static int		aout_loadimage(struct loaded_module *mp, int fd, vm_offset_t loadaddr, struct exec *ehdr, int kernel);

#if 0
static vm_offset_t	aout_findkldident(struct loaded_module *mp, struct exec *ehdr);
static int		aout_fixupkldmod(struct loaded_module *mp, struct exec *ehdr);
#endif

const char	*aout_kerneltype = "a.out kernel";
const char	*aout_moduletype = "a.out module";

/*
 * Attempt to load the file (file) as an a.out module.  It will be stored at
 * (dest), and a pointer to a module structure describing the loaded object
 * will be saved in (result).
 */
int
aout_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result)
{
    struct loaded_module	*mp, *kmp;
    struct exec			ehdr;
    int				fd;
    vm_offset_t			addr;
    int				err, kernel;
    u_int			pad;
    char			*s;

    mp = NULL;
    
    /*
     * Open the image, read and validate the a.out header 
     */
    if (filename == NULL)	/* can't handle nameless */
	return(EFTYPE);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(errno);
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
	err = EFTYPE;		/* could be EIO, but may be small file */
	goto oerr;
    }
    if (N_BADMAG(ehdr)) {
	err = EFTYPE;
	goto oerr;
    }

    /*
     * Check to see what sort of module we are.
     *
     * XXX should check N_GETMID()
     */
    kmp = mod_findmodule(NULL, NULL);
    if ((N_GETFLAG(ehdr)) & EX_DYNAMIC) {
	/* Looks like a kld module */
	if (kmp == NULL) {
	    printf("aout_loadmodule: can't load module before kernel\n");
	    err = EPERM;
	    goto oerr;
	}
	if (strcmp(aout_kerneltype, kmp->m_type)) {
	    printf("aout_loadmodule: can't load module with kernel type '%s'\n", kmp->m_type);
	    err = EPERM;
	    goto oerr;
	}
	/* Looks OK, got ahead */
	kernel = 0;

    } else if (N_GETFLAG(ehdr) == 0) {
	/* Looks like a kernel */
	if (kmp != NULL) {
	    printf("aout_loadmodule: kernel already loaded\n");
	    err = EPERM;
	    goto oerr;
	}
	/* 
	 * Calculate destination address based on kernel entrypoint 	
	 * XXX this is i386-freebsd-aout specific
	 */
	dest = ehdr.a_entry & 0x100000;
	if (dest == 0) {
	    printf("aout_loadmodule: not a kernel (maybe static binary?)\n");
	    err = EPERM;
	    goto oerr;
	}
	kernel = 1;
    } else {
	err = EFTYPE;
	goto oerr;
    }

    /* 
     * Ok, we think we should handle this.
     */
    mp = mod_allocmodule();
    if (kernel)
	setenv("kernelname", filename, 1);
    s = strrchr(filename, '/');
    if (s)
	mp->m_name = strdup(s + 1); 
    else
	mp->m_name = strdup(filename);
    mp->m_type = strdup(kernel ? aout_kerneltype : aout_moduletype);

    /* Page-align the load address */
    addr = dest;
    pad = (u_int)addr & PAGE_MASK;
    if (pad != 0) {
	pad = PAGE_SIZE - pad;
	addr += pad;
    }
    mp->m_addr = addr;					/* save the aligned load address */
    if (kernel)
	printf("%s at %p\n", filename, (void *) addr);

    mp->m_size = aout_loadimage(mp, fd, addr, &ehdr, kernel);
    if (mp->m_size == 0)
	goto ioerr;

#if 0
    /* Handle KLD module data */
    if (!kernel && ((err = aout_fixupkldmod(mp, &ehdr)) != 0))
	goto oerr;
#endif

    /* save exec header as metadata */
    mod_addmetadata(mp, MODINFOMD_AOUTEXEC, sizeof(struct exec), &ehdr);

    /* Load OK, return module pointer */
    *result = (struct loaded_module *)mp;
    err = 0;
    goto out;
    
 ioerr:
    err = EIO;
 oerr:
    mod_discard(mp);
 out:
    close(fd);
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
aout_loadimage(struct loaded_module *mp, int fd, vm_offset_t loadaddr, struct exec *ehdr, int kernel)
{
    u_int		pad;
    vm_offset_t		addr;
    size_t		ss;
    ssize_t		result;
    vm_offset_t		ssym, esym;
    
    addr = loadaddr;
    lseek(fd, (off_t)N_TXTOFF(*ehdr), SEEK_SET);

    /* text segment */
    printf("  text=0x%lx ", ehdr->a_text);
    result = archsw.arch_readin(fd, addr, ehdr->a_text);
    if (result < 0 || (size_t)result != ehdr->a_text)
	return(0);
    addr += ehdr->a_text;

    /* data segment */
    printf("data=0x%lx ", ehdr->a_data);
    result = archsw.arch_readin(fd, addr, ehdr->a_data);
    if (result < 0 || (size_t)result != ehdr->a_data)
	return(0);
    addr += ehdr->a_data;

    /* For kernels, we pad the BSS to a page boundary */
    if (kernel) {
	pad = (u_int)ehdr->a_bss & PAGE_MASK;
	if (pad != 0) {
	    pad = PAGE_SIZE - pad;
	    ehdr->a_bss += pad;
	}
    }
    printf("bss=0x%lx ", ehdr->a_bss);
    addr += ehdr->a_bss;

    /* symbol table size */
    ssym = esym = addr;
    if(ehdr->a_syms!=NULL) {
    	archsw.arch_copyin(&ehdr->a_syms, addr, sizeof(ehdr->a_syms));
    	addr += sizeof(ehdr->a_syms);

    	/* symbol table */
    	printf("symbols=[0x%lx+0x%lx", (long)sizeof(ehdr->a_syms),ehdr->a_syms);
	result = archsw.arch_readin(fd, addr, ehdr->a_syms);
	if (result < 0 || (size_t)result != ehdr->a_syms)
		return(0);
    	addr += ehdr->a_syms;

    	/* string table */
    	read(fd, &ss, sizeof(ss));
    	archsw.arch_copyin(&ss, addr, sizeof(ss));
    	addr += sizeof(ss);
    	ss -= sizeof(ss);
    	printf("+0x%lx+0x%x]", (long)sizeof(ss), ss);
	result = archsw.arch_readin(fd, addr, ss);
	if (result < 0 || (size_t)result != ss)
		return(0);
    	addr += ss;
	esym = addr;

    	mod_addmetadata(mp, MODINFOMD_SSYM, sizeof(ssym), &ssym);
    	mod_addmetadata(mp, MODINFOMD_ESYM, sizeof(esym), &esym);
    } else {
	printf("symbols=[none]");
    }
    printf("\n");

    return(addr - loadaddr);
}


