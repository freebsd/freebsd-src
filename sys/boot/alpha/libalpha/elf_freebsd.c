/* $Id: elf_freebsd.c,v 1.1.1.1 1998/08/21 03:17:42 msmith Exp $ */
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <machine/elf.h>
#include <machine/prom.h>
#include <machine/rpb.h>
#include <machine/bootinfo.h>

#include "bootstrap.h"

#define _KERNEL

static int	elf_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result);
static int	elf_exec(struct loaded_module *amp);
static int	elf_load(int fd, Elf_Ehdr *elf, vm_offset_t dest);

struct module_format alpha_elf = { elf_loadmodule, elf_exec };

vm_offset_t ffp_save, ptbr_save;
vm_offset_t ssym, esym;

static int
elf_loadmodule(char *filename, vm_offset_t dest, struct loaded_module **result)
{
    struct loaded_module	*mp;
    Elf_Ehdr hdr;
    ssize_t nr;
    int fd, rval;

    /* Open the file. */
    rval = 1;
    if ((fd = open(filename, 0)) < 0) {
	(void)printf("open %s: %s\n", filename, strerror(errno));
	goto err;
    }

    /* Read the exec header. */
    if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
	(void)printf("read header: %s\n", strerror(errno));
	goto err;
    }

    if (!(hdr.e_ident[0] == ELFMAG0
	  && hdr.e_ident[1] == ELFMAG1
	  && hdr.e_ident[2] == ELFMAG2
	  && hdr.e_ident[3] == ELFMAG3)) {
	(void)printf("%s: unknown executable format\n", filename);
	goto err;
    }
		
    /* 
     * Ok, we think this is for us.
     */
    mp = malloc(sizeof(struct loaded_module));
    mp->m_name = strdup(filename);	/* XXX should we prune the name? */
    mp->m_type = strdup("elf kernel");	/* XXX only if that's what we really are */
    mp->m_args = NULL;			/* XXX should we put the bootstrap args here and parse later? */
    mp->m_metadata = NULL;
    dest = (vm_offset_t) hdr.e_entry;
    mp->m_addr = dest;
    if (mod_findmodule(NULL, NULL) != NULL) {
	printf("elf_loadmodule: kernel already loaded\n");
	rval = EPERM;
	goto err;
    }
    rval = elf_load(fd, &hdr, (vm_offset_t) dest);

    /* save ELF header as metadata */
    mod_addmetadata(mp, MODINFOMD_ELFHDR, sizeof(Elf_Ehdr), &hdr);

    *result = (struct loaded_module *)mp;

 err:
    if (fd >= 0)
	(void)close(fd);
    return (rval);
}

