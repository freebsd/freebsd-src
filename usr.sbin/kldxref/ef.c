/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000, Boris Popov
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

#define	MAXSEGS 16
struct ef_file {
	char		*ef_name;
	struct elf_file *ef_efile;
	GElf_Phdr	*ef_ph;
	void		*ef_fpage;		/* First block of the file */
	int		ef_fplen;		/* length of first block */
	GElf_Hashelt	ef_nbuckets;
	GElf_Hashelt	ef_nchains;
	GElf_Hashelt	*ef_buckets;
	GElf_Hashelt	*ef_chains;
	GElf_Hashelt	*ef_hashtab;
	caddr_t		ef_strtab;
	long		ef_strsz;
	GElf_Sym	*ef_symtab;
	int		ef_nsegs;
	GElf_Phdr	*ef_segs[MAXSEGS];
	int		ef_verbose;
	GElf_Rel	*ef_rel;		/* relocation table */
	long		ef_relsz;		/* number of entries */
	GElf_Rela	*ef_rela;		/* relocation table */
	long		ef_relasz;		/* number of entries */
};

static void	ef_print_phdr(GElf_Phdr *);
static GElf_Off	ef_get_offset(elf_file_t, GElf_Addr);

static void	ef_close(elf_file_t ef);

static int	ef_seg_read_rel(elf_file_t ef, GElf_Addr address, size_t len,
		    void *dest);
static int	ef_seg_read_string(elf_file_t ef, GElf_Addr address, size_t len,
		    char *dest);

static GElf_Addr ef_symaddr(elf_file_t ef, GElf_Size symidx);
static int	ef_lookup_set(elf_file_t ef, const char *name,
		    GElf_Addr *startp, GElf_Addr *stopp, long *countp);
static int	ef_lookup_symbol(elf_file_t ef, const char *name,
		    GElf_Sym **sym);

static struct elf_file_ops ef_file_ops = {
	.close			= ef_close,
	.seg_read_rel		= ef_seg_read_rel,
	.seg_read_string	= ef_seg_read_string,
	.symaddr		= ef_symaddr,
	.lookup_set		= ef_lookup_set,
};

static void
ef_print_phdr(GElf_Phdr *phdr)
{

	if ((phdr->p_flags & PF_W) == 0) {
		printf("text=0x%jx ", (uintmax_t)phdr->p_filesz);
	} else {
		printf("data=0x%jx", (uintmax_t)phdr->p_filesz);
		if (phdr->p_filesz < phdr->p_memsz)
			printf("+0x%jx",
			    (uintmax_t)(phdr->p_memsz - phdr->p_filesz));
		printf(" ");
	}
}

static GElf_Off
ef_get_offset(elf_file_t ef, GElf_Addr addr)
{
	GElf_Phdr *ph;
	int i;

	for (i = 0; i < ef->ef_nsegs; i++) {
		ph = ef->ef_segs[i];
		if (addr >= ph->p_vaddr && addr < ph->p_vaddr + ph->p_memsz) {
			return (ph->p_offset + (addr - ph->p_vaddr));
		}
	}
	return (0);
}

/*
 * next two functions copied from link_elf.c
 */
static int
ef_lookup_symbol(elf_file_t ef, const char *name, GElf_Sym **sym)
{
	unsigned long hash, symnum;
	GElf_Sym *symp;
	char *strp;

	/* First, search hashed global symbols */
	hash = elf_hash(name);
	symnum = ef->ef_buckets[hash % ef->ef_nbuckets];

	while (symnum != STN_UNDEF) {
		if (symnum >= ef->ef_nchains) {
			warnx("ef_lookup_symbol: file %s have corrupted symbol table\n",
			    ef->ef_name);
			return (ENOENT);
		}

		symp = ef->ef_symtab + symnum;
		if (symp->st_name == 0) {
			warnx("ef_lookup_symbol: file %s have corrupted symbol table\n",
			    ef->ef_name);
			return (ENOENT);
		}

		strp = ef->ef_strtab + symp->st_name;

		if (strcmp(name, strp) == 0) {
			if (symp->st_shndx != SHN_UNDEF ||
			    (symp->st_value != 0 &&
				GELF_ST_TYPE(symp->st_info) == STT_FUNC)) {
				*sym = symp;
				return (0);
			} else
				return (ENOENT);
		}

		symnum = ef->ef_chains[symnum];
	}

	return (ENOENT);
}

