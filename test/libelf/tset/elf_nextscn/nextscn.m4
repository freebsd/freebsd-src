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
 * $Id: nextscn.m4 1385 2011-01-23 15:10:19Z jkoshy $
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
 * Null arguments are handled correctly.
 */
void
tcArgsNull(void)
{
	int error, result;
	Elf_Scn *scn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(NULL,*) fails.");

	result = TET_PASS;
	if ((scn = elf_nextscn(NULL, NULL)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	tet_result(result);
}

/*
 * elf_nextscn(non-elf) fails.
 */

static char *nonelf = "This is not an ELF file.";

void
tcArgsNonElf(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(non-elf) fails.");

	TS_OPEN_MEMORY(e, nonelf);

	result = TET_PASS;

	if ((scn = elf_nextscn(e, NULL)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}

/*
 * elf_nextscn(e,NULL) returns section number 1.
 */
undefine(`FN')
define(`FN',`
void
tcElfSuccess$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_nextscn(elf,NULL) succeeds.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	result = TET_PASS;
	if ((scn = elf_nextscn(e, NULL)) == NULL) {
		TP_FAIL("elf_newscn() failed: error=\"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((n = elf_ndxscn(scn)) != 1)
		TP_FAIL("elf_nextscn() returned index %d.", n);

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
 * elf_nextscn(new-elf, last-section) returns NULL and no error.
 */

undefine(`FN')
define(`FN',`
void
tcElfLastNewFile$1(void)
{
	Elf *e;
	Elf_Scn *scn, *nextscn;
	Elf$1_Ehdr *eh;
	int error, fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(newelf,last-scn) returns NULL.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;


	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed.");
		goto done;
	}

	(void) elf_errno();

	result = TET_PASS;
	if ((nextscn = elf_nextscn(e, scn)) != NULL ||
	    (error = elf_errno()) != ELF_E_NONE)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(32)
FN(64)


/*
 * elf_nextscn(old-elf, last-section) returns NULL and no error.
 */

undefine(`FN')
define(`FN',`
void
tcElfLastOldFile$2$1(void)
{
	Elf *e;
	Elf_Scn *scn, *nextscn;
	int error, fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(oldelf,last-scn) returns NULL.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &n) == 0) {
		TP_UNRESOLVED("elf_getshnum() failed.");
		goto done;
	}

	if ((scn = elf_getscn(e, n - 1)) == NULL) {
		TP_UNRESOLVED("elf_getscn(%d) failed.", (n-1));
		goto done;
	}

	(void) elf_errno();

	result = TET_PASS;
	if ((nextscn = elf_nextscn(e, scn)) != NULL ||
	    (error = elf_errno()) != ELF_E_NONE)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
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
 * elf_nextscn(iterates through sections in ascending order.
 */

undefine(`FN')
define(`FN',`
void
tcElfAscending$2$1(void)
{
	Elf *e;
	Elf_Scn *scn, *oldscn;
	int error, fd, result;
	size_t nsections, n, r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(elf,last-scn) returns NULL.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &nsections) == 0) {
		TP_UNRESOLVED("elf_getshnum() failed.");
		goto done;
	}

	if ((oldscn = elf_getscn(e, 0)) == NULL) {
		TP_UNRESOLVED("elf_getscn(0) failed.");
		goto done;
	}

	result = TET_PASS;

	for (n = 0; n < nsections-1; n++) {
		if ((scn = elf_nextscn(e, oldscn)) == NULL) {
			TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
			    error, elf_errmsg(error));
			goto done;
		}

		if ((r = elf_ndxscn(scn)) != n+1) {
			TP_FAIL("scn=%p ndx %d != %d.", (void *) scn, r, n+1);
			goto done;
		}
		oldscn = scn;
	}

	/* check the last one */
	if ((scn = elf_nextscn(e, oldscn)) != NULL ||
	    (r = elf_ndxscn(oldscn)) != (nsections-1))
		TP_FAIL("scn=%p r=%d", (void *) scn, r);

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
 * elf_nextscn() returns an error on mismatched Elf,Scn.
 */

undefine(`FN')
define(`FN',`
void
tcElfMismatch$2$1(void)
{
	Elf *e1, *e2;
	Elf_Scn *scn, *nextscn;
	int error, fd1, fd2, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_nextscn(e1,scn-not-of-e1) fails.");

	e1 = e2 = NULL;
	fd1 = fd2 = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e1, "newscn.$2$1", ELF_C_READ, fd1, goto done;);
	_TS_OPEN_FILE(e2, TS_NEWFILE, ELF_C_WRITE, fd2, goto done;);

	if ((scn = elf_getscn(e1, 0)) == NULL) {
		TP_UNRESOLVED("elf_getscn(%d) failed.", (n-1));
		goto done;
	}

	result = TET_PASS;
	if ((nextscn = elf_nextscn(e2, scn)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

 done:
	if (e1)
		(void) elf_end(e1);
	if (e2)
		(void) elf_end(e2);
	if (fd1 != -1)
		(void) close(fd1);
	if (fd2 != -1)
		(void) close(fd2);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')
