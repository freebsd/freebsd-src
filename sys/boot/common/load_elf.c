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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <stand.h>
#define FREEBSD_ELF
#include <link.h>

#include "bootstrap.h"

static int	elf_loadimage(struct loaded_module *mp, int fd, vm_offset_t loadaddr, Elf_Ehdr *ehdr, int kernel, caddr_t firstpage, int firstlen);

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
    Elf_Ehdr			*ehdr;
    int				fd;
    int				err, kernel;
    u_int			pad;
    char			*s;
    caddr_t			firstpage;
    int				firstlen;

    mp = NULL;
    
    /*
     * Open the image, read and validate the ELF header 
     */
    if (filename == NULL)	/* can't handle nameless */
	return(EFTYPE);
    if ((fd = open(filename, O_RDONLY)) == -1)
	return(errno);
    firstpage = malloc(PAGE_SIZE);
    if (firstpage == NULL)
	return(ENOMEM);
    firstlen = read(fd, firstpage, PAGE_SIZE);
    if (firstlen <= sizeof(ehdr)) {
	err = EFTYPE;		/* could be EIO, but may be small file */
	goto oerr;
    }
    ehdr = (Elf_Ehdr *)firstpage;

    /* Is it ELF? */
    if (!IS_ELF(*ehdr)) {
	err = EFTYPE;
	goto oerr;
    }
    if (ehdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||	/* Layout ? */
	ehdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	ehdr->e_ident[EI_VERSION] != EV_CURRENT ||	/* Version ? */
	ehdr->e_version != EV_CURRENT ||
	ehdr->e_machine != ELF_TARG_MACH) {		/* Machine ? */
	err = EFTYPE;
	goto oerr;
    }


    /*
     * Check to see what sort of module we are.
     */
    kmp = mod_findmodule(NULL, NULL);
    if (ehdr->e_type == ET_DYN) {
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
	pad = (u_int)dest & PAGE_MASK;
	if (pad != 0) {
	    pad = PAGE_SIZE - pad;
	    dest += pad;
	}
    } else if (ehdr->e_type == ET_EXEC) {
	/* Looks like a kernel */
	if (kmp != NULL) {
	    printf("elf_loadmodule: kernel already loaded\n");
	    err = EPERM;
	    goto oerr;
	}
	/* 
	 * Calculate destination address based on kernel entrypoint 	
	 */
	dest = (vm_offset_t) ehdr->e_entry;
	if (dest == 0) {
	    printf("elf_loadmodule: not a kernel (maybe static binary?)\n");
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
    if (mp == NULL) {
	    printf("elf_loadmodule: cannot allocate module info\n");
	    err = EPERM;
	    goto out;
    }
    if (kernel)
	setenv("kernelname", filename, 1);
    s = strrchr(filename, '/');
    if (s)
	mp->m_name = strdup(s + 1);
    else
	mp->m_name = strdup(filename);
    mp->m_type = strdup(kernel ? elf_kerneltype : elf_moduletype);

#ifdef ELF_VERBOSE
    if (kernel)
	printf("%s entry at %p\n", filename, (void *) dest);
#else
    printf("%s ", filename);
#endif

    mp->m_size = elf_loadimage(mp, fd, dest, ehdr, kernel, firstpage, firstlen);
    if (mp->m_size == 0 || mp->m_addr == 0)
	goto ioerr;

    /* save exec header as metadata */
    mod_addmetadata(mp, MODINFOMD_ELFHDR, sizeof(*ehdr), ehdr);

    /* Load OK, return module pointer */
    *result = (struct loaded_module *)mp;
    err = 0;
    goto out;
    
 ioerr:
    err = EIO;
 oerr:
    mod_discard(mp);
 out:
    if (firstpage)
	free(firstpage);
    close(fd);
    return(err);
}

/*
 * With the file (fd) open on the image, and (ehdr) containing
 * the Elf header, load the image at (off)
 */
static int
elf_loadimage(struct loaded_module *mp, int fd, vm_offset_t off,
	      Elf_Ehdr *ehdr, int kernel, caddr_t firstpage, int firstlen)
{
    int 	i, j;
    Elf_Phdr	*phdr;
    Elf_Shdr	*shdr;
    int		ret;
    vm_offset_t firstaddr;
    vm_offset_t lastaddr;
    void	*buf;
    size_t	resid, chunk;
    ssize_t	result;
    vm_offset_t	dest;
    vm_offset_t	ssym, esym;
    Elf_Dyn	*dp;
    int		ndp;
    char	*s;
    char	*strtab;
    size_t	strsz;
    int		symstrindex;
    int		symtabindex;
    long	size;
    u_int	fpcopy;

    dp = NULL;
    shdr = NULL;
    ret = 0;
    firstaddr = lastaddr = 0;
    if (kernel) {
#ifdef __i386__
	off = - (off & 0xff000000u);	/* i386 relocates after locore */
#else
	off = 0;		/* alpha is direct mapped for kernels */
#endif
    }

    if ((ehdr->e_phoff + ehdr->e_phnum * sizeof(*phdr)) > firstlen) {
	printf("elf_loadimage: program header not within first page\n");
	goto out;
    }
    phdr = (Elf_Phdr *)(firstpage + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
	/* We want to load PT_LOAD segments only.. */
	if (phdr[i].p_type != PT_LOAD)
	    continue;

#ifdef ELF_VERBOSE
	printf("Segment: 0x%lx@0x%lx -> 0x%lx-0x%lx",
	    (long)phdr[i].p_filesz, (long)phdr[i].p_offset,
	    (long)(phdr[i].p_vaddr + off),
	    (long)(phdr[i].p_vaddr + off + phdr[i].p_memsz - 1));
#else
	if ((phdr[i].p_flags & PF_W) == 0) {
	    printf("text=0x%lx ", (long)phdr[i].p_filesz);
	} else {
	    printf("data=0x%lx", (long)phdr[i].p_filesz);
	    if (phdr[i].p_filesz < phdr[i].p_memsz)
		printf("+0x%lx", (long)(phdr[i].p_memsz -phdr[i].p_filesz));
	    printf(" ");
	}
#endif
	fpcopy = 0;
	if (firstlen > phdr[i].p_offset) {
	    fpcopy = firstlen - phdr[i].p_offset;
	    archsw.arch_copyin(firstpage + phdr[i].p_offset,
			       phdr[i].p_vaddr + off, fpcopy);
	}
	if (phdr[i].p_filesz > fpcopy) {
	    if (lseek(fd, phdr[i].p_offset + fpcopy, SEEK_SET) == -1) {
		printf("\nelf_loadexec: cannot seek\n");
		goto out;
	    }
	    if (archsw.arch_readin(fd, phdr[i].p_vaddr + off + fpcopy,
		phdr[i].p_filesz - fpcopy) != phdr[i].p_filesz - fpcopy) {
		printf("\nelf_loadexec: archsw.readin failed\n");
		goto out;
	    }
	}
	/* clear space from oversized segments; eg: bss */
	if (phdr[i].p_filesz < phdr[i].p_memsz) {
#ifdef ELF_VERBOSE
	    printf(" (bss: 0x%lx-0x%lx)",
		(long)(phdr[i].p_vaddr + off + phdr[i].p_filesz),
		(long)(phdr[i].p_vaddr + off + phdr[i].p_memsz - 1));
#endif

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
#ifdef ELF_VERBOSE
	printf("\n");
#endif

	if (firstaddr == 0 || firstaddr > (phdr[i].p_vaddr + off))
	    firstaddr = phdr[i].p_vaddr + off;
	if (lastaddr == 0 || lastaddr < (phdr[i].p_vaddr + off + phdr[i].p_memsz))
	    lastaddr = phdr[i].p_vaddr + off + phdr[i].p_memsz;
    }
    lastaddr = roundup(lastaddr, sizeof(long));

    /*
     * Now grab the symbol tables.  This isn't easy if we're reading a
     * .gz file.  I think the rule is going to have to be that you must
     * strip a file to remove symbols before gzipping it so that we do not
     * try to lseek() on it.
     */
    chunk = ehdr->e_shnum * ehdr->e_shentsize;
    if (chunk == 0 || ehdr->e_shoff == 0)
	goto nosyms;
    shdr = malloc(chunk);
    if (shdr == NULL)
	goto nosyms;
    if (lseek(fd, ehdr->e_shoff, SEEK_SET) == -1) {
	printf("\nelf_loadimage: cannot lseek() to section headers");
	goto nosyms;
    }
    if (read(fd, shdr, chunk) != chunk) {
	printf("\nelf_loadimage: read section headers failed");
	goto nosyms;
    }
    symtabindex = -1;
    symstrindex = -1;
    for (i = 0; i < ehdr->e_shnum; i++) {
	if (shdr[i].sh_type != SHT_SYMTAB)
	    continue;
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
	/* Save it for loading below */
	symtabindex = i;
	symstrindex = shdr[i].sh_link;
    }
    if (symtabindex < 0 || symstrindex < 0)
	goto nosyms;

    /* Ok, committed to a load. */
#ifndef ELF_VERBOSE
    printf("syms=[");
#endif
    ssym = lastaddr;
    for (i = symtabindex; i >= 0; i = symstrindex) {
#ifdef ELF_VERBOSE
	char	*secname;

	switch(shdr[i].sh_type) {
	    case SHT_SYMTAB:		/* Symbol table */
		secname = "symtab";
		break;
	    case SHT_STRTAB:		/* String table */
		secname = "strtab";
		break;
	    default:
		secname = "WHOA!!";
		break;
	}
#endif

	size = shdr[i].sh_size;
	archsw.arch_copyin(&size, lastaddr, sizeof(size));
	lastaddr += sizeof(long);

#ifdef ELF_VERBOSE
	printf("\n%s: 0x%lx@0x%lx -> 0x%lx-0x%lx", secname,
	    shdr[i].sh_size, shdr[i].sh_offset,
	    lastaddr, lastaddr + shdr[i].sh_size);
#else
	if (i == symstrindex)
	    printf("+");
	printf("0x%lx+0x%lx", (long)sizeof(size), size);
#endif

	if (lseek(fd, shdr[i].sh_offset, SEEK_SET) == -1) {
	    printf("\nelf_loadimage: could not seek for symbols - skipped!");
	    lastaddr = ssym;
	    ssym = 0;
	    goto nosyms;
	}
	if (archsw.arch_readin(fd, lastaddr, shdr[i].sh_size) !=
	    shdr[i].sh_size) {
	    printf("\nelf_loadimage: could not read symbols - skipped!");
	    lastaddr = ssym;
	    ssym = 0;
	    goto nosyms;
	}
	/* Reset offsets relative to ssym */
	lastaddr += shdr[i].sh_size;
	lastaddr = roundup(lastaddr, sizeof(long));
	if (i == symtabindex)
	    symtabindex = -1;
	else if (i == symstrindex)
	    symstrindex = -1;
    }
    esym = lastaddr;
#ifndef ELF_VERBOSE
    printf("]");
#endif

    mod_addmetadata(mp, MODINFOMD_SSYM, sizeof(ssym), &ssym);
    mod_addmetadata(mp, MODINFOMD_ESYM, sizeof(esym), &esym);

nosyms:
    printf("\n");

    ret = lastaddr - firstaddr;
    mp->m_addr = firstaddr;

    for (i = 0; i < ehdr->e_phnum; i++) {
	if (phdr[i].p_type == PT_DYNAMIC) {
	    dp = (Elf_Dyn *)(phdr[i].p_vaddr);
	    mod_addmetadata(mp, MODINFOMD_DYNAMIC, sizeof(dp), &dp);
	    dp = NULL;
	    break;
	}
    }

    if (kernel)		/* kernel must not depend on anything */
	goto out;

    ndp = 0;
    for (i = 0; i < ehdr->e_phnum; i++) {
	if (phdr[i].p_type == PT_DYNAMIC) {
	    ndp = phdr[i].p_filesz / sizeof(Elf_Dyn);
	    dp = malloc(phdr[i].p_filesz);
	    archsw.arch_copyout(phdr[i].p_vaddr + off, dp, phdr[i].p_filesz);
	}
    }
    if (dp == NULL || ndp == 0)
	goto out;
    strtab = NULL;
    strsz = 0;
    for (i = 0; i < ndp; i++) {
	if (dp[i].d_tag == NULL)
	    break;
	switch (dp[i].d_tag) {
	case DT_STRTAB:
	    strtab = (char *)(dp[i].d_un.d_ptr + off);
	    break;
	case DT_STRSZ:
	    strsz = dp[i].d_un.d_val;
	    break;
	default:
	    break;
	}
    }
    if (strtab == NULL || strsz == 0)
	goto out;

    for (i = 0; i < ndp; i++) {
	if (dp[i].d_tag == NULL)
	    break;
	if (dp[i].d_tag != DT_NEEDED)
	    continue;
	j = dp[i].d_un.d_ptr;
	if (j < 1 || j > (strsz - 2))
	    continue;	/* bad symbol name index */
	s = strdupout((vm_offset_t)&strtab[j]);
	mod_addmetadata(mp, MODINFOMD_DEPLIST, strlen(s) + 1, s);
	free(s);
    }

out:
    if (dp)
	free(dp);
    if (shdr)
	free(shdr);
    return ret;
}