static int
ef_lookup_set(elf_file_t ef, const char *name, GElf_Addr *startp,
    GElf_Addr *stopp, long *countp)
{
	GElf_Sym *sym;
	char *setsym;
	int error, len;

	len = strlen(name) + sizeof("__start_set_"); /* sizeof includes \0 */
	setsym = malloc(len);
	if (setsym == NULL)
		return (errno);

	/* get address of first entry */
	snprintf(setsym, len, "%s%s", "__start_set_", name);
	error = ef_lookup_symbol(ef, setsym, &sym);
	if (error != 0)
		goto out;
	*startp = sym->st_value;

	/* get address of last entry */
	snprintf(setsym, len, "%s%s", "__stop_set_", name);
	error = ef_lookup_symbol(ef, setsym, &sym);
	if (error != 0)
		goto out;
	*stopp = sym->st_value;

	/* and the number of entries */
	*countp = (*stopp - *startp) / elf_pointer_size(ef->ef_efile);

out:
	free(setsym);
	return (error);
}

static GElf_Addr
ef_symaddr(elf_file_t ef, GElf_Size symidx)
{
	const GElf_Sym *sym;

	if (symidx >= ef->ef_nchains)
		return (0);
	sym = ef->ef_symtab + symidx;

	if (GELF_ST_BIND(sym->st_info) == STB_LOCAL &&
	    sym->st_shndx != SHN_UNDEF && sym->st_value != 0)
		return (sym->st_value);
	return (0);
}

