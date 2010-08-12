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
#include <sys/endian.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#if (defined(NLIST_ELF32) && (ELFSIZE == 32)) || \
    (defined(NLIST_ELF64) && (ELFSIZE == 64))

#define	__ELF_WORD_SIZE ELFSIZE
#if (ELFSIZE == 32)
#include <sys/elf32.h>
#define	xewtoh(x)	((data == ELFDATA2MSB) ? be32toh(x) : le32toh(x))
#define	htoxew(x)	((data == ELFDATA2MSB) ? htobe32(x) : htole32(x))
#define	wewtoh(x)	((data == ELFDATA2MSB) ? be32toh(x) : le32toh(x))
#define	htowew(x)	((data == ELFDATA2MSB) ? htobe32(x) : htole32(x))
#elif (ELFSIZE == 64)
#include <sys/elf64.h>
#define	xewtoh(x)	((data == ELFDATA2MSB) ? be64toh(x) : le64toh(x))
#define	htoxew(x)	((data == ELFDATA2MSB) ? htobe64(x) : htole64(x))
/* elf64 Elf64_Word are 32 bits */
#define	wewtoh(x)	((data == ELFDATA2MSB) ? be32toh(x) : le32toh(x))
#define	htowew(x)	((data == ELFDATA2MSB) ? htobe32(x) : htole32(x))
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

static void *
xrealloc(void *ptr, size_t size, const char *fn, const char *use)
{
	void *rv;
		
	rv = realloc(ptr, size);
	if (rv == NULL) {
		free(ptr);
		fprintf(stderr, "%s: out of memory (reallocating for %s)\n",
		    fn, use);
	}
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
#ifndef EM_ARM
#define EM_ARM		40
#endif
	case EM_ARM: break;
#ifndef EM_MIPS
#define EM_MIPS		8
#endif
#ifndef EM_MIPS_RS4_BE		/* same as EM_MIPS_RS3_LE */
#define EM_MIPS_RS4_BE	10
#endif
	case EM_MIPS: break;
	case /* EM_MIPS_RS3_LE */ EM_MIPS_RS4_BE: break;
#ifndef EM_IA_64
#define	EM_IA_64	50
#endif
	case EM_IA_64: break;
#ifndef EM_PPC
#define	EM_PPC		20
#endif
	case EM_PPC: break;
#ifndef EM_PPC64
#define	EM_PPC64	21
#endif
	case EM_PPC64: break;
#ifndef EM_SPARCV9
#define	EM_SPARCV9	43
#endif
	case EM_SPARCV9: break;
#ifndef EM_X86_64
#define	EM_X86_64	62
#endif
	case EM_X86_64: break;
/*        ELFDEFNNAME(MACHDEP_ID_CASES) */

        default:
                return 0;
        }

	return 1;
}

/*
 * This function 'hides' (some of) ELF executable file's symbols.
 * It hides them by renaming them to "_$$hide$$ <filename> <symbolname>".
 * Symbols in the global keep list, or which are marked as being undefined,
 * are left alone.
 *
 * An old version of this code shuffled various tables around, turning
 * global symbols to be hidden into local symbols.  That lost on the
 * mips, because CALL16 relocs must reference global symbols, and, if
 * those symbols were being hidden, they were no longer global.
 *
 * The new renaming behaviour doesn't take global symbols out of the
 * namespace.  However, it's ... unlikely that there will ever be
 * any collisions in practice because of the new method.
 */
