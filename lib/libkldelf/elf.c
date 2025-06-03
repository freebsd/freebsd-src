/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 John Baldwin <jhb@FreeBSD.org>
 *
 * This software was developed by SRI International and the University
 * of Cambridge Computer Laboratory (Department of Computer Science
 * and Technology) under Defense Advanced Research Projects Agency
 * (DARPA) contract HR0011-18-C-0016 ("ECATS"), as part of the DARPA
 * SSITH research programme and under DARPA Contract No. HR001123C0031
 * ("MTSS").
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
 */

#include <sys/param.h>
#include <sys/endian.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kldelf.h"

SET_DECLARE(elf_reloc, struct elf_reloc_data);

static elf_reloc_t *
elf_find_reloc(const GElf_Ehdr *hdr)
{
	struct elf_reloc_data **erd;

	SET_FOREACH(erd, elf_reloc) {
		if (hdr->e_ident[EI_CLASS] == (*erd)->class &&
		    hdr->e_ident[EI_DATA] == (*erd)->data &&
		    hdr->e_machine == (*erd)->machine)
			return ((*erd)->reloc);
	}
	return (NULL);
}

int
elf_open_file(struct elf_file *efile, const char *filename, int verbose)
{
	int error;

	memset(efile, 0, sizeof(*efile));
	efile->ef_filename = filename;
	efile->ef_fd = open(filename, O_RDONLY);
	if (efile->ef_fd == -1) {
		if (verbose)
			warn("open(%s)", filename);
		return (errno);
	}

	efile->ef_elf = elf_begin(efile->ef_fd, ELF_C_READ, NULL);
	if (efile->ef_elf == NULL) {
		if (verbose)
			warnx("elf_begin(%s): %s", filename, elf_errmsg(0));
		elf_close_file(efile);
		return (EINVAL);
	}

	if (elf_kind(efile->ef_elf) != ELF_K_ELF) {
		if (verbose)
			warnx("%s: not an ELF file", filename);
		elf_close_file(efile);
		return (EINVAL);
	}

	if (gelf_getehdr(efile->ef_elf, &efile->ef_hdr) == NULL) {
		if (verbose)
			warnx("gelf_getehdr(%s): %s", filename, elf_errmsg(0));
		elf_close_file(efile);
		return (EINVAL);
	}

	efile->ef_reloc = elf_find_reloc(&efile->ef_hdr);
	if (efile->ef_reloc == NULL) {
		if (verbose)
			warnx("%s: unsupported architecture", filename);
		elf_close_file(efile);
		return (EFTYPE);
	}

	error = ef_open(efile, verbose);
	if (error != 0) {
		error = ef_obj_open(efile, verbose);
		if (error != 0) {
			if (verbose)
				warnc(error, "%s: not a valid DSO or object file",
				    filename);
			elf_close_file(efile);
			return (error);
		}
	}

	efile->ef_pointer_size = elf_object_size(efile, ELF_T_ADDR);

	return (0);
}

void
elf_close_file(struct elf_file *efile)
{
	if (efile->ef_ops != NULL) {
		EF_CLOSE(efile);
	}
	if (efile->ef_elf != NULL) {
		elf_end(efile->ef_elf);
		efile->ef_elf = NULL;
	}
	if (efile->ef_fd > 0) {
		close(efile->ef_fd);
		efile->ef_fd = -1;
	}
}

bool
elf_compatible(struct elf_file *efile, const GElf_Ehdr *hdr)
{
	if (efile->ef_hdr.e_ident[EI_CLASS] != hdr->e_ident[EI_CLASS] ||
	    efile->ef_hdr.e_ident[EI_DATA] != hdr->e_ident[EI_DATA] ||
	    efile->ef_hdr.e_machine != hdr->e_machine)
		return (false);
	return (true);
}

size_t
elf_object_size(struct elf_file *efile, Elf_Type type)
{
	return (gelf_fsize(efile->ef_elf, type, 1, efile->ef_hdr.e_version));
}

/*
 * The number of objects of 'type' in region of the file of size
 * 'file_size'.
 */
static size_t
elf_object_count(struct elf_file *efile, Elf_Type type, size_t file_size)
{
	return (file_size / elf_object_size(efile, type));
}

int
elf_read_raw_data(struct elf_file *efile, off_t offset, void *dst, size_t len)
{
	ssize_t nread;

	nread = pread(efile->ef_fd, dst, len, offset);
	if (nread == -1)
		return (errno);
	if (nread != len)
		return (EIO);
	return (0);
}

