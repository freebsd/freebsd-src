/*-
 * Copyright (c) 2006,2011 Joseph Koshy
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
 * $Id: getident.m4 1694 2011-08-02 04:34:35Z jkoshy $
 */

#include <ar.h>
#include <libelf.h>
#include <string.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

void
tcNullNull(void)
{
	int result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getident(NULL,...) fails with error ELF_E_ARGUMENT.");

	result = TET_PASS;
	if (elf_getident(NULL, NULL) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	tet_result(result);
}

void
tcNullSize(void)
{
	size_t dummy;
	int result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getident(NULL,&foo) fails, and sets"
	    " `foo' to zero.");

	dummy = (size_t) 0xdeadc0de;

	result = TET_PASS;
	if (elf_getident(NULL, &dummy) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT ||
	    dummy != 0)
		result = TET_FAIL;
	tet_result(result);

}

changequote({,})
static char ar_file[] = "!<arch>\n"
	"t/              1151656346  1001  0     100644  5         `\n"
	"Test\n";
changequote

void
tcMainArIdent(void)
{
	Elf *e;
	char *p;
	size_t sz;
	int result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("an ar(1) archive's ident is correctly returned.");

	TS_OPEN_MEMORY(e, ar_file);

	result = TET_PASS;
	sz = (size_t) 0xdeadc0de;
	if ((p = elf_getident(e, &sz)) == NULL ||
	    sz != SARMAG || strncmp(p, ARMAG, SARMAG))
		result = TET_FAIL;
	tet_result(result);
}

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

void
tcMainElfIdent(void)
{
	Elf *e;
	char *p;
	size_t sz;
	int result;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: an ELF object's ident is correctly"
	    " returned.");

	TS_OPEN_MEMORY(e, elf_file);

	result = TET_PASS;
	sz = (size_t) 0xdeadc0de;
	if ((p = elf_getident(e, &sz)) == NULL ||
	    sz != EI_NIDENT ||
	    memcmp(elf_file, p, sz))
		result = TET_FAIL;
	tet_result(result);
}


static char unknown_data[] = "Revenge!  Revenge!";

void
tcMainUnknownData(void)
{
	Elf *e;
	char *p;
	size_t sz;
	int result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getident() returns the initial bytes of"
	    " an unknown data object.");

	TS_OPEN_MEMORY(e, unknown_data);

	result = TET_PASS;
	sz = (size_t) 0xdeadc0de;
	if ((p = elf_getident(e, &sz)) == NULL ||
	    sz != sizeof(unknown_data) ||
	    memcmp(p, unknown_data, sizeof(unknown_data)))
		result = TET_FAIL;
	tet_result(result);
}

/*
 * TODO:
 *
 * - getident on an elf descriptor opened for WRITE should fail until
 *   an elf_update() is done.
 *
 */
