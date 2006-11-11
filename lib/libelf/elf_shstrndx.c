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

/*
 * Helpers to get/set the e_shstrndx field of the ELF header.
 */

int
_libelf_getshstrndx(Elf *e, void *eh, int ec, size_t *strndx)
{
	Elf_Scn *scn;
	void *sh;
	size_t n;

	n = (ec == ELFCLASS32) ? ((Elf32_Ehdr *) eh)->e_shstrndx :
	    ((Elf64_Ehdr *) eh)->e_shstrndx;

	if (n < SHN_LORESERVE) {
		*strndx = n;
		return (1);
	}

	if ((scn = elf_getscn(e, (size_t) SHN_UNDEF)) == NULL)
		return (0);
	if ((sh = _libelf_getshdr(scn, ec)) == NULL)
		return (0);

	if (ec == ELFCLASS32)
		*strndx = ((Elf32_Shdr *) sh)->sh_link;
	else
		*strndx = ((Elf64_Shdr *) sh)->sh_link;

	return (1);
}

int
_libelf_setshstrndx(Elf *e, void *eh, int ec, size_t strndx)
{
	Elf_Scn *scn;
	void *sh;

	if (strndx < SHN_LORESERVE) {
		if (ec == ELFCLASS32)
			((Elf32_Ehdr *) eh)->e_shstrndx = strndx;
		else
			((Elf64_Ehdr *) eh)->e_shstrndx = strndx;
		return (1);
	}

	if ((scn = elf_getscn(e, (size_t) SHN_UNDEF)) == NULL)
		return (0);
	if ((sh = _libelf_getshdr(scn, ec)) == NULL)
		return (0);

	if (ec == ELFCLASS32) {
		((Elf32_Ehdr *) eh)->e_shstrndx = SHN_XINDEX;
		((Elf32_Shdr *) sh)->sh_link = strndx;
	} else {
		((Elf64_Ehdr *) eh)->e_shstrndx = SHN_XINDEX;
		((Elf64_Shdr *) sh)->sh_link = strndx;
	}

	return (1);
}

int
elf_getshstrndx(Elf *e, size_t *strndx)
{
	void *eh;
	int ec;

	if (e == NULL || e->e_kind != ELF_K_ELF ||
	    ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64) ||
	    ((eh = _libelf_ehdr(e, ec, 0)) == NULL)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	return (_libelf_getshstrndx(e, eh, ec, strndx));
}

int
elf_setshstrndx(Elf *e, size_t strndx)
{
	void *eh;
	int ec;

	if (e == NULL || e->e_kind != ELF_K_ELF ||
	    ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64) ||
	    ((eh = _libelf_ehdr(e, ec, 0)) == NULL)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	return (_libelf_setshstrndx(e, eh, ec, strndx));
}
