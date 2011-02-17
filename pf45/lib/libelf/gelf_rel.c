/*-
 * Copyright (c) 2006 Joseph Koshy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/limits.h>

#include <assert.h>
#include <gelf.h>

#include "_libelf.h"

GElf_Rel *
gelf_getrel(Elf_Data *d, int ndx, GElf_Rel *dst)
{
	int ec;
	Elf *e;
	Elf_Scn *scn;
	Elf32_Rel *rel32;
	Elf64_Rel *rel64;
	size_t msz;
	uint32_t sh_type;

	if (d == NULL || ndx < 0 || dst == NULL ||
	    (scn = d->d_scn) == NULL ||
	    (e = scn->s_elf) == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	ec = e->e_class;
	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (ec == ELFCLASS32)
		sh_type = scn->s_shdr.s_shdr32.sh_type;
	else
		sh_type = scn->s_shdr.s_shdr64.sh_type;

	if (_libelf_xlate_shtype(sh_type) != ELF_T_REL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	msz = _libelf_msize(ELF_T_REL, ec, e->e_version);

	assert(msz > 0);

	if (msz * ndx >= d->d_size) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if (ec == ELFCLASS32) {
		rel32 = (Elf32_Rel *) d->d_buf + ndx;

		dst->r_offset = (Elf64_Addr) rel32->r_offset;
		dst->r_info   = ELF64_R_INFO(
		    (Elf64_Xword) ELF32_R_SYM(rel32->r_info),
		    ELF32_R_TYPE(rel32->r_info));

	} else {

		rel64 = (Elf64_Rel *) d->d_buf + ndx;

		*dst = *rel64;
	}

	return (dst);
}

int
gelf_update_rel(Elf_Data *d, int ndx, GElf_Rel *dr)
{
	int ec;
	Elf *e;
	Elf_Scn *scn;
	Elf32_Rel *rel32;
	Elf64_Rel *rel64;
	size_t msz;
	uint32_t sh_type;

	if (d == NULL || ndx < 0 || dr == NULL ||
	    (scn = d->d_scn) == NULL ||
	    (e = scn->s_elf) == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	ec = e->e_class;
	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (ec == ELFCLASS32)
		sh_type = scn->s_shdr.s_shdr32.sh_type;
	else
		sh_type = scn->s_shdr.s_shdr64.sh_type;

	if (_libelf_xlate_shtype(sh_type) != ELF_T_REL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	msz = _libelf_msize(ELF_T_REL, ec, e->e_version);
	assert(msz > 0);

	if (msz * ndx >= d->d_size) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	if (ec == ELFCLASS32) {
		rel32 = (Elf32_Rel *) d->d_buf + ndx;

		LIBELF_COPY_U32(rel32, dr, r_offset);

		if (ELF64_R_SYM(dr->r_info) > ELF32_R_SYM(~0UL) ||
		    ELF64_R_TYPE(dr->r_info) > ELF32_R_TYPE(~0U)) {
			LIBELF_SET_ERROR(RANGE, 0);
			return (0);
		}
		rel32->r_info = ELF32_R_INFO(ELF64_R_SYM(dr->r_info),
		    ELF64_R_TYPE(dr->r_info));
	} else {
		rel64 = (Elf64_Rel *) d->d_buf + ndx;

		*rel64 = *dr;
	}

	return (1);
}
