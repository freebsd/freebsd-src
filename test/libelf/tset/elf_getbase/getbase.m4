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
 * $Id: getbase.m4 1694 2011-08-02 04:34:35Z jkoshy $
 */

#include <libelf.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

/*
 * Test the `elf_getbase' entry point.
 */

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

void
tcNonMemberElf(void)
{
	int result;
	off_t off;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getbase() on an ELF file returns 0");

	TS_OPEN_MEMORY(e, elf_file);

	result = TET_PASS;
	if ((off = elf_getbase(e)) != (off_t) 0)
		result = TET_FAIL;

	tet_result(result);
	(void) elf_end(e);
}

changequote({,})
static char ar_file[] = "!<arch>\n"
	"t/              1151656346  1001  0     100644  5         `\n"
	"Test\n";
changequote

void
tcNonMemberAr(void)
{
	int result;
	off_t off;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getbase on an AR file returns 0");

	TS_OPEN_MEMORY(e, ar_file);

	result = TET_PASS;

	if ((off = elf_getbase(e)) != (off_t) 0)
		result = TET_FAIL;

	tet_result(result);
	(void) elf_end(e);
}

/*
 * Todo:
 * - test an ar archive with an embedded ELF file.
 * - test an ar archive with an embedded non-elf file.
 */
