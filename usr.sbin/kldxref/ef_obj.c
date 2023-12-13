/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000, Boris Popov
 * Copyright (c) 1998-2000 Doug Rabson
 * Copyright (c) 2004 Peter Wemm
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ef.h"

typedef struct {
	char		*addr;
	GElf_Off	size;
	int		flags;
	int		sec;	/* Original section */
	char		*name;
} Elf_progent;

typedef struct {
	GElf_Rel	*rel;
	long		nrel;
	int		sec;
} Elf_relent;

typedef struct {
	GElf_Rela	*rela;
	long		nrela;
	int		sec;
} Elf_relaent;

struct ef_file {
	char		*ef_name;
	struct elf_file *ef_efile;

	char		*address;
	GElf_Off	size;

	Elf_progent	*progtab;
	int		nprogtab;

	Elf_relaent	*relatab;
	int		nrela;

	Elf_relent	*reltab;
	int		nrel;

	GElf_Sym	*ddbsymtab;	/* The symbol table we are using */
	size_t		ddbsymcnt;	/* Number of symbols */
	caddr_t		ddbstrtab;	/* String table */
	long		ddbstrcnt;	/* number of bytes in string table */

	caddr_t		shstrtab;	/* Section name string table */
	long		shstrcnt;	/* number of bytes in string table */

	int		ef_verbose;
};

static void	ef_obj_close(elf_file_t ef);

static int	ef_obj_seg_read_rel(elf_file_t ef, GElf_Addr address,
		    size_t len, void *dest);
static int	ef_obj_seg_read_string(elf_file_t ef, GElf_Addr address,
		    size_t len, char *dest);

static GElf_Addr ef_obj_symaddr(elf_file_t ef, GElf_Size symidx);
static int	ef_obj_lookup_set(elf_file_t ef, const char *name,
		    GElf_Addr *startp, GElf_Addr *stopp, long *countp);
static int	ef_obj_lookup_symbol(elf_file_t ef, const char *name,
		    GElf_Sym **sym);

static struct elf_file_ops ef_obj_file_ops = {
	.close			= ef_obj_close,
	.seg_read_rel		= ef_obj_seg_read_rel,
	.seg_read_string	= ef_obj_seg_read_string,
	.symaddr		= ef_obj_symaddr,
	.lookup_set		= ef_obj_lookup_set,
};

static int
ef_obj_lookup_symbol(elf_file_t ef, const char *name, GElf_Sym **sym)
{
	GElf_Sym *symp;
	const char *strp;
	int i;

	for (i = 0, symp = ef->ddbsymtab; i < ef->ddbsymcnt; i++, symp++) {
		strp = ef->ddbstrtab + symp->st_name;
		if (symp->st_shndx != SHN_UNDEF && strcmp(name, strp) == 0) {
			*sym = symp;
			return (0);
		}
	}
	return (ENOENT);
}

static int
ef_obj_lookup_set(elf_file_t ef, const char *name, GElf_Addr *startp,
    GElf_Addr *stopp, long *countp)
{
	int i;

	for (i = 0; i < ef->nprogtab; i++) {
		if ((strncmp(ef->progtab[i].name, "set_", 4) == 0) &&
		    strcmp(ef->progtab[i].name + 4, name) == 0) {
			*startp = ef->progtab[i].addr - ef->address;
			*stopp = ef->progtab[i].addr + ef->progtab[i].size -
			    ef->address;
			*countp = (*stopp - *startp) /
			    elf_pointer_size(ef->ef_efile);
			return (0);
		}
	}
	return (ESRCH);
}

static GElf_Addr
ef_obj_symaddr(elf_file_t ef, GElf_Size symidx)
{
	const GElf_Sym *sym;

	if (symidx >= ef->ddbsymcnt)
		return (0);
	sym = ef->ddbsymtab + symidx;

	if (sym->st_shndx != SHN_UNDEF)
		return (sym->st_value - (GElf_Addr)ef->address);
	return (0);
}

