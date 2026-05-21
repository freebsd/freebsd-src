/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <arpa/inet.h>

#include <a.out.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include <machine/elf.h>
#include <elf-hints.h>

int __fdnlist(int, struct nlist *);
int __elf_fdnlist(int, struct nlist *);
int __elf_is_okay__(Elf_Ehdr *);

int
nlist(const char *name, struct nlist *list)
{
	int fd, n;

	fd = _open(name, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0)
		return (-1);
	n = __fdnlist(fd, list);
	(void)_close(fd);
	return (n);
}

static struct nlist_handlers {
	int	(*fn)(int fd, struct nlist *list);
} nlist_fn[] = {
	{ __elf_fdnlist },
};

int
__fdnlist(int fd, struct nlist *list)
{
	int n = -1;
	unsigned int i;

	for (i = 0; i < nitems(nlist_fn); i++) {
		n = (nlist_fn[i].fn)(fd, list);
		if (n != -1)
			break;
	}
	return (n);
}

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

static int elf_scan_symtab(Elf_Shdr *, int, int, off_t, size_t, char *, size_t,
    struct nlist *, int);
static void elf_sym_to_nlist(struct nlist *, Elf_Sym *, Elf_Shdr *, int);

/*
 * __elf_is_okay__ - Determine if ehdr really
 * is ELF and valid for the target platform.
 *
 * WARNING:  This is NOT an ELF ABI function and
 * as such its use should be restricted.
 */
int
__elf_is_okay__(Elf_Ehdr *ehdr)
{
	int retval = 0;
	/*
	 * We need to check magic, class size, endianess,
	 * and version before we look at the rest of the
	 * Elf_Ehdr structure.  These few elements are
	 * represented in a machine independent fashion.
	 */
	if (IS_ELF(*ehdr) &&
	    ehdr->e_ident[EI_CLASS] == ELF_TARG_CLASS &&
	    ehdr->e_ident[EI_DATA] == ELF_TARG_DATA &&
	    ehdr->e_ident[EI_VERSION] == ELF_TARG_VER) {

		/* Now check the machine dependant header */
		if (ehdr->e_machine == ELF_TARG_MACH &&
		    ehdr->e_version == ELF_TARG_VER)
			retval = 1;
	}
	return retval;
}

int
__elf_fdnlist(int fd, struct nlist *list)
{
	struct nlist *p;
	Elf_Off symoff = 0, stroff = 0;
	Elf_Size symsize = 0, strsize = 0;
	Elf_Ssize i;
	int nent = -1;
	int errsave;
	Elf_Ehdr ehdr;
	Elf_Shdr *shdr;
	Elf_Size shdr_size;
	void *base;
	struct stat st;

	/* Make sure obj is OK */
	if (lseek(fd, 0, SEEK_SET) == -1 ||
	    _read(fd, &ehdr, sizeof(Elf_Ehdr)) != sizeof(Elf_Ehdr) ||
	    !__elf_is_okay__(&ehdr) ||
	    _fstat(fd, &st) < 0)
		return (-1);

	/* calculate section header table size */
	shdr_size = ehdr.e_shentsize * ehdr.e_shnum;

	/* Make sure it's not too big to mmap */
	if (shdr_size > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}

	/* mmap section header table */
	base = mmap(NULL, (size_t)shdr_size, PROT_READ, MAP_PRIVATE, fd,
	    (off_t)ehdr.e_shoff);
	if (base == MAP_FAILED)
		return (-1);
	shdr = (Elf_Shdr *)base;

	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 *
	 * XXX clearing anything other than n_type and n_value violates
	 * the semantics given in the man page.
	 */
	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}

	/*
	 * Find the symbol table entry and it's corresponding
	 * string table entry.	Version 1.1 of the ABI states
	 * that there is only one symbol table but that this
	 * could change in the future.
	 */
	for (i = 0; nent > 0 && i < ehdr.e_shnum; i++) {
		if (shdr[i].sh_type != SHT_SYMTAB &&
		    shdr[i].sh_type != SHT_DYNSYM)
			continue;
		symoff = shdr[i].sh_offset;
		symsize = shdr[i].sh_size;
		stroff = shdr[shdr[i].sh_link].sh_offset;
		strsize = shdr[shdr[i].sh_link].sh_size;

		/*
		 * Skip this section if it or its string table is empty or
		 * extends beyond the end of the file, or if the string
		 * table is too large to map into memory.
		 */
		if (symoff == 0 || symsize == 0 ||
		    symsize > SIZE_MAX - symoff ||
		    symoff + symsize > st.st_size ||
		    stroff == 0 || strsize == 0 ||
		    strsize > SIZE_MAX - stroff ||
		    stroff + strsize > st.st_size) {
			errno = ENOENT;
			continue;
		}

		/*
		 * Map string table into our address space.  This gives us
		 * an easy way to randomly access all the strings, without
		 * making the memory allocation permanent as with
		 * malloc/free (i.e., munmap will return it to the
		 * system).
		 */
		base = mmap(NULL, (size_t)strsize, PROT_READ,
		    MAP_PRIVATE, fd, (off_t)stroff);
		if (base == MAP_FAILED)
			continue;

		nent = elf_scan_symtab(shdr, ehdr.e_shnum, fd, symoff, symsize,
		    base, strsize, list, nent);

		errsave = errno;
		munmap(base, strsize);
		errno = errsave;
	}
	errsave = errno;
	munmap(shdr, shdr_size);
	errno = errsave;
	return (nent);
}

