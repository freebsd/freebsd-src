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

#include <ar.h>
#include <libelf.h>

#include "_libelf.h"

int
_libelf_getshnum(Elf *e, void *eh, int ec, size_t *shnum)
{
	Elf64_Off off;
	Elf_Scn *scn;
	void *sh;
	size_t n;

	if (ec == ELFCLASS32) {
		n = ((Elf32_Ehdr *) eh)->e_shnum;
		off = (Elf64_Off) ((Elf32_Ehdr *) eh)->e_shoff;
	} else {
		n = ((Elf64_Ehdr *) eh)->e_shnum;
		off = ((Elf64_Ehdr *) eh)->e_shoff;
	}

	if (n != 0) {
		*shnum = n;
		return (1);
	}

	if (off == 0L) {
		*shnum = (size_t) 0;
		return (1);
	}

	/*
	 * If 'e_shnum' is zero and 'e_shoff' is non-zero, the file is
	 * using extended section numbering, and the true section
	 * number is kept in the 'sh_size' field of the section header
	 * at offset SHN_UNDEF.
	 */
	if ((scn = elf_getscn(e, (size_t) SHN_UNDEF)) == NULL)
		return (0);
	if ((sh = _libelf_getshdr(scn, ec)) == NULL)
		return (0);

	if (ec == ELFCLASS32)
		*shnum = ((Elf32_Shdr *) sh)->sh_size;
	else
		*shnum = ((Elf64_Shdr *) sh)->sh_size;

	return (1);
}

int
_libelf_setshnum(Elf *e, void *eh, int ec, size_t shnum)
{
	Elf_Scn *scn;
	void *sh;

	if (shnum < SHN_LORESERVE) {
		if (ec == ELFCLASS32)
			((Elf32_Ehdr *) eh)->e_shnum = shnum;
		else
			((Elf64_Ehdr *) eh)->e_shnum = shnum;
		return (1);
	}

	if ((scn = elf_getscn(e, (size_t) SHN_UNDEF)) == NULL)
		return (0);
	if ((sh = _libelf_getshdr(scn, ec)) == NULL)
		return (0);

	if (ec == ELFCLASS32)
		((Elf32_Shdr *) sh)->sh_size = shnum;
	else
		((Elf64_Shdr *) sh)->sh_size = shnum;

	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	return (1);
}

int
elf_getshnum(Elf *e, size_t *shnum)
{
	void *eh;
	int ec;

	if (e == NULL || e->e_kind != ELF_K_ELF ||
	    ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64) ||
	    ((eh = _libelf_ehdr(e, ec, 0)) == NULL)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	return (_libelf_getshnum(e, eh, ec, shnum));
}
