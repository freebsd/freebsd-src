/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
__RCSID("$NetBSD: exec_elf32.c,v 1.4 1997/08/12 06:07:24 mikel Exp $");
#endif
#endif
__FBSDID("$FreeBSD$");
 
#ifndef ELFSIZE
#define ELFSIZE         32
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "endian.h"
#include "extern.h"

#if (defined(NLIST_ELF32) && (ELFSIZE == 32)) || \
    (defined(NLIST_ELF64) && (ELFSIZE == 64))

#define	__ELF_WORD_SIZE ELFSIZE
#if (ELFSIZE == 32)
#include <sys/elf32.h>
#define	xewtoh(x)	((data == ELFDATA2MSB) ? be32toh(x) : le32toh(x))
#elif (ELFSIZE == 64)
#include <sys/elf64.h>
#define	xewtoh(x)	((data == ELFDATA2MSB) ? be64toh(x) : le64toh(x))
#endif
#include <sys/elf_generic.h>

#define CONCAT(x,y)     __CONCAT(x,y)
#define ELFNAME(x)      CONCAT(elf,CONCAT(ELFSIZE,CONCAT(_,x)))
#define ELFNAME2(x,y)   CONCAT(x,CONCAT(_elf,CONCAT(ELFSIZE,CONCAT(_,y))))
#define ELFNAMEEND(x)   CONCAT(x,CONCAT(_elf,ELFSIZE))
#define ELFDEFNNAME(x)  CONCAT(ELF,CONCAT(ELFSIZE,CONCAT(_,x)))

#define	xe16toh(x)	((data == ELFDATA2MSB) ? be16toh(x) : le16toh(x))
#define	xe32toh(x)	((data == ELFDATA2MSB) ? be32toh(x) : le32toh(x))
#define	htoxe32(x)	((data == ELFDATA2MSB) ? htobe32(x) : htole32(x))

struct listelem {
	struct listelem *next;
	void *mem;
	off_t file;
	size_t size;
};

static ssize_t
xreadatoff(int fd, void *buf, off_t off, size_t size, const char *fn)
{
	ssize_t rv;

	if (lseek(fd, off, SEEK_SET) != off) {
		perror(fn);
		return -1;
	}
	if ((rv = read(fd, buf, size)) != size) {
		fprintf(stderr, "%s: read error: %s\n", fn,
		    rv == -1 ? strerror(errno) : "short read");
		return -1;
	}
	return size;
}

static ssize_t
xwriteatoff(int fd, void *buf, off_t off, size_t size, const char *fn)
{
	ssize_t rv;

	if (lseek(fd, off, SEEK_SET) != off) {
		perror(fn);
		return -1;
	}
	if ((rv = write(fd, buf, size)) != size) {
		fprintf(stderr, "%s: write error: %s\n", fn,
		    rv == -1 ? strerror(errno) : "short write");
		return -1;
	}
	return size;
}

static void *
xmalloc(size_t size, const char *fn, const char *use)
{
	void *rv;

	rv = malloc(size);
	if (rv == NULL)
		fprintf(stderr, "%s: out of memory (allocating for %s)\n",
		    fn, use);
	return (rv);
}

int
ELFNAMEEND(check)(int fd, const char *fn)
{
	Elf_Ehdr eh;
	struct stat sb;
	unsigned char data;

	/*
	 * Check the header to maek sure it's an ELF file (of the
	 * appropriate size).
	 */
	if (fstat(fd, &sb) == -1)
		return 0;
	if (sb.st_size < sizeof eh)
		return 0;
	if (read(fd, &eh, sizeof eh) != sizeof eh)
		return 0;

	if (IS_ELF(eh) == 0)
                return 0;

	data = eh.e_ident[EI_DATA];

	switch (xe16toh(eh.e_machine)) {
	case EM_386: break;
	case EM_ALPHA: break;
#ifndef EM_IA_64
#define	EM_IA_64	50
#endif
	case EM_IA_64: break;
#ifndef EM_SPARCV9
#define	EM_SPARCV9	43
#endif
	case EM_SPARCV9: break;
/*        ELFDEFNNAME(MACHDEP_ID_CASES) */

        default:
                return 0;
        }

	return 1;
}

