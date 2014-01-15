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
 * $Id: memory.m4 1694 2011-08-02 04:34:35Z jkoshy $
 */

#include <ar.h>
#include <libelf.h>
#include <string.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

/*
 * Test the `elf_memory' entry point.
 *
 * See also: elf_memory() sequence tests in the test cases for elf_version().
 */

/*
 * Check that a NULL memory pointer and a zero size arena size are
 * rejected.
 */

void
tcInvalidArgNullPtrs(void)
{
	Elf *e;

	TP_ANNOUNCE("elf_memory(NULL,0) results in a NULL return"
	    " and an error return of ELF_E_ARGUMENT.");

	TP_CHECK_INITIALIZATION();

	if ((e = elf_memory(NULL, ~0)) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT) {
		tet_result(TET_FAIL);
		return;
	}

	if ((e = elf_memory((char *) &e, 0)) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT) {
		tet_result(TET_FAIL);
		return;
	}

	tet_result(TET_PASS);
}

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

/*
 * The next two test cases check a pointer to valid ELF content, but
 * with (a) a valid object size and (b) a too-small object size.
 */

void
tcValidElfValidSize(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("valid ELF contents and size are correctly"
	    " recognized as an ELF file.");

	if ((e = elf_memory(elf_file, sizeof(elf_file))) == NULL ||
	    elf_kind(e) != ELF_K_ELF) {
		tet_result(TET_FAIL);
		return;
	}

	(void) elf_end(e);
	tet_result(TET_PASS);
}

void
tcValidElfInvalidSize(void)
{
	Elf *e;

	TP_ANNOUNCE("a valid ELF prelude with a too-small size is"
	    " to be recognized as 'DATA'.");

	TP_CHECK_INITIALIZATION();

	/* Check size > SARMAG, but < EI_NIDENT */
	if ((e = elf_memory(elf_file, EI_NIDENT-1)) == NULL ||
	    elf_kind(e) != ELF_K_NONE)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);
}


void
tcInvalidElfSignature(void)
{
	Elf *e;
	char newelf[sizeof(elf_file)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("an ELF-like object with an invalid signature"
	    " should be recognized as 'DATA'.");

	memcpy(newelf, elf_file, sizeof(elf_file));
	newelf[EI_MAG0] = '\1';

	if ((e = elf_memory(newelf, sizeof(newelf))) == NULL ||
	    elf_kind(e) != ELF_K_NONE)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);
}

void
tcInvalidElfVersionMismatch(void)
{
	Elf *e;
	char newelf[sizeof(elf_file)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("an ELF blob with an invalid version number is"
	    " to be rejected, with error ELF_E_VERSION.");

	memcpy(newelf, elf_file, sizeof(elf_file));
	newelf[EI_VERSION] = EV_CURRENT+1;	/* change version */

	if ((e = elf_memory(newelf, sizeof(newelf))) != NULL ||
	    elf_errno() != ELF_E_VERSION)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);
}

/*
 * `ar' archives.
 */

changequote({,})
static char ar_file[] = "!<arch>\n"
	"t/              1151656346  1001  0     100644  5         `\n"
	"Test\n";
changequote

void
tcValidArValid(void)
{
	Elf *e;
	TP_ANNOUNCE("an valid AR archive is accepted as type"
	    " ELF_K_AR.");

	TP_CHECK_INITIALIZATION();

	if ((e = elf_memory(ar_file, sizeof(ar_file))) == NULL ||
	    elf_kind(e) != ELF_K_AR)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);

}

void
tcInvalidArInvalidSize(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("a too-small AR archive size is classified"
	    " as 'DATA'.");

	if ((e = elf_memory(ar_file, SARMAG-1)) == NULL ||
	    elf_kind(e) != ELF_K_NONE)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);
}

void
tcInvalidArSignature(void)
{
	Elf *e;
	char not_an_archive[sizeof(ar_file)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("invalid signature for an archive -> unrecognized");

	(void) memcpy(not_an_archive, ar_file, sizeof(not_an_archive));
	not_an_archive[0] = '~';

	if ((e = elf_memory(not_an_archive, sizeof(not_an_archive))) == NULL ||
	    elf_kind(e) != ELF_K_NONE)
		tet_result(TET_FAIL);

	(void) elf_end(e);
	tet_result(TET_PASS);
}

/*
 * TODO
 * - `ar' archives with an archive symbol table.
 */