static int
elf_load(int fd, Elf_Ehdr *elf, vm_offset_t dest)
{
	Elf_Shdr *shp;
	Elf_Off off;
	int i;
	int first = 1;
	int havesyms;

	for (i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		if (lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET)
		    == -1)  {
			(void)printf("lseek phdr: %s\n", strerror(errno));
			return (1);
		}
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			(void)printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		(void)printf("%s%lu", first ? "" : "+", phdr.p_filesz);
		if (lseek(fd, phdr.p_offset, SEEK_SET) == -1)  {
		    (void)printf("lseek text: %s\n", strerror(errno));
		    return (1);
		}
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			(void)printf("read text: %s\n", strerror(errno));
			return (1);
		}
		if (first || ffp_save < phdr.p_vaddr + phdr.p_memsz)
			ffp_save = phdr.p_vaddr + phdr.p_memsz;

		/* Zero out bss. */
		if (phdr.p_filesz < phdr.p_memsz) {
			(void)printf("+%lu", phdr.p_memsz - phdr.p_filesz);
			bzero((void *)(phdr.p_vaddr + phdr.p_filesz),
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}
	/*
	 * Copy the ELF and section headers.
	 */
	ffp_save = roundup(ffp_save, sizeof(long));
	ssym = ffp_save;
	bcopy(elf, (void *)ffp_save, sizeof(Elf_Ehdr));
	ffp_save += sizeof(Elf_Ehdr);
	if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
		printf("lseek section headers: %s\n", strerror(errno));
		return (1);
	}
	if (read(fd, (void *)ffp_save, elf->e_shnum * sizeof(Elf_Shdr)) !=
	    elf->e_shnum * sizeof(Elf_Shdr)) {
		printf("read section headers: %s\n", strerror(errno));
		return (1);
	}
	shp = (Elf_Shdr *)ffp_save;
	ffp_save += roundup((elf->e_shnum * sizeof(Elf_Shdr)), sizeof(long));

	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned. Don't bother with string tables if
	 * there are no symbol sections.
	 */
	off = roundup((sizeof(Elf_Ehdr) + (elf->e_shnum * sizeof(Elf_Shdr))),
	    sizeof(long));
	for (havesyms = i = 0; i < elf->e_shnum; i++)
		if (shp[i].sh_type == SHT_SYMTAB)
			havesyms = 1;
	for (first = 1, i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type == SHT_SYMTAB ||
		    shp[i].sh_type == SHT_STRTAB) {
			printf("%s%ld", first ? " [" : "+", shp[i].sh_size);
			if (havesyms) {
				if (lseek(fd, shp[i].sh_offset, SEEK_SET)
					== -1) {
					printf("\nlseek symbols: %s\n",
					    strerror(errno));
					return (1);
				}
				if (read(fd, (void *)ffp_save, shp[i].sh_size)
					!= shp[i].sh_size) {
					printf("\nread symbols: %s\n",
					    strerror(errno));
					return (1);
				}
			}
			ffp_save += roundup(shp[i].sh_size, sizeof(long));
			shp[i].sh_offset = off;
			off += roundup(shp[i].sh_size, sizeof(long));
			first = 0;
		}
	}
	esym = ffp_save;

	if (first == 0)
		printf("]");

	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PAGE_MASK) & ~PAGE_MASK)
	    >> PAGE_SHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");

	/*
	 * Frob the copied ELF header to give information relative
	 * to ssym.
	 */
	elf = (Elf_Ehdr *)ssym;
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;

	return (0);
}

static int
elf_exec(struct loaded_module *mp)
{
    static struct bootinfo_v1	bootinfo_v1;
    struct module_metadata	*md;
    Elf_Ehdr			*hdr;

    if ((md = mod_findmetadata(mp, MODINFOMD_ELFHDR)) == NULL)
	return(EFTYPE);			/* XXX actually EFUCKUP */
    hdr = (Elf_Ehdr *)&(md->md_data);

    /*
     * Fill in the bootinfo for the kernel.
     */
    bzero(&bootinfo_v1, sizeof(bootinfo_v1));
    bootinfo_v1.ssym = ssym;
    bootinfo_v1.esym = esym;
    strncpy(bootinfo_v1.booted_kernel, mp->m_name,
	    sizeof(bootinfo_v1.booted_kernel));
    prom_getenv(PROM_E_BOOTED_OSFLAGS, bootinfo_v1.boot_flags,
		sizeof(bootinfo_v1.boot_flags));
    bootinfo_v1.hwrpb = (void *)HWRPB_ADDR;
    bootinfo_v1.hwrpbsize = ((struct rpb *)HWRPB_ADDR)->rpb_size;
    bootinfo_v1.cngetc = NULL;
    bootinfo_v1.cnputc = NULL;
    bootinfo_v1.cnpollc = NULL;

    printf("Entering %s at 0x%lx...\n", mp->m_name, hdr->e_entry);
    closeall();
    alpha_pal_imb();
    (*(void (*)())hdr->e_entry)(ffp_save, ptbr_save,
			       BOOTINFO_MAGIC, &bootinfo_v1, 1, 0);
}



