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

#include <assert.h>
#include <errno.h>
#include <gelf.h>
#include <libelf.h>
#include <stdlib.h>

#include "_libelf.h"

/*
 * Load an ELF section table and create a list of Elf_Scn structures.
 */
static int
_libelf_load_scn(Elf *e, void *ehdr)
{
	int ec, swapbytes;
	size_t fsz, i, shnum;
	uint64_t shoff;
	uint32_t shtype;
	char *src;
	Elf32_Ehdr *eh32;
	Elf64_Ehdr *eh64;
	Elf_Scn *scn;
	void (*xlator)(char *_d, char *_s, size_t _c, int _swap);

	assert(e != NULL);
	assert(ehdr != NULL);

#define	CHECK_EHDR(E,EH)	do {				\
		if (fsz != (EH)->e_shentsize ||			\
		    shoff + fsz * shnum > e->e_rawsize) {	\
			LIBELF_SET_ERROR(HEADER, 0);		\
			return (0);				\
		}						\
	} while (0)

	fsz = gelf_fsize(e, ELF_T_SHDR, (size_t) 1, e->e_version);
	assert(fsz > 0);

	ec = e->e_class;
	if (ec == ELFCLASS32) {
		eh32 = (Elf32_Ehdr *) ehdr;
		shnum = eh32->e_shnum;
		shoff = (uint64_t) eh32->e_shoff;
		CHECK_EHDR(e, eh32);
	} else {
		eh64 = (Elf64_Ehdr *) ehdr;
		shnum = eh64->e_shnum;
		shoff = eh64->e_shoff;
		CHECK_EHDR(e, eh64);
	}

	xlator = _libelf_get_translator(ELF_T_SHDR, ELF_TOMEMORY, ec);

	swapbytes = e->e_byteorder != LIBELF_PRIVATE(byteorder);
	src = e->e_rawfile + shoff;
	i = 0;

	if (shnum == (size_t) 0 && shoff != 0LL) {
		/* Extended section numbering */
		if ((scn = _libelf_allocate_scn(e, (size_t) 0)) == NULL)
			return (0);

		(*xlator)((char *) &scn->s_shdr, src, (size_t) 1, swapbytes);

		if (ec == ELFCLASS32) {
			shtype = scn->s_shdr.s_shdr32.sh_type;
			shnum = scn->s_shdr.s_shdr32.sh_size;
		} else {
			shtype = scn->s_shdr.s_shdr64.sh_type;
			shnum = scn->s_shdr.s_shdr64.sh_size;
		}

		if (shtype != SHT_NULL) {
			LIBELF_SET_ERROR(SECTION, 0);
			return (0);
		}

		scn->s_size = 0LL;
		scn->s_offset = scn->s_rawoff = 0LL;

		i++;
		src += fsz;
	}

	for (; i < shnum; i++, src += fsz) {
		if ((scn = _libelf_allocate_scn(e, i)) == NULL)
			return (0);

		(*xlator)((char *) &scn->s_shdr, src, (size_t) 1, swapbytes);

		if (ec == ELFCLASS32) {
			scn->s_offset = scn->s_rawoff =
			    scn->s_shdr.s_shdr32.sh_offset;
			scn->s_size = scn->s_shdr.s_shdr32.sh_size;
		} else {
			scn->s_offset = scn->s_rawoff =
			    scn->s_shdr.s_shdr64.sh_offset;
			scn->s_size = scn->s_shdr.s_shdr64.sh_size;
		}
	}
	return (1);
}


Elf_Scn *
elf_getscn(Elf *e, size_t index)
{
	int ec;
	void *ehdr;
	Elf_Scn *s;

	if (e == NULL || e->e_kind != ELF_K_ELF ||
	    ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if ((ehdr = _libelf_ehdr(e, ec, 0)) == NULL)
		return (NULL);

	if (e->e_cmd != ELF_C_WRITE && STAILQ_EMPTY(&e->e_u.e_elf.e_scn) &&
	    _libelf_load_scn(e, ehdr) == 0)
		return (NULL);

	STAILQ_FOREACH(s, &e->e_u.e_elf.e_scn, s_next)
		if (s->s_ndx == index)
			return (s);

	LIBELF_SET_ERROR(ARGUMENT, 0);
	return (NULL);
}

size_t
elf_ndxscn(Elf_Scn *s)
{
	if (s == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (SHN_UNDEF);
	}
	return (s->s_ndx);
}

Elf_Scn *
elf_newscn(Elf *e)
{
	int ec;
	void *ehdr;
	size_t shnum;
	Elf_Scn *scn;

	if (e == NULL || e->e_kind != ELF_K_ELF) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if ((ec = e->e_class) != ELFCLASS32 && ec != ELFCLASS64) {
		LIBELF_SET_ERROR(CLASS, 0);
		return (NULL);
	}

	if ((ehdr = _libelf_ehdr(e, ec, 0)) == NULL)
		return (NULL);

	/*
	 * The application may be asking for a new section descriptor
	 * on an ELF object opened with ELF_C_RDWR or ELF_C_READ.  We
	 * need to bring in the existing section information before
	 * appending a new one to the list.
	 *
	 * Per the ELF(3) API, an application is allowed to open a
	 * file using ELF_C_READ, mess with its internal structure and
	 * use elf_update(...,ELF_C_NULL) to compute its new layout.
	 */
	if (e->e_cmd != ELF_C_WRITE && STAILQ_EMPTY(&e->e_u.e_elf.e_scn) &&
	    _libelf_load_scn(e, ehdr) == 0)
		return (NULL);

	if (_libelf_getshnum(e, ehdr, ec, &shnum) == 0)
		return (NULL);

	if (STAILQ_EMPTY(&e->e_u.e_elf.e_scn)) {
		assert(shnum == 0);
		if ((scn = _libelf_allocate_scn(e, (size_t) SHN_UNDEF)) ==
		    NULL)
			return (NULL);
		shnum++;
	}

	assert(shnum > 0);

	if ((scn = _libelf_allocate_scn(e, shnum)) == NULL)
		return (NULL);

	shnum++;

	if (_libelf_setshnum(e, ehdr, ec, shnum) == 0)
		return (NULL);

	(void) elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY);

	return (scn);
}

Elf_Scn *
elf_nextscn(Elf *e, Elf_Scn *s)
{
	if (e == NULL || (e->e_kind != ELF_K_ELF) ||
	    (s && s->s_elf != e)) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	return (s == NULL ? elf_getscn(e, (size_t) 1) :
	    STAILQ_NEXT(s, s_next));
}
