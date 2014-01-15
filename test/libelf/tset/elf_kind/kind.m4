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
 * $Id: kind.m4 1694 2011-08-02 04:34:35Z jkoshy $
 */

#include <libelf.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

/*
 * Test the `elf_kind' entry point.
 */

void
tcNullParameter(void)
{
	TP_ANNOUNCE("NULL elf returns null.");

	TP_CHECK_INITIALIZATION();

	tet_result(elf_kind(NULL) == ELF_K_NONE ? TET_PASS : TET_FAIL);
}

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

void
tcValidElf(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("valid ELF file returns ELF_K_ELF.");

	if ((e = elf_memory(elf_file, sizeof(elf_file))) == NULL) {
		tet_printf("elf_memory: %s", elf_errmsg(-1));
		tet_result(TET_UNRESOLVED);
		return;
	}

	tet_result(elf_kind(e) == ELF_K_ELF ? TET_PASS : TET_FAIL);

	(void) elf_end(e);
}

changequote({,})
static char ar_file[] = "!<arch>\n"
	"t/              1151656346  1001  0     100644  5         `\n"
	"Test\n";
changequote

void
tcValidAr(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("valid ar archive returns ELF_K_AR.");

	if ((e = elf_memory(ar_file, sizeof(ar_file))) == NULL) {
		tet_printf("elf_memory: %s", elf_errmsg(-1));
		tet_result(TET_UNRESOLVED);
		return;
	}

	tet_result(elf_kind(e) == ELF_K_AR ? TET_PASS : TET_FAIL);

	(void) elf_end(e);
}

static char unknown_file[] = "0xdeadc0de";

void
tcUnknownKind(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("unknown file type returns ELF_K_NONE.");

	if ((e = elf_memory(unknown_file, sizeof(unknown_file))) == NULL) {
		tet_printf("elf_memory: %s", elf_errmsg(-1));
		tet_result(TET_UNRESOLVED);
	}

	tet_result(elf_kind(e) == ELF_K_NONE ? TET_PASS : TET_FAIL);

	(void) elf_end(e);
}
