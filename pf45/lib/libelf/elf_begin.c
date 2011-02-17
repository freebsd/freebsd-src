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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ar.h>
#include <ctype.h>
#include <libelf.h>
#include <unistd.h>

#include "_libelf.h"

static Elf *
_libelf_open_object(int fd, Elf_Cmd c)
{
	Elf *e;
	void *m;
	struct stat sb;

	/*
	 * 'Raw' files are always mapped with 'PROT_READ'.  At
	 * elf_update(3) time for files opened with ELF_C_RDWR the
	 * mapping is unmapped, file data is written to using write(2)
	 * and then the raw data is immediately mapped back in.
	 */
	if (fstat(fd, &sb) < 0) {
		LIBELF_SET_ERROR(IO, errno);
		return (NULL);
	}

	m = NULL;
	if ((m = mmap(NULL, (size_t) sb.st_size, PROT_READ, MAP_PRIVATE, fd,
	    (off_t) 0)) == MAP_FAILED) {
		LIBELF_SET_ERROR(IO, errno);
		return (NULL);
	}

	if ((e = elf_memory(m, (size_t) sb.st_size)) == NULL) {
		(void) munmap(m, (size_t) sb.st_size);
		return (NULL);
	}

	e->e_flags |= LIBELF_F_MMAP;
	e->e_fd = fd;
	e->e_cmd = c;

	if (c == ELF_C_RDWR && e->e_kind == ELF_K_AR) {
		(void) elf_end(e);
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	return (e);
}

Elf *
elf_begin(int fd, Elf_Cmd c, Elf *a)
{
	Elf *e;

	e = NULL;

	if (LIBELF_PRIVATE(version) == EV_NONE) {
		LIBELF_SET_ERROR(SEQUENCE, 0);
		return (NULL);
	}

	switch (c) {
	case ELF_C_NULL:
		return (NULL);

	case ELF_C_WRITE:

		if (a != NULL) { /* not allowed for ar(1) archives. */
			LIBELF_SET_ERROR(ARGUMENT, 0);
			return (NULL);
		}

		/*
		 * Check writeability of `fd' immediately and fail if
		 * not writeable.
		 */
		if (ftruncate(fd, (off_t) 0) < 0) {
			LIBELF_SET_ERROR(IO, errno);
			return (NULL);
		}

		if ((e = _libelf_allocate_elf()) != NULL) {
			_libelf_init_elf(e, ELF_K_ELF);
			e->e_byteorder = LIBELF_PRIVATE(byteorder);
			e->e_fd = fd;
			e->e_cmd = c;
		}
		return (e);

	case ELF_C_RDWR:
		if (a != NULL) { /* not allowed for ar(1) archives. */
			LIBELF_SET_ERROR(ARGUMENT, 0);
			return (NULL);
		}
		/*FALLTHROUGH*/
	case ELF_C_READ:
		/*
		 * Descriptor `a' could be for a regular ELF file, or
		 * for an ar(1) archive.  If descriptor `a' was opened
		 * using a valid file descriptor, we need to check if
		 * the passed in `fd' value matches the original one.
		 */
		if (a &&
		    ((a->e_fd != -1 && a->e_fd != fd) || c != a->e_cmd)) {
			LIBELF_SET_ERROR(ARGUMENT, 0);
			return (NULL);
		}
		break;

	default:
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);

	}

	if (a == NULL)
		e = _libelf_open_object(fd, c);
	else if (a->e_kind == ELF_K_AR)
		e = _libelf_ar_open_member(a->e_fd, c, a);
	else
		(e = a)->e_activations++;

	return (e);
}
