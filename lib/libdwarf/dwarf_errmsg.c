/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
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

#include <stdio.h>
#include "_libdwarf.h"

static const char *_libdwarf_errors[] = {
#define	DEFINE_ERROR(N,S)		[DWARF_E_##N] = S
	DEFINE_ERROR(NONE,		"No Error"),
	DEFINE_ERROR(ERROR,		"An error"),
	DEFINE_ERROR(NO_ENTRY,		"No entry found"),
	DEFINE_ERROR(ARGUMENT,		"Invalid argument"),
	DEFINE_ERROR(DEBUG_INFO,	"Debug info NULL"),
	DEFINE_ERROR(MEMORY,		"Insufficient memory"),
	DEFINE_ERROR(ELF,		"ELF error"),
	DEFINE_ERROR(INVALID_CU,	"Invalid compilation unit data"),
	DEFINE_ERROR(CU_VERSION,	"Wrong CU version. Only 2 and 3 supported"),
	DEFINE_ERROR(MISSING_ABBREV,	"Abbrev not found"),
	DEFINE_ERROR(NOT_IMPLEMENTED,	"Unimplemented code at"),
	DEFINE_ERROR(CU_CURRENT,	"No current compilation unit"),
	DEFINE_ERROR(BAD_FORM,		"Wrong form type for attribute value"),
	DEFINE_ERROR(INVALID_EXPR,	"Invalid DWARF expression"),
	DEFINE_ERROR(NUM,		"Unknown DWARF error")
#undef	DEFINE_ERROR
};

const char *
dwarf_errmsg(Dwarf_Error *error)
{
	const char *p;

	if (error == NULL)
		return NULL;

	if (error->err_error < 0 || error->err_error >= DWARF_E_NUM)
		return _libdwarf_errors[DWARF_E_NUM];
	else if (error->err_error == DWARF_E_NONE)
		return _libdwarf_errors[DWARF_E_NONE];
	else
		p = _libdwarf_errors[error->err_error];

	if (error->err_error == DWARF_E_ELF)
		snprintf(error->err_msg, sizeof(error->err_msg),
		    "ELF error : %s [%s(%d)]", elf_errmsg(error->elf_error),
		    error->err_func, error->err_line);
	else
		snprintf(error->err_msg, sizeof(error->err_msg),
		    "%s [%s(%d)]", p, error->err_func, error->err_line);

	return (const char *) error->err_msg;
}
