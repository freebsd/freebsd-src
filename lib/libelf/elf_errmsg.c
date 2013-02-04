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

#include <libelf.h>
#include <string.h>

#include "_libelf.h"

/*
 * Retrieve a human readable translation for an error message.
 */

static const char *_libelf_errors[] = {
#define	DEFINE_ERROR(N,S)	[ELF_E_##N] = S
	DEFINE_ERROR(NONE,	"No Error"),
	DEFINE_ERROR(ARCHIVE,	"Malformed ar(1) archive"),
	DEFINE_ERROR(ARGUMENT,	"Invalid argument"),
	DEFINE_ERROR(CLASS,	"ELF class mismatch"),
	DEFINE_ERROR(DATA,	"Invalid data buffer descriptor"),
	DEFINE_ERROR(HEADER,	"Missing or malformed ELF header"),
	DEFINE_ERROR(IO,	"I/O error"),
	DEFINE_ERROR(LAYOUT,	"Layout constraint violation"),
	DEFINE_ERROR(MODE,	"Incorrect ELF descriptor mode"),
	DEFINE_ERROR(RANGE,	"Value out of range of target"),
	DEFINE_ERROR(RESOURCE,	"Resource exhaustion"),
	DEFINE_ERROR(SECTION,	"Invalid section descriptor"),
	DEFINE_ERROR(SEQUENCE,	"API calls out of sequence"),
	DEFINE_ERROR(UNIMPL,	"Unimplemented feature"),
	DEFINE_ERROR(VERSION,	"Unknown ELF API version"),
	DEFINE_ERROR(NUM,	"Unknown error")
#undef	DEFINE_ERROR
};

const char *
elf_errmsg(int error)
{
	int oserr;

	if (error == 0 && (error = LIBELF_PRIVATE(error)) == 0)
	    return NULL;
	else if (error == -1)
	    error = LIBELF_PRIVATE(error);

	oserr = error >> LIBELF_OS_ERROR_SHIFT;
	error &= LIBELF_ELF_ERROR_MASK;

	if (error < 0 || error >= ELF_E_NUM)
		return _libelf_errors[ELF_E_NUM];
	if (oserr) {
		strlcpy(LIBELF_PRIVATE(msg), _libelf_errors[error],
		    sizeof(LIBELF_PRIVATE(msg)));
		strlcat(LIBELF_PRIVATE(msg), ": ", sizeof(LIBELF_PRIVATE(msg)));
		strlcat(LIBELF_PRIVATE(msg), strerror(oserr),
		    sizeof(LIBELF_PRIVATE(msg)));
		return (const char *)&LIBELF_PRIVATE(msg);
	}
	return _libelf_errors[error];
}

#if	defined(LIBELF_TEST_HOOKS)

const char *
_libelf_get_unknown_error_message(void)
{
	return _libelf_errors[ELF_E_NUM];
}

const char *
_libelf_get_no_error_message(void)
{
	return _libelf_errors[0];
}

#endif	/* LIBELF_TEST_HOOKS */
