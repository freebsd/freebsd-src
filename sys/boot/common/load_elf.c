/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Peter Wemm <peter@freebsd.org>
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
 *	$Id: load_elf.c,v 1.1 1998/09/30 19:38:26 peter Exp $
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <stand.h>
#define FREEBSD_ELF
#include <link.h>

#include "bootstrap.h"

static int		elf_loadimage(struct loaded_module *mp, int fd, vm_offset_t *loadaddr, Elf_Ehdr *ehdr, int kernel);
#if 0
static vm_offset_t	elf_findkldident(struct loaded_module *mp, Elf_Ehdr *ehdr);
static int		elf_fixupkldmod(struct loaded_module *mp, Elf_Ehdr *ehdr);
#endif

char	*elf_kerneltype = "elf kernel";
char	*elf_moduletype = "elf module";

/*
 * Attempt to load the file (file) as an ELF module.  It will be stored at
 * (dest), and a pointer to a module structure describing the loaded object
 * will be saved in (result).
 */
int
elf_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result)
{
    struct loaded_module	*mp, *kmp;
    Elf_Ehdr			ehdr;
    int				fd;
    vm_offset_t			addr;
    int				err, kernel;
    u_int			pad;

    mp = NULL;
    
    /*
     * Open the image, read and validate the ELF header 
     */
    if (filename == NULL)	/* can't handle nameless */
	return(EFTYPE);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(errno);
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
	err = EFTYPE;		/* could be EIO, but may be small file */
	goto oerr;
    }

    /* Is it ELF? */
    if (!IS_ELF(ehdr)) {
	err = EFTYPE;
	goto oerr;
    }
    if (ehdr.e_ident[EI_CLASS] != ELF_TARG_CLASS ||	/* Layout ? */
	ehdr.e_ident[EI_DATA] != ELF_TARG_DATA ||
	ehdr.e_ident[EI_VERSION] != EV_CURRENT ||	/* Version ? */
	ehdr.e_version != EV_CURRENT ||
	ehdr.e_machine != ELF_TARG_MACH) {		/* Machine ? */
	err = EFTYPE;
	goto oerr;
    }


    /*
     * Check to see what sort of module we are.
     */
    kmp = mod_findmodule(NULL, NULL);
    if (ehdr.e_type == ET_DYN) {
	/* Looks like a kld module */
	if (kmp == NULL) {
	    printf("elf_loadmodule: can't load module before kernel\n");
	    err = EPERM;
	    goto oerr;
	}
	if (strcmp(elf_kerneltype, kmp->m_type)) {
	    printf("elf_loadmodule: can't load module with kernel type '%s'\n", kmp->m_type);
	    err = EPERM;
	    goto oerr;
	}
	/* Looks OK, got ahead */
	kernel = 0;

	/* Page-align the load address */
	addr = dest;
	pad = (u_int)addr & PAGE_MASK;
	if (pad != 0) {
	    pad = PAGE_SIZE - pad;
	    addr += pad;
	}
    } else if (ehdr.e_type == ET_EXEC) {
	/* Looks like a kernel */
	if (kmp != NULL) {
	    printf("elf_loadmodule: kernel already loaded\n");
	    err = EPERM;
	    goto oerr;
	}
	/* 
	 * Calculate destination address based on kernel entrypoint 	
	 */
	dest = (vm_offset_t) ehdr.e_entry;
	if (dest == 0) {
	    printf("elf_loadmodule: not a kernel (maybe static binary?)\n");
	    err = EPERM;
	    goto oerr;
	}
	kernel = 1;

	addr = dest;
    } else {
	err = EFTYPE;
	goto oerr;
    }

    /* 
     * Ok, we think we should handle this.
     */
    mp = mod_allocmodule();
    if (kernel)
	mp->m_name = strdup(filename);		/* XXX should we prune the name? */
    mp->m_type = strdup(kernel ? elf_kerneltype : elf_moduletype);

    printf("%s at %p\n", filename, (void *) addr);

    mp->m_size = elf_loadimage(mp, fd, &addr, &ehdr, kernel);
    if (mp->m_size == 0)
	goto ioerr;
    mp->m_addr = addr;			/* save the aligned load address */

#if 0
    /* Handle KLD module data */
    if (!kernel && ((err = elf_fixupkldmod(mp, &ehdr)) != 0))
	goto oerr;
