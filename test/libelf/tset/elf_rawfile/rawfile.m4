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
 * $Id: rawfile.m4 1694 2011-08-02 04:34:35Z jkoshy $
 */

#include <libelf.h>
#include <string.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

/*
 * Test the `elf_rawfile' entry point.
 */

IC_REQUIRES_VERSION_INIT();

/*
 * A NULL `elf *' argument should return the appropriate error,
 * and set the `sz' pointer to zero.
 */
void
tcNullNonNull(void)
{
	size_t sz;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_rawfile(NULL,...) returns an error,"
	    " and sets the size pointer to zero.");

	sz = -1;
	if (elf_rawfile(NULL, &sz) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT ||
	    sz != 0)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}

changequote({,})
static char ar_file[] = "!<arch>\n"
	"t/              1151656346  1001  0     100644  5         `\n"
	"Test\n";
changequote

void
tcValidAr(void)
{
	char *p;
	Elf *e;
	size_t sz;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_rawfile(E,...) descriptor with a valid"
	    " descriptor `E' to an ar(1) archive succeeds and returns"
	    " correct values.");

	TS_OPEN_MEMORY(e, ar_file);

	if ((p = elf_rawfile(e, &sz)) == NULL ||
	    sz != sizeof(ar_file) ||
	    memcmp(p, ar_file, sizeof(ar_file)))
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

void
tcValidElf(void)
{
	char *p;
	Elf *e;
	size_t sz;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_rawfile(E,...) descriptor with a valid"
	    " descriptor `E' to an ELF object succeeds and returns"
	    " correct values.");

	TS_OPEN_MEMORY(e, elf_file);

	if ((p = elf_rawfile(e, &sz)) == NULL ||
	    sz != sizeof(elf_file) ||
	    memcmp(p, elf_file, sizeof(elf_file)))
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

void
tcValidNull(void)
{
	char *p;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("assertion: elf_rawfile(E,NULL) on a valid descriptor "
	    "`E' and NULL sz pointer succeeds and returns the correct "
	    "value.");

	TS_OPEN_MEMORY(e, elf_file);

	if ((p = elf_rawfile(e, NULL)) == NULL ||
	    memcmp(p, elf_file, sizeof(elf_file)))
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

/*
 * Todo:
 *
 * Test elf_rawfile() on an ELF object embedded inside an `ar' archive.
 */
