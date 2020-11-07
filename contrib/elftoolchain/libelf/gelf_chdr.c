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

#include <assert.h>
#include <gelf.h>
#include <libelf.h>
#include <limits.h>
#include <stdint.h>

#include "_libelf.h"

Elf32_Chdr *
elf32_getchdr(Elf_Scn *s)
{
	return (_libelf_getchdr(s, ELFCLASS32));
}

Elf64_Chdr *
elf64_getchdr(Elf_Scn *s)
{
	return (_libelf_getchdr(s, ELFCLASS64));
}

GElf_Chdr *
gelf_getchdr(Elf_Scn *s, GElf_Chdr *d)
{
	int ec;
	void *ch;
	Elf32_Chdr *ch32;
	Elf64_Chdr *ch64;

	if (d == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if ((ch = _libelf_getchdr(s, ELFCLASSNONE)) == NULL)
		return (NULL);

	ec = s->s_elf->e_class;
	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (ec == ELFCLASS32) {
		ch32 = (Elf32_Chdr *)ch;

		d->ch_type = (Elf64_Word)ch32->ch_type;
		d->ch_size = (Elf64_Xword)ch32->ch_size;
		d->ch_addralign = (Elf64_Xword)ch32->ch_addralign;
	} else {
		ch64 = (Elf64_Chdr *)ch;
		*d = *ch64;
	}

	return (d);
}