int
ELFNAMEEND(hide)(int fd, const char *fn)
{
	Elf_Ehdr ehdr;
	Elf_Shdr *shdrp = NULL, *symtabshdr, *strtabshdr;
	Elf_Sym *symtabp = NULL;
	char *strtabp = NULL;
	Elf_Size  nsyms, nlocalsyms, ewi;
	ssize_t shdrsize;
	int rv, i, weird;
	size_t nstrtab_size, nstrtab_nextoff, fn_size;
	char *nstrtabp = NULL;
	unsigned char data;
	Elf_Off maxoff, stroff;
	const char *weirdreason = NULL;

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
	maxoff = stroff = 0;
	for (i = 0; i < xe16toh(ehdr.e_shnum); i++) {
		if (xewtoh(shdrp[i].sh_offset) > maxoff)
			maxoff = xewtoh(shdrp[i].sh_offset);
		switch (xe32toh(shdrp[i].sh_type)) {
		case SHT_SYMTAB:
			if (symtabshdr != NULL)
				weird = 1;
			symtabshdr = &shdrp[i];
			strtabshdr = &shdrp[xe32toh(shdrp[i].sh_link)];

			/* Check whether the string table is the last section */
			stroff = xewtoh(shdrp[xe32toh(shdrp[i].sh_link)].sh_offset);
			if (!weird && xe32toh(shdrp[i].sh_link) != (xe16toh(ehdr.e_shnum) - 1)) {
				weird = 1;
				weirdreason = "string table not last section";
			}
			break;
		}
	}
	if (! weirdreason)
		weirdreason = "unsupported";
	if (symtabshdr == NULL)
		goto out;
	if (strtabshdr == NULL)
		weird = 1;
	if (!weird && stroff != maxoff) {
		weird = 1;
		weirdreason = "string table section not last in file";
	}   
	if (weird) {
		fprintf(stderr, "%s: weird executable (%s)\n", fn, weirdreason);
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

	nstrtab_size = 256;
	nstrtabp = xmalloc(nstrtab_size, fn, "new string table");
	if (nstrtabp == NULL)
		goto bad;
	nstrtab_nextoff = 0;

	fn_size = strlen(fn);

	/* Prepare data structures for symbol movement. */
	nsyms = xewtoh(symtabshdr->sh_size) / xewtoh(symtabshdr->sh_entsize);
	nlocalsyms = xe32toh(symtabshdr->sh_info);

	/* move symbols, making them local */
	for (ewi = 0; ewi < nsyms; ewi++) {
		Elf_Sym *sp = &symtabp[ewi];
		const char *symname = strtabp + xe32toh(sp->st_name);
		size_t newent_len;
		/*
		 * make sure there's size for the next entry, even if it's
		 * as large as it can be.
		 *
		 * "_$$hide$$ <filename> <symname><NUL>" ->
		 *    9 + 3 + sizes of fn and sym name
		 */
		while ((nstrtab_size - nstrtab_nextoff) <
		    strlen(symname) + fn_size + 12) {
			nstrtab_size *= 2;
			nstrtabp = xrealloc(nstrtabp, nstrtab_size, fn,
			    "new string table");
			if (nstrtabp == NULL)
				goto bad;
		}

		sp->st_name = htowew(nstrtab_nextoff);

		/* if it's a keeper or is undefined, don't rename it. */
		if (in_keep_list(symname) ||
		    (xe16toh(sp->st_shndx) == SHN_UNDEF)) {
			newent_len = sprintf(nstrtabp + nstrtab_nextoff,
			    "%s", symname) + 1;
		} else {
			newent_len = sprintf(nstrtabp + nstrtab_nextoff,
			    "_$$hide$$ %s %s", fn, symname) + 1;
		}
		nstrtab_nextoff += newent_len;
	}
	strtabshdr->sh_size = htoxew(nstrtab_nextoff);

	/*
	 * write new tables to the file
	 */
	if (xwriteatoff(fd, shdrp, xewtoh(ehdr.e_shoff), shdrsize, fn) !=
	    shdrsize)
		goto bad;
	if (xwriteatoff(fd, symtabp, xewtoh(symtabshdr->sh_offset),
	    xewtoh(symtabshdr->sh_size), fn) != xewtoh(symtabshdr->sh_size))
		goto bad;
	/* write new symbol table strings */
	if ((size_t)xwriteatoff(fd, nstrtabp, xewtoh(strtabshdr->sh_offset),
	    xewtoh(strtabshdr->sh_size), fn) != xewtoh(strtabshdr->sh_size))
		goto bad;

out:
	if (shdrp != NULL)
		free(shdrp);
	if (symtabp != NULL)
		free(symtabp);
	if (strtabp != NULL)
		free(strtabp);
	return (rv);

bad:
	rv = 1;
	goto out;
}

#endif /* include this size of ELF */