int
ELFNAMEEND(hide)(int fd, const char *fn)
{
	Elf_Ehdr ehdr;
	Elf_Shdr *shdrp = NULL, *symtabshdr, *strtabshdr;
	Elf_Sym *symtabp = NULL;
	char *strtabp = NULL;
	Elf_Word *symfwmap = NULL, *symrvmap = NULL, nsyms, nlocalsyms, ewi;
	struct listelem *relalist = NULL, *rellist = NULL, *tmpl;
	ssize_t shdrsize;
	int rv, i, weird;
	unsigned char data;

	rv = 0;
	if (xreadatoff(fd, &ehdr, 0, sizeof ehdr, fn) != sizeof ehdr)
		goto bad;

	data = ehdr.e_ident[EI_DATA];

	shdrsize = xe16toh(ehdr.e_shnum) * xe16toh(ehdr.e_shentsize);
	if ((shdrp = xmalloc(shdrsize, fn, "section header table")) == NULL)
		goto bad;
	if (xreadatoff(fd, shdrp, xewtoh(ehdr.e_shoff), shdrsize, fn) !=
	    shdrsize)
		goto bad;

	symtabshdr = strtabshdr = NULL;
	weird = 0;
	for (i = 0; i < xe16toh(ehdr.e_shnum); i++) {
		switch (xe32toh(shdrp[i].sh_type)) {
		case SHT_SYMTAB:
			if (symtabshdr != NULL)
				weird = 1;
			symtabshdr = &shdrp[i];
			strtabshdr = &shdrp[xe32toh(shdrp[i].sh_link)];
			break;
		case SHT_RELA:
			tmpl = xmalloc(sizeof *tmpl, fn, "rela list element");
			if (tmpl == NULL)
				goto bad;
			tmpl->mem = NULL;
			tmpl->file = shdrp[i].sh_offset;
			tmpl->size = shdrp[i].sh_size;
			tmpl->next = relalist;
			relalist = tmpl;
			break;
		case SHT_REL:
			tmpl = xmalloc(sizeof *tmpl, fn, "rel list element");
			if (tmpl == NULL)
				goto bad;
			tmpl->mem = NULL;
			tmpl->file = shdrp[i].sh_offset;
			tmpl->size = shdrp[i].sh_size;
			tmpl->next = rellist;
			rellist = tmpl;
			break;
		}
	}
	if (symtabshdr == NULL)
		goto out;
	if (strtabshdr == NULL)
		weird = 1;
	if (weird) {
		fprintf(stderr, "%s: weird executable (unsupported)\n", fn);
		goto bad;
	}

	/*
	 * load up everything we need
	 */

	/* symbol table */
	if ((symtabp = xmalloc(xewtoh(symtabshdr->sh_size), fn, "symbol table"))
	    == NULL)
		goto bad;
	if (xreadatoff(fd, symtabp, xewtoh(symtabshdr->sh_offset),
	    xewtoh(symtabshdr->sh_size), fn) != xewtoh(symtabshdr->sh_size))
		goto bad;

	/* string table */
	if ((strtabp = xmalloc(xewtoh(strtabshdr->sh_size), fn, "string table"))
	    == NULL)
		goto bad;
	if (xreadatoff(fd, strtabp, xewtoh(strtabshdr->sh_offset),
	    xewtoh(strtabshdr->sh_size), fn) != xewtoh(strtabshdr->sh_size))
		goto bad;

	/* any rela tables */
	for (tmpl = relalist; tmpl != NULL; tmpl = tmpl->next) {
		if ((tmpl->mem = xmalloc(xewtoh(tmpl->size), fn, "rela table"))
		    == NULL)
			goto bad;
		if (xreadatoff(fd, tmpl->mem, xewtoh(tmpl->file),
		    xewtoh(tmpl->size), fn) != xewtoh(tmpl->size))
			goto bad;
	}

	/* any rel tables */
	for (tmpl = rellist; tmpl != NULL; tmpl = tmpl->next) {
		if ((tmpl->mem = xmalloc(xewtoh(tmpl->size), fn, "rel table"))
		    == NULL)
			goto bad;
		if (xreadatoff(fd, tmpl->mem, xewtoh(tmpl->file),
		    xewtoh(tmpl->size), fn) != xewtoh(tmpl->size))
			goto bad;
	}

	/* Prepare data structures for symbol movement. */
	nsyms = xewtoh(symtabshdr->sh_size) / xewtoh(symtabshdr->sh_entsize);
	nlocalsyms = xe32toh(symtabshdr->sh_info);
	if ((symfwmap = xmalloc(nsyms * sizeof (Elf_Word), fn,
	    "symbol forward mapping table")) == NULL)
		goto bad;
	if ((symrvmap = xmalloc(nsyms * sizeof (Elf_Word), fn,
	    "symbol reverse mapping table")) == NULL)
		goto bad;

	/* init location -> symbol # table */
	for (ewi = 0; ewi < nsyms; ewi++)
		symrvmap[ewi] = ewi;

	/* move symbols, making them local */
	for (ewi = nlocalsyms; ewi < nsyms; ewi++) {
		Elf_Sym *sp, symswap;
		Elf_Word mapswap;

		sp = &symtabp[ewi];

		/* if it's on our keep list, don't move it */
		if (in_keep_list(strtabp + xe32toh(sp->st_name)))
			continue;

		/* if it's an undefined symbol, keep it */
		if (xe16toh(sp->st_shndx) == SHN_UNDEF)
			continue;

		/* adjust the symbol so that it's local */
		sp->st_info =
		    ELF_ST_INFO(STB_LOCAL, sp->st_info);
/*		    (STB_LOCAL << 4) | ELF_SYM_TYPE(sp->st_info); *//* XXX */

		/*
		 * move the symbol to its new location
		 */

		/* note that symbols in those locations have been swapped */
		mapswap = symrvmap[ewi];
		symrvmap[ewi] = symrvmap[nlocalsyms];
		symrvmap[nlocalsyms] = mapswap;

		/* and swap the symbols */
		symswap = *sp;
		*sp = symtabp[nlocalsyms];
		symtabp[nlocalsyms] = symswap;

		nlocalsyms++;			/* note new local sym */
	}
	symtabshdr->sh_info = htoxe32(nlocalsyms);

	/* set up symbol # -> location mapping table */
	for (ewi = 0; ewi < nsyms; ewi++)
		symfwmap[symrvmap[ewi]] = ewi;

	/* any rela tables */
	for (tmpl = relalist; tmpl != NULL; tmpl = tmpl->next) {
		Elf_Rela *relap = tmpl->mem;

		for (ewi = 0; ewi < xewtoh(tmpl->size) / sizeof(*relap); ewi++) {
			relap[ewi].r_info =
#if (ELFSIZE == 32)					/* XXX */
			    symfwmap[ELF_R_SYM(xe32toh(relap[ewi].r_info))] << 8 |
			    ELF_R_TYPE(xe32toh(relap[ewi].r_info));
#elif (ELFSIZE == 64)					/* XXX */
			    symfwmap[ELF_R_SYM(xewtoh(relap[ewi].r_info))] << 32 |
			    ELF_R_TYPE(xewtoh(relap[ewi].r_info));
#endif							/* XXX */
		}
	}

	/* any rel tables */
	for (tmpl = rellist; tmpl != NULL; tmpl = tmpl->next) {
		Elf_Rel *relp = tmpl->mem;

		for (ewi = 0; ewi < xewtoh(tmpl->size) / sizeof *relp; ewi++) {
			relp[ewi].r_info =
#if (ELFSIZE == 32)					/* XXX */
			    symfwmap[ELF_R_SYM(xe32toh(relp[ewi].r_info))] << 8 |
			    ELF_R_TYPE(xe32toh(relp[ewi].r_info));
#elif (ELFSIZE == 64)					/* XXX */
			    symfwmap[ELF_R_SYM(xewtoh(relp[ewi].r_info))] << 32 |
			    ELF_R_TYPE(xewtoh(relp[ewi].r_info));
#endif							/* XXX */
		}
	}

	/*
	 * write new tables to the file
	 */
	if (xwriteatoff(fd, shdrp, xewtoh(ehdr.e_shoff), shdrsize, fn) !=
	    shdrsize)
		goto bad;
	if (xwriteatoff(fd, symtabp, xewtoh(symtabshdr->sh_offset),
	    xewtoh(symtabshdr->sh_size), fn) != xewtoh(symtabshdr->sh_size))
		goto bad;
	for (tmpl = relalist; tmpl != NULL; tmpl = tmpl->next) {
		if (xwriteatoff(fd, tmpl->mem, xewtoh(tmpl->file),
		    xewtoh(tmpl->size), fn) != xewtoh(tmpl->size))
			goto bad;
	}
	for (tmpl = rellist; tmpl != NULL; tmpl = tmpl->next) {
		if (xwriteatoff(fd, tmpl->mem, xewtoh(tmpl->file),
		    xewtoh(tmpl->size), fn) != xewtoh(tmpl->size))
			goto bad;
	}

out:
	if (shdrp != NULL)
		free(shdrp);
	if (symtabp != NULL)
		free(symtabp);
	if (strtabp != NULL)
		free(strtabp);
	if (symfwmap != NULL)
		free(symfwmap);
	if (symrvmap != NULL)
		free(symrvmap);
	while ((tmpl = relalist) != NULL) {
		relalist = tmpl->next;
		if (tmpl->mem != NULL)
			free(tmpl->mem);
		free(tmpl);
	}
	while ((tmpl = rellist) != NULL) {
		rellist = tmpl->next;
		if (tmpl->mem != NULL)
			free(tmpl->mem);
		free(tmpl);
	}
	return (rv);

bad:
	rv = 1;
	goto out;
}

#endif /* include this size of ELF */