static int
ef_parse_dynamic(elf_file_t ef, const GElf_Phdr *phdyn)
{
	GElf_Shdr *shdr;
	GElf_Dyn *dyn, *dp;
	size_t i, ndyn, nshdr, nsym;
	int error;
	GElf_Off hash_off, sym_off, str_off;
	GElf_Off rel_off;
	GElf_Off rela_off;
	int rel_sz;
	int rela_sz;
	int dynamic_idx;

	/*
	 * The kernel linker parses the PT_DYNAMIC segment to find
	 * various important tables.  The gelf API of libelf is
	 * section-oriented and requires extracting data from sections
	 * instead of segments (program headers).  As a result,
	 * iterate over section headers to read various tables after
	 * parsing values from PT_DYNAMIC.
	 */
	error = elf_read_shdrs(ef->ef_efile, &nshdr, &shdr);
	if (error != 0)
		return (EFTYPE);
	dyn = NULL;

	/* Find section for .dynamic. */
	dynamic_idx = -1;
	for (i = 0; i < nshdr; i++) {
		if (shdr[i].sh_type == SHT_DYNAMIC) {
			if (shdr[i].sh_offset != phdyn->p_offset ||
			    shdr[i].sh_size != phdyn->p_filesz) {
				warnx(".dynamic section doesn't match phdr");
				error = EFTYPE;
				goto out;
			}
			if (dynamic_idx != -1) {
				warnx("multiple SHT_DYNAMIC sections");
				error = EFTYPE;
				goto out;
			}
			dynamic_idx = i;
		}
	}

	error = elf_read_dynamic(ef->ef_efile, dynamic_idx, &ndyn, &dyn);
	if (error != 0)
		goto out;

	hash_off = rel_off = rela_off = sym_off = str_off = 0;
	rel_sz = rela_sz = 0;
	for (i = 0; i < ndyn; i++) {
		dp = &dyn[i];
		if (dp->d_tag == DT_NULL)
			break;

		switch (dp->d_tag) {
		case DT_HASH:
			if (hash_off != 0)
				warnx("second DT_HASH entry ignored");
			else
				hash_off = ef_get_offset(ef, dp->d_un.d_ptr);
			break;
		case DT_STRTAB:
			if (str_off != 0)
				warnx("second DT_STRTAB entry ignored");
			else
				str_off = ef_get_offset(ef, dp->d_un.d_ptr);
			break;
		case DT_SYMTAB:
			if (sym_off != 0)
				warnx("second DT_SYMTAB entry ignored");
			else
				sym_off = ef_get_offset(ef, dp->d_un.d_ptr);
			break;
		case DT_SYMENT:
			if (dp->d_un.d_val != elf_object_size(ef->ef_efile,
			    ELF_T_SYM)) {
				error = EFTYPE;
				goto out;
			}
			break;
		case DT_REL:
			if (rel_off != 0)
				warnx("second DT_REL entry ignored");
			else
				rel_off = ef_get_offset(ef, dp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			if (rel_sz != 0)
				warnx("second DT_RELSZ entry ignored");
			else
				rel_sz = dp->d_un.d_val;
			break;
		case DT_RELENT:
			if (dp->d_un.d_val != elf_object_size(ef->ef_efile,
			    ELF_T_REL)) {
				error = EFTYPE;
				goto out;
			}
			break;
		case DT_RELA:
			if (rela_off != 0)
				warnx("second DT_RELA entry ignored");
			else
				rela_off = ef_get_offset(ef, dp->d_un.d_ptr);
			break;
		case DT_RELASZ:
			if (rela_sz != 0)
				warnx("second DT_RELSZ entry ignored");
			else
				rela_sz = dp->d_un.d_val;
			break;
		case DT_RELAENT:
			if (dp->d_un.d_val != elf_object_size(ef->ef_efile,
			    ELF_T_RELA)) {
				error = EFTYPE;
				goto out;
			}
			break;
		}
	}
	if (hash_off == 0) {
		warnx("%s: no .hash section found\n", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (sym_off == 0) {
		warnx("%s: no .dynsym section found\n", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (str_off == 0) {
		warnx("%s: no .dynstr section found\n", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (rel_off == 0 && rela_off == 0) {
		warnx("%s: no ELF relocation table found\n", ef->ef_name);
		error = EFTYPE;
		goto out;
	}

	for (i = 0; i < nshdr; i++) {
		switch (shdr[i].sh_type) {
		case SHT_HASH:
			if (shdr[i].sh_offset != hash_off) {
				warnx("%s: ignoring SHT_HASH at different offset from DT_HASH",
				    ef->ef_name);
				break;
			}

			/*
			 * libelf(3) mentions ELF_T_HASH, but it is
			 * not defined.
			 */
			if (shdr[i].sh_size < sizeof(*ef->ef_hashtab) * 2) {
				warnx("hash section too small");
				error = EFTYPE;
				goto out;
			}
			error = elf_read_data(ef->ef_efile, ELF_T_WORD,
			    shdr[i].sh_offset, shdr[i].sh_size,
			    (void **)&ef->ef_hashtab);
			if (error != 0) {
				warnc(error, "can't read hash table");
				goto out;
			}
			ef->ef_nbuckets = ef->ef_hashtab[0];
			ef->ef_nchains = ef->ef_hashtab[1];
			if ((2 + ef->ef_nbuckets + ef->ef_nchains) *
			    sizeof(*ef->ef_hashtab) != shdr[i].sh_size) {
				warnx("inconsistent hash section size");
				error = EFTYPE;
				goto out;
			}

			ef->ef_buckets = ef->ef_hashtab + 2;
			ef->ef_chains = ef->ef_buckets + ef->ef_nbuckets;
			break;
		case SHT_DYNSYM:
			if (shdr[i].sh_offset != sym_off) {
				warnx("%s: ignoring SHT_DYNSYM at different offset from DT_SYMTAB",
				    ef->ef_name);
				break;
			}
			error = elf_read_symbols(ef->ef_efile, i, &nsym,
			    &ef->ef_symtab);
			if (error != 0) {
				if (ef->ef_verbose)
					warnx("%s: can't load .dynsym section (0x%jx)",
					    ef->ef_name, (uintmax_t)sym_off);
				goto out;
			}
			break;
		case SHT_STRTAB:
			if (shdr[i].sh_offset != str_off)
				break;
			error = elf_read_string_table(ef->ef_efile,
			    &shdr[i], &ef->ef_strsz, &ef->ef_strtab);
			if (error != 0) {
				warnx("can't load .dynstr section");
				error = EIO;
				goto out;
			}
			break;
		case SHT_REL:
			if (shdr[i].sh_offset != rel_off)
				break;
			if (shdr[i].sh_size != rel_sz) {
				warnx("%s: size mismatch for DT_REL section",
				    ef->ef_name);
				error = EFTYPE;
				goto out;
			}
			error = elf_read_rel(ef->ef_efile, i, &ef->ef_relsz,
			    &ef->ef_rel);
			if (error != 0) {
				warnx("%s: cannot load DT_REL section",
				    ef->ef_name);
				goto out;
			}
			break;
		case SHT_RELA:
			if (shdr[i].sh_offset != rela_off)
				break;
			if (shdr[i].sh_size != rela_sz) {
				warnx("%s: size mismatch for DT_RELA section",
				    ef->ef_name);
				error = EFTYPE;
				goto out;
			}
			error = elf_read_rela(ef->ef_efile, i, &ef->ef_relasz,
			    &ef->ef_rela);
			if (error != 0) {
				warnx("%s: cannot load DT_RELA section",
				    ef->ef_name);
				goto out;
			}
			break;
		}
	}

	if (ef->ef_hashtab == NULL) {
		warnx("%s: did not find a symbol hash table", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (ef->ef_symtab == NULL) {
		warnx("%s: did not find a dynamic symbol table", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (nsym != ef->ef_nchains) {
		warnx("%s: symbol count mismatch", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (ef->ef_strtab == NULL) {
		warnx("%s: did not find a dynamic string table", ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (rel_off != 0 && ef->ef_rel == NULL) {
		warnx("%s: did not find a DT_REL relocation table",
		    ef->ef_name);
		error = EFTYPE;
		goto out;
	}
	if (rela_off != 0 && ef->ef_rela == NULL) {
		warnx("%s: did not find a DT_RELA relocation table",
		    ef->ef_name);
		error = EFTYPE;
		goto out;
	}

	error = 0;
out:
	free(dyn);
	free(shdr);
	return (error);
}

static int
ef_seg_read_rel(elf_file_t ef, GElf_Addr address, size_t len, void *dest)
{
	GElf_Off ofs;
	const GElf_Rela *a;
	const GElf_Rel *r;
	int error;

	ofs = ef_get_offset(ef, address);
	if (ofs == 0) {
		if (ef->ef_verbose)
			warnx("ef_seg_read_rel(%s): bad address (%jx)",
			    ef->ef_name, (uintmax_t)address);
		return (EFAULT);
	}
	error = elf_read_raw_data(ef->ef_efile, ofs, dest, len);
	if (error != 0)
		return (error);

	for (r = ef->ef_rel; r < &ef->ef_rel[ef->ef_relsz]; r++) {
		error = elf_reloc(ef->ef_efile, r, ELF_T_REL, 0, address,
		    len, dest);
		if (error != 0)
			return (error);
	}
	for (a = ef->ef_rela; a < &ef->ef_rela[ef->ef_relasz]; a++) {
		error = elf_reloc(ef->ef_efile, a, ELF_T_RELA, 0, address,
		    len, dest);
		if (error != 0)
			return (error);
	}
	return (0);
}

static int
ef_seg_read_string(elf_file_t ef, GElf_Addr address, size_t len, char *dest)
{
	GElf_Off ofs;
	int error;

	ofs = ef_get_offset(ef, address);
	if (ofs == 0) {
		if (ef->ef_verbose)
			warnx("ef_seg_read_string(%s): bad offset (%jx:%ju)",
			    ef->ef_name, (uintmax_t)address, (uintmax_t)ofs);
		return (EFAULT);
	}

	error = elf_read_raw_data(ef->ef_efile, ofs, dest, len);
	if (error != 0)
		return (error);
	if (strnlen(dest, len) == len)
		return (EFAULT);

	return (0);
}

int
ef_open(struct elf_file *efile, int verbose)
{
	elf_file_t ef;
	GElf_Ehdr *hdr;
	size_t i, nphdr, nsegs;
	int error;
	GElf_Phdr *phdr, *phdyn;

	hdr = &efile->ef_hdr;
	if (hdr->e_phnum == 0 ||
	    hdr->e_phentsize != elf_object_size(efile, ELF_T_PHDR) ||
	    hdr->e_shnum == 0 || hdr->e_shoff == 0 ||
	    hdr->e_shentsize != elf_object_size(efile, ELF_T_SHDR))
		return (EFTYPE);

	ef = malloc(sizeof(*ef));
	if (ef == NULL)
		return (errno);

	efile->ef_ef = ef;
	efile->ef_ops = &ef_file_ops;

	bzero(ef, sizeof(*ef));
	ef->ef_verbose = verbose;
	ef->ef_name = strdup(efile->ef_filename);
	ef->ef_efile = efile;

	error = elf_read_phdrs(efile, &nphdr, &ef->ef_ph);
	if (error != 0) {
		phdr = NULL;
		goto out;
	}

	error = EFTYPE;
	nsegs = 0;
	phdyn = NULL;
	phdr = ef->ef_ph;
	for (i = 0; i < nphdr; i++, phdr++) {
		if (verbose > 1)
			ef_print_phdr(phdr);
		switch (phdr->p_type) {
		case PT_LOAD:
			if (nsegs < MAXSEGS)
				ef->ef_segs[nsegs] = phdr;
			nsegs++;
			break;
		case PT_PHDR:
			break;
		case PT_DYNAMIC:
			phdyn = phdr;
			break;
		}
	}
	if (verbose > 1)
		printf("\n");
	if (phdyn == NULL) {
		warnx("Skipping %s: not dynamically-linked",
		    ef->ef_name);
		goto out;
	}

	if (nsegs > MAXSEGS) {
		warnx("%s: too many segments", ef->ef_name);
		goto out;
	}
	ef->ef_nsegs = nsegs;

	error = ef_parse_dynamic(ef, phdyn);
out:
	if (error != 0)
		ef_close(ef);
	return (error);
}

static void
ef_close(elf_file_t ef)
{
	free(ef->ef_rela);
	free(ef->ef_rel);
	free(ef->ef_strtab);
	free(ef->ef_symtab);
	free(ef->ef_hashtab);
	free(ef->ef_ph);
	if (ef->ef_name)
		free(ef->ef_name);
	ef->ef_efile->ef_ops = NULL;
	ef->ef_efile->ef_ef = NULL;
	free(ef);
}
