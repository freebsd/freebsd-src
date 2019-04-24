/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <machine/elf.h>

#include <err.h>
#include <errno.h>

#include "ef.h"

/*
 * Apply relocations to the values we got from the file. `relbase' is the
 * target relocation address of the section, and `dataoff' is the target
 * relocation address of the data in `dest'.
 */
int
ef_reloc(struct elf_file *ef, const void *reldata, int reltype, Elf_Off relbase,
    Elf_Off dataoff, size_t len, void *dest)
{
	Elf_Addr *where, val;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	Elf_Addr addend, addr;
	Elf_Size rtype, symidx;

	switch (reltype) {
	case EF_RELOC_REL:
		rel = (const Elf_Rel *)reldata;
		where = (Elf_Addr *)((char *)dest + relbase + rel->r_offset -
		    dataoff);
		addend = 0;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		break;
	case EF_RELOC_RELA:
		rela = (const Elf_Rela *)reldata;
		where = (Elf_Addr *)((char *)dest + relbase + rela->r_offset -
		    dataoff);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		return (EINVAL);
	}

	if ((char *)where < (char *)dest || (char *)where >= (char *)dest + len)
		return (0);

	if (reltype == EF_RELOC_REL)
		addend = *where;

	switch (rtype) {
#ifdef __LP64__
	case R_MIPS_64:		/* S + A */
#else
	case R_MIPS_32:		/* S + A */
#endif
		addr = EF_SYMADDR(ef, symidx);
		val = addr + addend;
		*where = val;
		break;
	default:
		warnx("unhandled relocation type %d", (int)rtype);
	}
	return (0);
}