#endif

    /* save exec header as metadata */
    mod_addmetadata(mp, MODINFOMD_ELFHDR, sizeof(ehdr), &ehdr);

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
 * the Elf header, load the image at (addr)
 */
static int
elf_loadimage(struct loaded_module *mp, int fd, vm_offset_t *addr,
	      Elf_Ehdr *ehdr, int kernel)
{
    int 	i, j;
    Elf_Phdr	*phdr;
    Elf_Shdr	*shdr;
    Elf_Ehdr	local_ehdr;
    int		ret;
    vm_offset_t firstaddr;
    vm_offset_t lastaddr;
    vm_offset_t	off;
    void	*buf;
    size_t	resid, chunk;
    vm_offset_t	dest;
    char	*secname;
    vm_offset_t	shdrpos;
    vm_offset_t	ssym, esym;

    ret = 0;
    firstaddr = lastaddr = 0;
    if (kernel)
#ifdef __i386__
	off = 0x10000000;	/* -0xf0000000  - i386 relocates after locore */
#else
	off = 0;		/* alpha is direct mapped for kernels */
#endif
    else
	off = *addr;		/* load relative to passed address */

    phdr = malloc(ehdr->e_phnum * sizeof(*phdr));
    if (phdr == NULL)
	goto out;

    if (lseek(fd, ehdr->e_phoff, SEEK_SET) == -1) {
	printf("elf_loadexec: lseek for phdr failed\n");
	goto out;
    }
    if (read(fd, phdr, ehdr->e_phnum * sizeof(*phdr)) !=
	ehdr->e_phnum * sizeof(*phdr)) {
	printf("elf_loadexec: cannot read program header\n");
	goto out;
    }

    for (i = 0; i < ehdr->e_phnum; i++) {
	/* We want to load PT_LOAD segments only.. */
	if (phdr[i].p_type != PT_LOAD)
	    continue;

	printf("Segment: 0x%lx@0x%lx -> 0x%lx-0x%lx",
	    (long)phdr[i].p_filesz, (long)phdr[i].p_offset,
	    (long)(phdr[i].p_vaddr + off),
	    (long)(phdr[i].p_vaddr + off + phdr[i].p_memsz - 1));

	if (lseek(fd, phdr[i].p_offset, SEEK_SET) == -1) {
	    printf("\nelf_loadexec: cannot seek\n");
	    goto out;
	}
	if (archsw.arch_readin(fd, phdr[i].p_vaddr + off, phdr[i].p_filesz) !=
	    phdr[i].p_filesz) {
	    printf("\nelf_loadexec: archsw.readin failed\n");
	    goto out;
	}
	/* clear space from oversized segments; eg: bss */
	if (phdr[i].p_filesz < phdr[i].p_memsz) {
	    printf(" (bss: 0x%lx-0x%lx)",
		(long)(phdr[i].p_vaddr + off + phdr[i].p_filesz),
		(long)(phdr[i].p_vaddr + off + phdr[i].p_memsz - 1));

	    /* no archsw.arch_bzero */
	    buf = malloc(PAGE_SIZE);
	    bzero(buf, PAGE_SIZE);
	    resid = phdr[i].p_memsz - phdr[i].p_filesz;
	    dest = phdr[i].p_vaddr + off + phdr[i].p_filesz;
	    while (resid > 0) {
		chunk = min(PAGE_SIZE, resid);
		archsw.arch_copyin(buf, dest, chunk);
		resid -= chunk;
		dest += chunk;
	    }
	    free(buf);
	}
	printf("\n");

	if (firstaddr < (phdr[i].p_vaddr + off))
	    firstaddr = phdr[i].p_vaddr + off;
	if (lastaddr < (phdr[i].p_vaddr + off + phdr[i].p_memsz))
	    lastaddr = phdr[i].p_vaddr + off + phdr[i].p_memsz;
    }

    /*
     * Now grab the symbol tables.  This isn't easy if we're reading a
     * .gz file.  I think the rule is going to have to be that you must
     * strip a file to remove symbols before gzipping it so that we do not
     * try to lseek() on it.  The layout is a bit wierd, but it's what
     * the NetBSD-derived ddb/db_elf.c wants.
     */
    lastaddr = roundup(lastaddr, sizeof(long));
    chunk = ehdr->e_shnum * ehdr->e_shentsize;
    shdr = malloc(chunk);
    if (shdr == NULL)
	goto nosyms;
    ssym = lastaddr;
    printf("Symbols: ELF Ehdr @ 0x%x; ", lastaddr);
    lastaddr += sizeof(*ehdr);
    lastaddr = roundup(lastaddr, sizeof(long));
    /* Copy out executable header modified for base offsets */
    local_ehdr = *ehdr;
    local_ehdr.e_phoff = 0;
    local_ehdr.e_phentsize = 0;
    local_ehdr.e_phnum = 0;
    local_ehdr.e_shoff = lastaddr - ssym;
    archsw.arch_copyin(&local_ehdr, ssym, sizeof(*ehdr));
    if (lseek(fd, ehdr->e_shoff, SEEK_SET) == -1) {
	printf("elf_loadimage: cannot lseek() to section headers\n");
	lastaddr = ssym;	/* wind back */
	ssym = 0;
	goto nosyms;
    }
    if (read(fd, shdr, chunk) != chunk) {
	printf("elf_loadimage: read section headers failed\n");
	lastaddr = ssym;	/* wind back */
	ssym = 0;
	goto nosyms;
    }
    shdrpos = lastaddr;
    printf("Section table: 0x%x@0x%x\n", chunk, shdrpos);
    lastaddr += chunk;
    lastaddr = roundup(lastaddr, sizeof(long));
    for (i = 0; i < ehdr->e_shnum; i++) {
	switch(shdr[i].sh_type) {
	    /*
	     * These are the symbol tables.  Their names are relative to
	     * an arbitary string table.
	     */
	    case SHT_SYMTAB:		/* Symbol table */
		secname = "symtab";
		break;
	    case SHT_DYNSYM:		/* Dynamic linking symbol table */
		secname = "dynsym";
		break;
	    /*
	     * And here are the string tables.  These can be referred to from
	     * a number of sources, including the dynsym, the section table
	     * names itself, etc.
	     */
	    case SHT_STRTAB:		/* String table */
		secname = "strtab";
		break;
	    default:			/* Skip it */
		continue;
	}
	for (j = 0; j < ehdr->e_phnum; j++) {
	    if (phdr[j].p_type != PT_LOAD)
		continue;
	    if (shdr[i].sh_offset >= phdr[j].p_offset &&
		(shdr[i].sh_offset + shdr[i].sh_size <=
		 phdr[j].p_offset + phdr[j].p_filesz)) {
		shdr[i].sh_offset = 0;
		shdr[i].sh_size = 0;
		break;
	    }
	}
	if (shdr[i].sh_offset == 0 || shdr[i].sh_size == 0)
	    continue;		/* alread loaded in a PT_LOAD above */

	printf("%s: 0x%x@0x%x -> 0x%x-0x%x\n", secname,
	    shdr[i].sh_size, shdr[i].sh_offset,
	    lastaddr, lastaddr + shdr[i].sh_size);
	  
	if (lseek(fd, shdr[i].sh_offset, SEEK_SET) == -1) {
	    printf("\nelf_loadimage: could not seek for symbols - skipped!\n");
	    shdr[i].sh_offset = 0;
	    shdr[i].sh_size = 0;
	    continue;
	}
	if (archsw.arch_readin(fd, lastaddr, shdr[i].sh_size) !=
	    shdr[i].sh_size) {
	    printf("\nelf_loadimage: could not read symbols - skipped!\n");
	    shdr[i].sh_offset = 0;
	    shdr[i].sh_size = 0;
	    continue;
	}
	/* Reset offsets relative to ssym */
	shdr[i].sh_offset = lastaddr - ssym;
	lastaddr += shdr[i].sh_size;
	lastaddr = roundup(lastaddr, sizeof(long));
    }
    archsw.arch_copyin(shdr, lastaddr, sizeof(*ehdr));
    esym = lastaddr;

    mod_addmetadata(mp, MODINFOMD_ELFSSYM, sizeof(ssym), &ssym);
    mod_addmetadata(mp, MODINFOMD_ELFESYM, sizeof(esym), &esym);

nosyms:

    ret = lastaddr - firstaddr;
    *addr = firstaddr;
out:
    if (phdr)
	free(phdr);
    return ret;
}
