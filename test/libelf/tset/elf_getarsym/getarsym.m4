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
 * $Id: getarsym.m4 1407 2011-02-05 08:31:07Z jkoshy $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <ar.h>
#include <errno.h>
#include <libelf.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')
/*
 * The following defines should match that in `./Makefile'.
 */
define(`TP_ELFFILE',`"a1.o"')
define(`TP_ARFILE_BSD', `"a-bsd.ar"')
define(`TP_ARFILE_NOSYMTAB_BSD',`"a2-bsd.ar"')
define(`TP_ARFILE_SVR4', `"a.ar"')
define(`TP_ARFILE_NOSYMTAB_SVR4',`"a2.ar"')
define(`TP_NSYMBOLS',`3')

/*
 * A NULL `Elf' argument fails.
 */
void
tcArgsNull(void)
{
	int error, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarhdr(NULL) fails.");

	result = TET_PASS;
	n = ~(size_t) 0;
	if (elf_getarsym(NULL, &n) != NULL ||
	    (n != (size_t) 0) || (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("n=%d error=%d \"%s\".", n, error, elf_errmsg(error));

	tet_result(result);
}

/*
 * elf_getarsym() on a non-Ar file fails.
 */
static char *nonar = "This is not an AR file.";

void
tcArgsNonAr(void)
{
	Elf *e;
	int error, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarsym(non-ar) fails.");

	TS_OPEN_MEMORY(e, nonar);

	result = TET_PASS;

	n = ~ (size_t) 0;
	if (elf_getarsym(e, &n) != NULL || (n != (size_t) 0) ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("error=%d \"%s\".", error, elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}

/*
 * elf_getarsym() on a top-level ELF file fails.
 */

void
tcArgsElf(void)
{
	Elf *e;
	int error, fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarsym(elf) fails.");

	TS_OPEN_FILE(e, TP_ELFFILE, ELF_C_READ, fd);

	result = TET_PASS;

	n = ~ (size_t) 0;
	if (elf_getarsym(e, &n) != NULL || (n != (size_t) 0) ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("error=%d \"%s\".", error, elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}

/* This list of symbols must match the order of the files in test archive. */
struct refsym {
	char *as_name;
	unsigned long as_hash;
	char *as_object;
	int as_found;
};

struct refsym refsym[] = {
	{ .as_name = "a1", .as_hash = 0x641, .as_object = "a1.o" },
	{ .as_name = "a2", .as_hash = 0x642, .as_object = "a2.o" },
	{ .as_name = NULL }
};


define(`ARCHIVE_TESTS',`
/*
 * elf_getarsym() on an ar archive succeeds.
 */

void
tcArAr$1(void)
{
	Elf *e;
	Elf_Arsym *arsym;
	int fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarsym(ar-descriptor)/$1 succeeds.");

	TS_OPEN_FILE(e, TP_ARFILE_$1, ELF_C_READ, fd);

	result = TET_PASS;
	n = ~ (size_t) 0;
	if ((arsym = elf_getarsym(e, &n)) == NULL ||
	    n != TP_NSYMBOLS)
		TP_FAIL("error=\"%s\".", elf_errmsg(-1));

	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}

/*
 * Two elf_getarsym invocations return the same value.
 */

void
tcArDup$1(void)
{
	Elf *e;
	Elf_Arsym *arsym, *t;
	int fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("duplicate elf_getarsym()/$1 calls return the "
		"same value.");

	TS_OPEN_FILE(e, TP_ARFILE_$1, ELF_C_READ, fd);

	result = TET_PASS;
	n = ~ (size_t) 0;
	if ((arsym = elf_getarsym(e, &n)) == NULL ||
	    n != TP_NSYMBOLS) {
		TP_FAIL("error=\"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((t = elf_getarsym(e, &n)) == NULL ||
	    n != TP_NSYMBOLS) {
		TP_FAIL("error=\"%s\".", elf_errmsg(-1));
		goto done;
	}

	if (t != arsym)
		TP_FAIL("return values differ.");

done:
	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}

/*
 * elf_getarsym() on an ar archive without a symbol table fails.
 */

void
tcArNoSymtab$1(void)
{
	Elf *e;
	size_t n;
	Elf_Arsym *arsym;
	int fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarsym(ar-with-no-symtab)/$1 fails.");

	TS_OPEN_FILE(e, TP_ARFILE_NOSYMTAB_$1, ELF_C_READ, fd);

	result = TET_PASS;
	n = ~ (size_t) 0;
	if ((arsym = elf_getarsym(e, &n)) != NULL ||
	    n != 0)
		TP_FAIL("arsym=%p n=%d.", (void *) arsym, n);

	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}

/*
 * elf_getarsym() on ar archive members succeed.
 */

void
tcArArSym$1(void)
{
	Elf_Arhdr *arh;
	Elf *ar_e, *e;
	Elf_Arsym *arsym;
	off_t offset;
	int c, fd, result;
	struct refsym *r;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getarsym()/$1 returns a correct list of symbols.");

	ar_e = e = NULL;
	c = ELF_C_READ;
	fd = -1;

	TS_OPEN_FILE(ar_e, TP_ARFILE_$1, c, fd);

	result = TET_PASS;

	if ((arsym = elf_getarsym(ar_e, &n)) == NULL ||
	    (n != TP_NSYMBOLS)) {
		TP_FAIL("elf_getarsym() failed: n=%d error=\"%s\".", n,
		    elf_errmsg(-1));
		goto done;
	}

	for (; arsym->as_name; arsym++) {

		/* Lookup this symbol in the reference table */
		c = 0;
		for (r = refsym; r->as_name; r++) {
			if (strcmp(r->as_name, arsym->as_name) == 0 &&
			    r->as_hash == arsym->as_hash) {
				r->as_found = c = 1;
				break;
			}
		}

		if (c == 0) {
			TP_FAIL("extra symbol \"%s\".", arsym->as_name);
			goto done;
		}

		if ((offset = elf_rand(ar_e, arsym->as_off)) != arsym->as_off) {
			TP_FAIL("elf_rand(%jd) failed: \"%s\".",
			    (intmax_t) arsym->as_off, elf_errmsg(-1));
			goto done;
		}

		if ((e = elf_begin(fd, ELF_C_READ, ar_e)) == NULL) {
			TP_UNRESOLVED("elf_begin() failed: \"%s\".",
			    elf_errmsg(-1));
			goto done;
		}

		if ((arh = elf_getarhdr(e)) == NULL) {
			TP_UNRESOLVED("elf_getarhdr() failed: \"%s\".",
			    elf_errmsg(-1));
			goto done;
		}

		if (strcmp(arh->ar_name, r->as_object) != 0) {
			TP_FAIL("object-name \"%s\" != ref \"%s\".",
			    arh->ar_name, r->as_name);
			goto done;
		}

		(void) elf_end (e);
		e = NULL;
	}

	/* Check the last entry */
	if (arsym->as_name != NULL || arsym->as_hash != ~0UL ||
	    arsym->as_off != (off_t) 0) {
		TP_FAIL("last entry mangled.");
		goto done;
	}

	/* Check that all names have been retrieved. */
	for (r = refsym; r->as_name; r++) {
		if (r->as_found == 0) {
			TP_FAIL("symbol \"%s\" was not present.", r->as_name);
			break;
		}
	}

 done:
	if (e)
		(void) elf_end(e);
	if (ar_e)
		(void) elf_end(ar_e);
	if (fd != -1)
	        (void) close(fd);

	tet_result(result);

}
')

ARCHIVE_TESTS(`SVR4')
ARCHIVE_TESTS(`BSD')