static int
elf_scan_symtab(Elf_Shdr *shdr, int shnum, int fd, off_t symoff, size_t symsize,
    char *strtab, size_t strsize, struct nlist *list, int nent)
{
	Elf_Sym sbuf[1024];
	Elf_Sym *s;
	char *name;
	struct nlist *p;
	Elf_Ssize cc;
	size_t slen;

	if (lseek(fd, symoff, SEEK_SET) == -1)
		return (-1);
	while (symsize > 0 && nent > 0) {
		cc = MIN(symsize, sizeof(sbuf));
		if (_read(fd, sbuf, cc) != cc)
			break;
		symsize -= cc;
		for (s = sbuf; cc > 0 && nent > 0; ++s, cc -= sizeof(*s)) {
			if (s->st_name >= strsize)
				continue;
			name = strtab + s->st_name;
			if (name[0] == '\0')
				continue;
			slen = strnlen(name, strsize - s->st_name);
			for (p = list; nent > 0 && !ISLAST(p); p++) {
				if (strncmp(name, p->n_un.n_name, slen) == 0 &&
				    p->n_un.n_name[slen] == '\0') {
					elf_sym_to_nlist(p, s, shdr, shnum);
					--nent;
				}
			}
		}
	}
	return (nent);
}

/*
 * Convert an Elf_Sym into an nlist structure.  This fills in only the
 * n_value and n_type members.
 */
static void
elf_sym_to_nlist(struct nlist *nl, Elf_Sym *s, Elf_Shdr *shdr, int shnum)
{
	nl->n_value = s->st_value;

	switch (s->st_shndx) {
	case SHN_UNDEF:
	case SHN_COMMON:
		nl->n_type = N_UNDF;
		break;
	case SHN_ABS:
		nl->n_type = ELF_ST_TYPE(s->st_info) == STT_FILE ?
		    N_FN : N_ABS;
		break;
	default:
		if (s->st_shndx >= shnum)
			nl->n_type = N_UNDF;
		else {
			Elf_Shdr *sh = shdr + s->st_shndx;

			nl->n_type = sh->sh_type == SHT_PROGBITS ?
			    (sh->sh_flags & SHF_WRITE ? N_DATA : N_TEXT) :
			    (sh->sh_type == SHT_NOBITS ? N_BSS : N_UNDF);
		}
		break;
	}

	if (ELF_ST_BIND(s->st_info) == STB_GLOBAL ||
	    ELF_ST_BIND(s->st_info) == STB_WEAK)
		nl->n_type |= N_EXT;
}
