/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Tiger Gao under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

#include <gelf.h>
#include <libelf.h>

#include "_libelf.h"

void *
_libelf_getchdr(Elf_Scn *s, int ec)
{
	Elf *e;
	void *sh;
	Elf32_Shdr *sh32;
	Elf64_Shdr *sh64;

	sh32 = NULL;
	sh64 = NULL;

	if (s == NULL || (e = s->s_elf) == NULL || e->e_kind != ELF_K_ELF) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if (ec == ELFCLASSNONE) {
		ec = e->e_class;
	} else if (ec != e->e_class) {
		LIBELF_SET_ERROR(CLASS, 0);
		return (NULL);
	}

	if ((sh = _libelf_getshdr(s, ec)) == NULL) {
		LIBELF_SET_ERROR(HEADER, 0);
		return (NULL);
	}

	if (ec == ELFCLASS32) {
		sh32 = (Elf32_Shdr *)sh;
		if ((sh32->sh_flags & SHF_ALLOC) != 0) {
			LIBELF_SET_ERROR(INVALID_SECTION_FLAGS, 0);
			return (NULL);
		}

		if (sh32->sh_type == SHT_NULL || sh32->sh_type == SHT_NOBITS) {
			LIBELF_SET_ERROR(INVALID_SECTION_TYPE, 0);
			return (NULL);
		}

		if ((sh32->sh_flags & SHF_COMPRESSED) == 0) {
			LIBELF_SET_ERROR(NOT_COMPRESSED, 0);
			return (NULL);
		}
	} else {
		sh64 = (Elf64_Shdr *)sh;
		if ((sh64->sh_flags & SHF_ALLOC) != 0) {
			LIBELF_SET_ERROR(INVALID_SECTION_FLAGS, 0);
			return (NULL);
		}

		if (sh64->sh_type == SHT_NULL || sh64->sh_type == SHT_NOBITS) {
			LIBELF_SET_ERROR(INVALID_SECTION_TYPE, 0);
			return (NULL);
		}

		if ((sh64->sh_flags & SHF_COMPRESSED) == 0) {
			LIBELF_SET_ERROR(NOT_COMPRESSED, 0);
			return (NULL);
		}
	}

	Elf_Data *d = elf_getdata(s, NULL);

	if (!d)
		return (NULL);

	return ((void *)d->d_buf);
}