int
elf_read_raw_data_alloc(struct elf_file *efile, off_t offset, size_t len,
    void **out)
{
	void *buf;
	int error;

	buf = malloc(len);
	if (buf == NULL)
		return (ENOMEM);
	error = elf_read_raw_data(efile, offset, buf, len);
	if (error != 0) {
		free(buf);
		return (error);
	}
	*out = buf;
	return (0);
}

int
elf_read_raw_string(struct elf_file *efile, off_t offset, char *dst, size_t len)
{
	ssize_t nread;

	nread = pread(efile->ef_fd, dst, len, offset);
	if (nread == -1)
		return (errno);
	if (nread == 0)
		return (EIO);

	/* A short read is ok so long as the data contains a terminator. */
	if (strnlen(dst, nread) == nread)
		return (EFAULT);

	return (0);
}

int
elf_read_data(struct elf_file *efile, Elf_Type type, off_t offset, size_t len,
    void **out)
{
	Elf_Data dst, src;
	void *buf;
	int error;

	buf = malloc(len);
	if (buf == NULL)
		return (ENOMEM);

	error = elf_read_raw_data(efile, offset, buf, len);
	if (error != 0) {
		free(buf);
		return (error);
	}

	memset(&dst, 0, sizeof(dst));
	memset(&src, 0, sizeof(src));

	src.d_buf = buf;
	src.d_size = len;
	src.d_type = type;
	src.d_version = efile->ef_hdr.e_version;

	dst.d_buf = buf;
	dst.d_size = len;
	dst.d_version = EV_CURRENT;

	if (gelf_xlatetom(efile->ef_elf, &dst, &src, elf_encoding(efile)) ==
	    NULL) {
		free(buf);
		return (ENXIO);
	}

	if (dst.d_size != len)
		warnx("elf_read_data: translation of type %u size mismatch",
		    type);

	*out = buf;
	return (0);
}

int
elf_read_relocated_data(struct elf_file *efile, GElf_Addr address, size_t len,
    void **buf)
{
	int error;
	void *p;

	p = malloc(len);
	if (p == NULL)
		return (ENOMEM);
	error = EF_SEG_READ_REL(efile, address, len, p);
	if (error != 0) {
		free(p);
		return (error);
	}
	*buf = p;
	return (0);
}

int
elf_read_phdrs(struct elf_file *efile, size_t *nphdrp, GElf_Phdr **phdrp)
{
	GElf_Phdr *phdr;
	size_t nphdr, i;
	int error;

	if (elf_getphdrnum(efile->ef_elf, &nphdr) == -1)
		return (EFTYPE);

	phdr = calloc(nphdr, sizeof(*phdr));
	if (phdr == NULL)
		return (ENOMEM);

	for (i = 0; i < nphdr; i++) {
		if (gelf_getphdr(efile->ef_elf, i, &phdr[i]) == NULL) {
			error = EFTYPE;
			goto out;
		}
	}

	*nphdrp = nphdr;
	*phdrp = phdr;
	return (0);
out:
	free(phdr);
	return (error);
}

int
elf_read_shdrs(struct elf_file *efile, size_t *nshdrp, GElf_Shdr **shdrp)
{
	GElf_Shdr *shdr;
	Elf_Scn *scn;
	size_t nshdr, i;
	int error;

	if (elf_getshdrnum(efile->ef_elf, &nshdr) == -1)
		return (EFTYPE);

	shdr = calloc(nshdr, sizeof(*shdr));
	if (shdr == NULL)
		return (ENOMEM);

	for (i = 0; i < nshdr; i++) {
		scn = elf_getscn(efile->ef_elf, i);
		if (scn == NULL) {
			error = EFTYPE;
			goto out;
		}
		if (gelf_getshdr(scn, &shdr[i]) == NULL) {
			error = EFTYPE;
			goto out;
		}
	}

	*nshdrp = nshdr;
	*shdrp = shdr;
	return (0);
out:
	free(shdr);
	return (error);
}

