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
 * $Id: getscn.m4 1405 2011-02-05 07:53:03Z jkoshy $
 */

#include <ar.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

/*
 * A NULL argument is handled correctly.
 */
void
tcArgsNull(void)
{
	int error, result;
	Elf_Scn *scn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getscn(NULL,*) fails.");

	result = TET_PASS;
	if ((scn = elf_getscn(NULL, (size_t) 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	tet_result(result);
}

/*
 * elf_getscn(non-elf) fails.
 */

static char *nonelf = "This is not an ELF file.";

void
tcArgsNonElf(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getscn(non-elf) fails.");

	TS_OPEN_MEMORY(e, nonelf);

	result = TET_PASS;

	if ((scn = elf_getscn(e, (size_t) 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}

/*
 * elf_getscn works for all sections in a file.
 */

undefine(`FN')
define(`FN',`
void
tcElfAll$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, fd, result;
	size_t nsections, n, r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_getscn() can retrieve all sections.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &nsections) == 0) {
		TP_UNRESOLVED("elf_getshnum() failed.");
		goto done;
	}

	result = TET_PASS;

	for (n = 0; n < nsections; n++) {
		/* Retrieve the section ... */
		if ((scn = elf_getscn(e, n)) == NULL) {
			TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
			    error, elf_errmsg(error));
			break;
		}

		/* ... and verify that the section has the correct index. */
		if ((r = elf_ndxscn(scn)) != n) {
			TP_FAIL("scn=%p ndx %d != %d.", (void *) scn, r, n);
			break;
		}
	}

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * elf_getscn(e,nsections+1) returns NULL.
 */
undefine(`FN')
define(`FN',`
void
tcElfRange$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_getscn(elf,nsections+1) fails.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &n) == 0) {
		TP_UNRESOLVED("elf_getshnum() failed.");
		goto done;
	}

	result = TET_PASS;
	if ((scn = elf_getscn(e, n)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=\"%s\".", (void *) scn,
		    elf_errmsg(error));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * elf_getscn(e,*) fails with ELF_E_SECTION on malformed extended numbering.
 */
undefine(`FN')
define(`FN',`
void
tcExSecNumError$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_getscn() fails on a malformed file.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "xscn-1.$2$1", ELF_C_READ, fd, goto done;);

	result = TET_PASS;
	if ((scn = elf_getscn(e, 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_SECTION)
		TP_FAIL("scn=%p, error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * elf_getscn works correctly for a file with > SHN_XINDEX sections.
 */

undefine(`FN')
define(`FN',`
void
tcExSecNumLast$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	Elf$1_Shdr *sh;
	int error, fd, result;
	size_t n, r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_getscn() retrieves the last extended "
	    "section.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "xscn-2.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &n) == 0) {
		TP_UNRESOLVED("elf_getshnum() failed.");
		goto done;
	}

	result = TET_PASS;

	n--;

	/* Retrieve the section ... */
	if ((scn = elf_getscn(e, n)) == NULL) {
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));
		goto done;
	}

	/* ... and verify that the section has the correct index. */
	if ((r = elf_ndxscn(scn)) != n) {
		TP_FAIL("scn=%p ndx %d != %d.", (void *) scn, r, n);
		goto done;
	}

	/* ... and check the type of the section too. */
	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (sh->sh_type != SHT_STRTAB)
		TP_FAIL("section[%d] has wrong type %d.", n, sh->sh_type);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')