static int
ef_obj_seg_read_rel(elf_file_t ef, GElf_Addr address, size_t len, void *dest)
{
	char *memaddr;
	GElf_Rel *r;
	GElf_Rela *a;
	GElf_Addr secbase, dataoff;
	int error, i, sec;

	if (address + len > ef->size) {
		if (ef->ef_verbose)
			warnx("ef_obj_seg_read_rel(%s): bad offset/len (%lx:%ld)",
			    ef->ef_name, (long)address, (long)len);
		return (EFAULT);
	}
	bcopy(ef->address + address, dest, len);

	/* Find out which section contains the data. */
	memaddr = ef->address + address;
	sec = -1;
	secbase = dataoff = 0;
	for (i = 0; i < ef->nprogtab; i++) {
		if (ef->progtab[i].addr == NULL)
			continue;
		if (memaddr < (char *)ef->progtab[i].addr || memaddr + len >
		     (char *)ef->progtab[i].addr + ef->progtab[i].size)
			continue;
		sec = ef->progtab[i].sec;
		/* We relocate to address 0. */
		secbase = (char *)ef->progtab[i].addr - ef->address;
		dataoff = memaddr - ef->address;
		break;
	}

	if (sec == -1)
		return (EFAULT);

	/* Now do the relocations. */
	for (i = 0; i < ef->nrel; i++) {
		if (ef->reltab[i].sec != sec)
			continue;
		for (r = ef->reltab[i].rel;
		     r < &ef->reltab[i].rel[ef->reltab[i].nrel]; r++) {
			error = elf_reloc(ef->ef_efile, r, ELF_T_REL, secbase,
			    dataoff, len, dest);
			if (error != 0)
				return (error);
		}
	}
	for (i = 0; i < ef->nrela; i++) {
		if (ef->relatab[i].sec != sec)
			continue;
		for (a = ef->relatab[i].rela;
		     a < &ef->relatab[i].rela[ef->relatab[i].nrela]; a++) {
			error = elf_reloc(ef->ef_efile, a, ELF_T_RELA, secbase,
			    dataoff, len, dest);
			if (error != 0)
				return (error);
		}
	}
	return (0);
}

static int
ef_obj_seg_read_string(elf_file_t ef, GElf_Addr address, size_t len, char *dest)
{

	if (address >= ef->size) {
		if (ef->ef_verbose)
			warnx("ef_obj_seg_read_string(%s): bad address (%lx)",
			    ef->ef_name, (long)address);
		return (EFAULT);
	}

	if (ef->size - address < len)
		len = ef->size - address;

	if (strnlen(ef->address + address, len) == len)
		return (EFAULT);

	memcpy(dest, ef->address + address, len);
	return (0);
}