int
elf_read_dynamic(struct elf_file *efile, int section_index, size_t *ndynp,
    GElf_Dyn **dynp)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	Elf_Data *data;
	GElf_Dyn *dyn;
	long i, ndyn;

	scn = elf_getscn(efile->ef_elf, section_index);
	if (scn == NULL)
		return (EINVAL);
	if (gelf_getshdr(scn, &shdr) == NULL)
		return (EINVAL);
	data = elf_getdata(scn, NULL);
	if (data == NULL)
		return (EINVAL);

	ndyn = elf_object_count(efile, ELF_T_DYN, shdr.sh_size);
	dyn = calloc(ndyn, sizeof(*dyn));
	if (dyn == NULL)
		return (ENOMEM);

	for (i = 0; i < ndyn; i++) {
		if (gelf_getdyn(data, i, &dyn[i]) == NULL) {
			free(dyn);
			return (EINVAL);
		}
	}

	*ndynp = ndyn;
	*dynp = dyn;
	return (0);
}

int
elf_read_symbols(struct elf_file *efile, int section_index, size_t *nsymp,
    GElf_Sym **symp)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	Elf_Data *data;
	GElf_Sym *sym;
	size_t i, nsym;

	scn = elf_getscn(efile->ef_elf, section_index);
	if (scn == NULL)
		return (EINVAL);
	if (gelf_getshdr(scn, &shdr) == NULL)
		return (EINVAL);
	data = elf_getdata(scn, NULL);
	if (data == NULL)
		return (EINVAL);

	nsym = elf_object_count(efile, ELF_T_SYM, shdr.sh_size);
	sym = calloc(nsym, sizeof(*sym));
	if (sym == NULL)
		return (ENOMEM);

	for (i = 0; i < nsym; i++) {
		if (gelf_getsym(data, i, &sym[i]) == NULL) {
			free(sym);
			return (EINVAL);
		}
	}

	*nsymp = nsym;
	*symp = sym;
	return (0);
}

int
elf_read_string_table(struct elf_file *efile, const GElf_Shdr *shdr,
    long *strcnt, char **strtab)
{
	int error;

	if (shdr->sh_type != SHT_STRTAB)
		return (EINVAL);
	error = elf_read_raw_data_alloc(efile, shdr->sh_offset, shdr->sh_size,
	    (void **)strtab);
	if (error != 0)
		return (error);
	*strcnt = shdr->sh_size;
	return (0);
}

int
elf_read_rel(struct elf_file *efile, int section_index, long *nrelp,
    GElf_Rel **relp)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	Elf_Data *data;
	GElf_Rel *rel;
	long i, nrel;

	scn = elf_getscn(efile->ef_elf, section_index);
	if (scn == NULL)
		return (EINVAL);
	if (gelf_getshdr(scn, &shdr) == NULL)
		return (EINVAL);
	data = elf_getdata(scn, NULL);
	if (data == NULL)
		return (EINVAL);

	nrel = elf_object_count(efile, ELF_T_REL, shdr.sh_size);
	rel = calloc(nrel, sizeof(*rel));
	if (rel == NULL)
		return (ENOMEM);

	for (i = 0; i < nrel; i++) {
		if (gelf_getrel(data, i, &rel[i]) == NULL) {
			free(rel);
			return (EINVAL);
		}
	}

	*nrelp = nrel;
	*relp = rel;
	return (0);
}

int
elf_read_rela(struct elf_file *efile, int section_index, long *nrelap,
    GElf_Rela **relap)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	Elf_Data *data;
	GElf_Rela *rela;
	long i, nrela;

	scn = elf_getscn(efile->ef_elf, section_index);
	if (scn == NULL)
		return (EINVAL);
	if (gelf_getshdr(scn, &shdr) == NULL)
		return (EINVAL);
	data = elf_getdata(scn, NULL);
	if (data == NULL)
		return (EINVAL);

	nrela = elf_object_count(efile, ELF_T_RELA, shdr.sh_size);
	rela = calloc(nrela, sizeof(*rela));
	if (rela == NULL)
		return (ENOMEM);

	for (i = 0; i < nrela; i++) {
		if (gelf_getrela(data, i, &rela[i]) == NULL) {
			free(rela);
			return (EINVAL);
		}
	}

	*nrelap = nrela;
	*relap = rela;
	return (0);
}

size_t
elf_pointer_size(struct elf_file *efile)
{
	return (efile->ef_pointer_size);
}

int
elf_int(struct elf_file *efile, const void *p)
{
	if (elf_encoding(efile) == ELFDATA2LSB)
		return (le32dec(p));
	else
		return (be32dec(p));
}

GElf_Addr
elf_address_from_pointer(struct elf_file *efile, const void *p)
{
	switch (elf_class(efile)) {
	case ELFCLASS32:
		if (elf_encoding(efile) == ELFDATA2LSB)
			return (le32dec(p));
		else
			return (be32dec(p));
	case ELFCLASS64:
		if (elf_encoding(efile) == ELFDATA2LSB)
			return (le64dec(p));
		else
			return (be64dec(p));
	default:
		__unreachable();
	}
}

