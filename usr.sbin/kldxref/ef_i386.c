/*-
 * Copyright (c) 2003 Jake Burkholder.
 * Copyright 1996-1998 John D. Polstra.
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

#include <sys/types.h>
#include <machine/elf.h>

#include <err.h>
#include <errno.h>
#include <string.h>

#include "ef.h"

/*
 * Apply relocations to the values we got from the file.
 */
int
ef_reloc(struct elf_file *ef, const void *data, int type, Elf_Off offset,
    size_t len, void *dest)
{
	Elf_Addr *where, addr, addend;
	Elf_Word rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;

	switch (type) {
	case EF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *)(dest + rel->r_offset - offset);
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		break;
	case EF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *)(dest + rela->r_offset - offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		return (EINVAL);
	}

	if ((char *)where < (char *)dest || (char *)where >= (char *)dest + len)
		return (0);

	if (type == EF_RELOC_REL)
		addend = *where;

	switch (rtype) {
	case R_386_RELATIVE:	/* A + B */
		addr = (Elf_Addr)addend;
		*where = addr;
		break;
	case R_386_32:	/* S + A - P */
		addr = EF_SYMADDR(ef, symidx);
		addr += addend;
		*where = addr;
		break;
	case R_386_GLOB_DAT:	/* S */
		addr = EF_SYMADDR(ef, symidx);
		*where = addr;
		break;
	default:
		warnx("unhandled relocation type %d", (int)rtype);
	}
	return (0);
}
