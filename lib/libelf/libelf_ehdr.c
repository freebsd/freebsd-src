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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>

#include "_libelf.h"

#define	EHDR_INIT(E,SZ)	 do {						\
		Elf##SZ##_Ehdr *eh = (E);				\
		eh->e_ident[EI_MAG0] = ELFMAG0;				\
		eh->e_ident[EI_MAG1] = ELFMAG1;				\
		eh->e_ident[EI_MAG2] = ELFMAG2;				\
		eh->e_ident[EI_MAG3] = ELFMAG3;				\
		eh->e_ident[EI_CLASS] = ELFCLASS##SZ;			\
		eh->e_ident[EI_DATA]  = ELFDATANONE;			\
		eh->e_ident[EI_VERSION] = LIBELF_PRIVATE(version);	\
		eh->e_machine = EM_NONE;				\
		eh->e_type    = ELF_K_NONE;				\
		eh->e_version = LIBELF_PRIVATE(version);		\
	} while (0)

void *
_libelf_ehdr(Elf *e, int ec, int allocate)
{
	size_t fsz, msz;
	void *ehdr;
	void (*xlator)(char *_d, char *_s, size_t _c, int _swap);

	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (e == NULL || e->e_kind != ELF_K_ELF) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if (e->e_class != ELFCLASSNONE && e->e_class != ec) {
		LIBELF_SET_ERROR(CLASS, 0);
		return (NULL);
	}

	if (e->e_version != EV_CURRENT) {
		LIBELF_SET_ERROR(VERSION, 0);
		return (NULL);
	}

	if (e->e_class == ELFCLASSNONE)
		e->e_class = ec;

	if (ec == ELFCLASS32)
		ehdr = (void *) e->e_u.e_elf.e_ehdr.e_ehdr32;
	else
		ehdr = (void *) e->e_u.e_elf.e_ehdr.e_ehdr64;

	if (ehdr != NULL)	/* already have a translated ehdr */
		return (ehdr);

	fsz = gelf_fsize(e, ELF_T_EHDR, (size_t) 1, e->e_version);

	assert(fsz > 0);

	if (e->e_cmd != ELF_C_WRITE && e->e_rawsize < fsz) {
		LIBELF_SET_ERROR(HEADER, 0);
		return (NULL);
	}

	msz = _libelf_msize(ELF_T_EHDR, ec, EV_CURRENT);

	assert(msz > 0);

	if ((ehdr = calloc((size_t) 1, msz)) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, 0);
		return (NULL);
	}

	if (ec == ELFCLASS32) {
		e->e_u.e_elf.e_ehdr.e_ehdr32 = ehdr;
		EHDR_INIT(ehdr,32);
	} else {
		e->e_u.e_elf.e_ehdr.e_ehdr64 = ehdr;
		EHDR_INIT(ehdr,64);
	}

	if (allocate)
		e->e_flags |= ELF_F_DIRTY;

	if (e->e_cmd == ELF_C_WRITE)
		return (ehdr);

	xlator = _libelf_get_translator(ELF_T_EHDR, ELF_TOMEMORY, ec);
	(*xlator)(ehdr, e->e_rawfile, (size_t) 1,
	    e->e_byteorder != LIBELF_PRIVATE(byteorder));

	return (ehdr);
}