int
elf_read_string(struct elf_file *efile, GElf_Addr address, void *dst,
    size_t len)
{
	return (EF_SEG_READ_STRING(efile, address, len, dst));
}

int
elf_read_linker_set(struct elf_file *efile, const char *name, GElf_Addr **bufp,
    long *countp)
{
	GElf_Addr *buf, start, stop;
	char *p;
	void *raw;
	long i, count;
	int error;

	error = EF_LOOKUP_SET(efile, name, &start, &stop, &count);
	if (error != 0)
		return (error);

	error = elf_read_relocated_data(efile, start,
	    count * elf_pointer_size(efile), &raw);
	if (error != 0)
		return (error);

	buf = calloc(count, sizeof(*buf));
	if (buf == NULL) {
		free(raw);
		return (ENOMEM);
	}

	p = raw;
	for (i = 0; i < count; i++) {
		buf[i] = elf_address_from_pointer(efile, p);
		p += elf_pointer_size(efile);
	}
	free(raw);

	*bufp = buf;
	*countp = count;
	return (0);
}

int
elf_read_mod_depend(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_depend *mdp)
{
	int *p;
	int error;

	error = elf_read_relocated_data(efile, addr, sizeof(int) * 3,
	    (void **)&p);
	if (error != 0)
		return (error);

	memset(mdp, 0, sizeof(*mdp));
	mdp->md_ver_minimum = elf_int(efile, p);
	mdp->md_ver_preferred = elf_int(efile, p + 1);
	mdp->md_ver_maximum = elf_int(efile, p + 2);
	free(p);
	return (0);
}

int
elf_read_mod_version(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_version *mdv)
{
	int error, value;

	error = EF_SEG_READ_REL(efile, addr, sizeof(int), &value);
	if (error != 0)
		return (error);

	memset(mdv, 0, sizeof(*mdv));
	mdv->mv_version = elf_int(efile, &value);
	return (0);
}

int
elf_read_mod_metadata(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_metadata *md)
{
	char *p;
	size_t len, offset, pointer_size;
	int error;

	pointer_size = elf_pointer_size(efile);
	len = 2 * sizeof(int);
	len = roundup(len, pointer_size);
	len += 2 * pointer_size;

	error = elf_read_relocated_data(efile, addr, len, (void **)&p);
	if (error != 0)
		return (error);

	memset(md, 0, sizeof(*md));
	offset = 0;
	md->md_version = elf_int(efile, p + offset);
	offset += sizeof(int);
	md->md_type = elf_int(efile, p + offset);
	offset += sizeof(int);
	offset = roundup(offset, pointer_size);
	md->md_data = elf_address_from_pointer(efile, p + offset);
	offset += pointer_size;
 	md->md_cval = elf_address_from_pointer(efile, p + offset);
	free(p);
	return (0);
}

int
elf_read_mod_pnp_match_info(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_pnp_match_info *pnp)
{
	char *p;
	size_t len, offset, pointer_size;
	int error;

	pointer_size = elf_pointer_size(efile);
	len = 3 * pointer_size;
	len = roundup(len, pointer_size);
	len += 2 * sizeof(int);

	error = elf_read_relocated_data(efile, addr, len, (void **)&p);
	if (error != 0)
		return (error);

	memset(pnp, 0, sizeof(*pnp));
	offset = 0;
	pnp->descr = elf_address_from_pointer(efile, p + offset);
	offset += pointer_size;
	pnp->bus = elf_address_from_pointer(efile, p + offset);
	offset += pointer_size;
	pnp->table = elf_address_from_pointer(efile, p + offset);
	offset += pointer_size;
	offset = roundup(offset, pointer_size);
	pnp->entry_len = elf_int(efile, p + offset);
	offset += sizeof(int);
	pnp->num_entry = elf_int(efile, p + offset);
	free(p);
	return (0);
}

int
elf_reloc(struct elf_file *efile, const void *reldata, Elf_Type reltype,
    GElf_Addr relbase, GElf_Addr dataoff, size_t len, void *dest)
{
	return (efile->ef_reloc(efile, reldata, reltype, relbase, dataoff, len,
	    dest));
}

int
elf_lookup_symbol(struct elf_file *efile, const char *name, GElf_Sym **sym,
    bool see_local)
{
	return (EF_LOOKUP_SYMBOL(efile, name, sym, see_local));
}
