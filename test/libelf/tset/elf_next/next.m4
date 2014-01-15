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
 * $Id: next.m4 1622 2011-07-07 11:48:35Z jkoshy $
 */

#include <libelf.h>
#include <unistd.h>

#include "tet_api.h"

#include "elfts.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

/*
 * Test the `elf_next' API.
 */

/*
 * Assertion: with a NULL value passed in, elf_next returns ELF_C_NULL.
 */
void
tcArgsNull(void)
{
	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("NULL argument returns ELF_C_NULL");

	tet_result(elf_next(NULL) == ELF_C_NULL ? TET_PASS : TET_FAIL);
}

/*
 * Invoking elf_next on a non-archive should return ELF_C_NULL.
 */

static char *notar = "This is not an AR archive.";
static char *elf = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

undefine(`FN')
define(`FN',`
void
tcArgsNonAr`'TOUPPER($1)(void)
{
	Elf *e;
	Elf_Cmd c;
	int result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("error ELF_C_NULL with a non-archive \"$1\".");

	TS_OPEN_MEMORY(e, $1);

	result = TET_PASS;
	if ((c = elf_next(e)) != ELF_C_NULL)
		TP_FAIL("\"$1\" c=%d, != ELF_C_NULL", c);

	(void) elf_end(e);

	tet_result(result);
}')

FN(notar)
FN(elf)

/*
 * FN(ar-file-name, count)
 *
 * Returns ELF_C_READ as expected for an archive with > 1 members (as measured
 * by the number of ELF_C_READ return values).
 *
 * The test cases for elf_begin() verify that the correct Elf * pointers get
 * returned by a subsequent call to elf_begin().
 *
 */
undefine(`FN')
define(`FN',`
void
tcArArchive$1(void)
{
	int error, fd, i, result;
	Elf_Cmd c;
	Elf *a, *e;

	TP_ANNOUNCE("correctly iterates through \"a$1.ar\" with $2 members.");

	result = TET_UNRESOLVED;
	a = e = NULL;
	fd = -1;
	i = 0;

	_TS_OPEN_FILE(a, "a$1.ar", ELF_C_READ, fd, goto done;);

	(void) elf_errno();
	c = ELF_C_READ;
	while ((e = elf_begin(fd, c, a)) != NULL) {
		c = elf_next(e);
		(void) elf_end(e);
		i++;
	}

	if ((error = elf_errno()) != ELF_E_NONE) {
		TP_FAIL("error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;
	if (i != $2)
		TP_FAIL("i=%d expected $2.", i);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(1, 2) dnl text files with short names
FN(2, 2) dnl text files with long names
FN(3, 4) dnl text files + ELF objects.