int
ef_obj_open(struct elf_file *efile, int verbose)
{
	elf_file_t ef;
	GElf_Ehdr *hdr;
	GElf_Shdr *shdr;
	GElf_Sym *es;
	char *mapbase;
	size_t i, mapsize, alignmask, max_addralign, nshdr;
	int error, pb, ra, rl;
	int j, nsym, symstrindex, symtabindex;

	hdr = &efile->ef_hdr;
	if (hdr->e_type != ET_REL || hdr->e_shnum == 0 || hdr->e_shoff == 0 ||
	    hdr->e_shentsize != elf_object_size(efile, ELF_T_SHDR))
		return (EFTYPE);

	ef = calloc(1, sizeof(*ef));
	if (ef == NULL)
		return (errno);

	efile->ef_ef = ef;
	efile->ef_ops = &ef_obj_file_ops;

	ef->ef_verbose = verbose;
	ef->ef_name = strdup(efile->ef_filename);
	ef->ef_efile = efile;

	error = elf_read_shdrs(efile, &nshdr, &shdr);
	if (error != 0) {
		shdr = NULL;
		goto out;
	}

	/* Scan the section headers for information and table sizing. */
	nsym = 0;
	symtabindex = -1;
	symstrindex = -1;
	for (i = 0; i < nshdr; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			ef->nprogtab++;
			break;
		case SHT_SYMTAB:
			nsym++;
			symtabindex = i;
			symstrindex = shdr[i].sh_link;
			break;
		case SHT_REL:
			ef->nrel++;
			break;
		case SHT_RELA:
			ef->nrela++;
			break;
		case SHT_STRTAB:
			break;
		}
	}

	if (ef->nprogtab == 0) {
		warnx("%s: file has no contents", ef->ef_name);
		goto out;
	}
	if (nsym != 1) {
		warnx("%s: file has no valid symbol table", ef->ef_name);
		goto out;
	}
	if (symstrindex < 0 || symstrindex > nshdr ||
	    shdr[symstrindex].sh_type != SHT_STRTAB) {
		warnx("%s: file has invalid symbol strings", ef->ef_name);
		goto out;
	}

	/* Allocate space for tracking the load chunks */
	if (ef->nprogtab != 0)
		ef->progtab = calloc(ef->nprogtab, sizeof(*ef->progtab));
	if (ef->nrel != 0)
		ef->reltab = calloc(ef->nrel, sizeof(*ef->reltab));
	if (ef->nrela != 0)
		ef->relatab = calloc(ef->nrela, sizeof(*ef->relatab));
	if ((ef->nprogtab != 0 && ef->progtab == NULL) ||
	    (ef->nrel != 0 && ef->reltab == NULL) ||
	    (ef->nrela != 0 && ef->relatab == NULL)) {
		printf("malloc failed\n");
		error = ENOMEM;
		goto out;
	}

	if (elf_read_symbols(efile, symtabindex, &ef->ddbsymcnt,
	    &ef->ddbsymtab) != 0) {
		printf("elf_read_symbols failed\n");
		goto out;
	}

	if (elf_read_string_table(efile, &shdr[symstrindex], &ef->ddbstrcnt,
	    &ef->ddbstrtab) != 0) {
		printf("elf_read_string_table failed\n");
		goto out;
	}

	/* Do we have a string table for the section names?  */
	if (hdr->e_shstrndx != 0 &&
	    shdr[hdr->e_shstrndx].sh_type == SHT_STRTAB) {
		if (elf_read_string_table(efile, &shdr[hdr->e_shstrndx],
		    &ef->shstrcnt, &ef->shstrtab) != 0) {
			printf("elf_read_string_table failed\n");
			goto out;
		}
	}

	/* Size up code/data(progbits) and bss(nobits). */
	alignmask = 0;
	max_addralign = 0;
	mapsize = 0;
	for (i = 0; i < nshdr; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			alignmask = shdr[i].sh_addralign - 1;
			if (shdr[i].sh_addralign > max_addralign)
				max_addralign = shdr[i].sh_addralign;
			mapsize += alignmask;
			mapsize &= ~alignmask;
			mapsize += shdr[i].sh_size;
			break;
		}
	}

	/* We know how much space we need for the text/data/bss/etc. */
	ef->size = mapsize;
	if (posix_memalign((void **)&ef->address, max_addralign, mapsize)) {
		printf("posix_memalign failed\n");
		goto out;
	}
	mapbase = ef->address;

	/*
	 * Now load code/data(progbits), zero bss(nobits), allocate
	 * space for and load relocs
	 */
	pb = 0;
	rl = 0;
	ra = 0;
	alignmask = 0;
	for (i = 0; i < nshdr; i++) {
		switch (shdr[i].sh_type) {
		case SHT_PROGBITS:
		case SHT_NOBITS:
			alignmask = shdr[i].sh_addralign - 1;
			mapbase += alignmask;
			mapbase = (char *)((uintptr_t)mapbase & ~alignmask);
			ef->progtab[pb].addr = (void *)(uintptr_t)mapbase;
			if (shdr[i].sh_type == SHT_PROGBITS) {
				ef->progtab[pb].name = "<<PROGBITS>>";
				if (elf_read_raw_data(efile,
				    shdr[i].sh_offset, ef->progtab[pb].addr,
				    shdr[i].sh_size) != 0) {
					printf("failed to read progbits\n");
					goto out;
				}
			} else {
				ef->progtab[pb].name = "<<NOBITS>>";
				bzero(ef->progtab[pb].addr, shdr[i].sh_size);
			}
			ef->progtab[pb].size = shdr[i].sh_size;
			ef->progtab[pb].sec = i;
			if (ef->shstrtab && shdr[i].sh_name != 0)
				ef->progtab[pb].name =
				    ef->shstrtab + shdr[i].sh_name;

			/* Update all symbol values with the offset. */
			for (j = 0; j < ef->ddbsymcnt; j++) {
				es = &ef->ddbsymtab[j];
				if (es->st_shndx != i)
					continue;
				es->st_value += (GElf_Addr)ef->progtab[pb].addr;
			}
			mapbase += shdr[i].sh_size;
			pb++;
			break;
		case SHT_REL:
			ef->reltab[rl].sec = shdr[i].sh_info;
			if (elf_read_rel(efile, i, &ef->reltab[rl].nrel,
			    &ef->reltab[rl].rel) != 0) {
				printf("elf_read_rel failed\n");
				goto out;
			}
			rl++;
			break;
		case SHT_RELA:
			ef->relatab[ra].sec = shdr[i].sh_info;
			if (elf_read_rela(efile, i, &ef->relatab[ra].nrela,
			    &ef->relatab[ra].rela) != 0) {
				printf("elf_read_rela failed\n");
				goto out;
			}
			ra++;
			break;
		}
	}
	error = 0;
out:
	free(shdr);
	if (error != 0)
		ef_obj_close(ef);
	return (error);
}

static void
ef_obj_close(elf_file_t ef)
{
	int i;

	if (ef->ef_name)
		free(ef->ef_name);
	if (ef->size != 0)
		free(ef->address);
	if (ef->nprogtab != 0)
		free(ef->progtab);
	if (ef->nrel != 0) {
		for (i = 0; i < ef->nrel; i++)
			if (ef->reltab[i].rel != NULL)
				free(ef->reltab[i].rel);
		free(ef->reltab);
	}
	if (ef->nrela != 0) {
		for (i = 0; i < ef->nrela; i++)
			if (ef->relatab[i].rela != NULL)
				free(ef->relatab[i].rela);
		free(ef->relatab);
	}
	if (ef->ddbsymtab != NULL)
		free(ef->ddbsymtab);
	if (ef->ddbstrtab != NULL)
		free(ef->ddbstrtab);
	if (ef->shstrtab != NULL)
		free(ef->shstrtab);
	ef->ef_efile->ef_ops = NULL;
	ef->ef_efile->ef_ef = NULL;
	free(ef);
}
