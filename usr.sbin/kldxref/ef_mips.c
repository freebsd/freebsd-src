/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 John Baldwin <jhb@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <sys/endian.h>

#include <err.h>
#include <errno.h>
#include <gelf.h>

#include "ef.h"

/*
 * Apply relocations to the values obtained from the file. `relbase' is the
 * target relocation address of the section, and `dataoff/len' is the region
 * that is to be relocated, and has been copied to *dest
 */
static int
ef_mips_reloc(struct elf_file *ef, const void *reldata, Elf_Type reltype,
    GElf_Addr relbase, GElf_Addr dataoff, size_t len, void *dest)
{
	char *where;
	GElf_Addr addr, addend;
	GElf_Size rtype, symidx;
	const GElf_Rel *rel;
	const GElf_Rela *rela;

	switch (reltype) {
	case ELF_T_REL:
		rel = (const GElf_Rel *)reldata;
		where = (char *)dest + (relbase + rel->r_offset - dataoff);
		addend = 0;
		rtype = GELF_R_TYPE(rel->r_info);
		symidx = GELF_R_SYM(rel->r_info);
		break;
	case ELF_T_RELA:
		rela = (const GElf_Rela *)reldata;
		where = (char *)dest + (relbase + rela->r_offset - dataoff);
		addend = rela->r_addend;
		rtype = GELF_R_TYPE(rela->r_info);
		symidx = GELF_R_SYM(rela->r_info);
		break;
	default:
		return (EINVAL);
	}

	if (where < (char *)dest || where >= (char *)dest + len)
		return (0);

	if (reltype == ELF_T_REL) {
		if (elf_class(ef) == ELFCLASS64) {
			if (elf_encoding(ef) == ELFDATA2LSB)
				addend = le64dec(where);
			else
				addend = be64dec(where);
		} else {
			if (elf_encoding(ef) == ELFDATA2LSB)
				addend = le32dec(where);
			else
				addend = be32dec(where);
		}
	}

	switch (rtype) {
	case R_MIPS_64:		/* S + A */
		addr = EF_SYMADDR(ef, symidx) + addend;
		if (elf_encoding(ef) == ELFDATA2LSB)
			le64enc(where, addr);
		else
			be64enc(where, addr);
		break;
	case R_MIPS_32:		/* S + A */
		addr = EF_SYMADDR(ef, symidx) + addend;
		if (elf_encoding(ef) == ELFDATA2LSB)
			le32enc(where, addr);
		else
			be32enc(where, addr);
		break;
	default:
		warnx("unhandled relocation type %d", (int)rtype);
	}
	return (0);
}

ELF_RELOC(ELFCLASS32, ELFDATA2LSB, EM_MIPS, ef_mips_reloc);
ELF_RELOC(ELFCLASS32, ELFDATA2MSB, EM_MIPS, ef_mips_reloc);
ELF_RELOC(ELFCLASS64, ELFDATA2LSB, EM_MIPS, ef_mips_reloc);
ELF_RELOC(ELFCLASS64, ELFDATA2MSB, EM_MIPS, ef_mips_reloc);
