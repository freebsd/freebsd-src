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
 * $Id: strptr.m4 1380 2011-01-23 07:21:25Z jkoshy $
 */

#include <errno.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>

#include "tet_api.h"

#include "elfts.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

/*
 * A Null ELF value is rejected.
 */

void
tcArgsNull(void)
{
	int error, result;
	char *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_strptr(NULL,*,*) fails.");

	result = TET_PASS;
	if ((r = elf_strptr(NULL, (size_t) 0, (size_t) 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p error=%d \"%s\".", r, error, elf_errmsg(error));

	tet_result(result);
}

/*
 * An illegal section index is rejected.
 */
undefine(`FN')
define(`FN',`
void
tcArgsIllegalSection$1`'TOUPPER($2)(void)
{
	int error, fd, result;
	Elf *e;
	char *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: a non-STRTAB section is rejected.");

	_TS_OPEN_FILE(e, "$3.$2$1", ELF_C_READ, fd, goto done;);

	result = TET_PASS;
	if ((r = elf_strptr(e, SHN_UNDEF, (size_t) 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p error=%d \"%s\".", r, error,
		    elf_errmsg(error));

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(32,`lsb',`newscn')
FN(32,`msb',`newscn')
FN(64,`lsb',`newscn')
FN(64,`msb',`newscn')

/*
 * An invalid section offset is rejected.
 */

undefine(`FN')
define(`FN',`
void
tcArgsIllegalOffset$1`'TOUPPER($2)(void)
{
	Elf *e;
	char *r;
	Elf_Scn *scn;
	Elf$1_Ehdr *eh;
	Elf$1_Shdr *sh;
	int error, fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: invalid offsets are rejected.");

	_TS_OPEN_FILE(e, "$3.$2$1", ELF_C_READ, fd, goto done;);

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_getehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_getscn(e, eh->e_shstrndx)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() faied: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if ((r = elf_strptr(e, eh->e_shstrndx, sh->sh_size)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p error=%d \"%s\".", (void *) r, error,
		    elf_errmsg(error));

	/* Try a very large value */
	if ((r = elf_strptr(e, eh->e_shstrndx, ~ (size_t) 0)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p error=%d \"%s\".", (void *) r, error,
		    elf_errmsg(error));

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(32,`lsb',`newscn')
FN(32,`msb',`newscn')
FN(64,`lsb',`newscn')
FN(64,`msb',`newscn')

/*
 * A section index inside a 'hole' is rejected.
 */

static char teststring[] = {
	'a', 'b', 'c', 'd', '\0'
};

undefine(`FN')
define(`FN',`
void
tcArgsOffsetInHole$1`'TOUPPER($2)(void)
{
	int error, fd, result;
	Elf *e;
	size_t sz;
	Elf_Scn *scn;
	Elf_Data *d;
	Elf$1_Shdr *sh;
	Elf$1_Ehdr *eh;
	char *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: invalid offsets are rejected.");

	_TS_OPEN_FILE(e, "$3.$2$1", ELF_C_READ, fd, goto done;);

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_getehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_getscn(e, eh->e_shstrndx)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr(): failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Get the current size of the section. */
	sz = sh->sh_size;

	/* Add a new data descriptor to the section. */
	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_align  = 512;
	d->d_buf  = teststring;
	d->d_size = sizeof(teststring);

	/* Resync. */
	if (elf_update(e, ELF_C_NULL) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;

	/* first byte offset in the "hole". */
	if ((r = elf_strptr(e, eh->e_shstrndx, sz)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p offset=%d error=%d \"%s\".", (void *) r, sz,
		    error, elf_errmsg(error));

	/* last offset in the "hole". */
	if ((r = elf_strptr(e, eh->e_shstrndx, (size_t) d->d_align - 1)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p offset=%d error=%d \"%s\".", (void *) r, (d->d_align-1),
		    error, elf_errmsg(error));

	/* offset after the new end of the section. */
	if ((r = elf_strptr(e, eh->e_shstrndx, (size_t) d->d_align + d->d_size)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("r=%p offset=%d error=%d \"%s\".", (void *) r,
		    (d->d_align+d->d_size), error, elf_errmsg(error));

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(32,`lsb',`newscn')
FN(32,`msb',`newscn')
FN(64,`lsb',`newscn')
FN(64,`msb',`newscn')

/*
 * Check that all strings have their correct offsets.
 */

struct refstr {
	size_t offset;
	char   *string;
} refstr[] = {
	/* From the newscn.* file. */
	{ .offset = 0, .string = "" },
	{ .offset = 1, .string = ".shstrtab" },
	{ .offset = 11, .string = ".foobar" },
#define NSTATIC 3
	 /* added by test case() */
	{ .offset = 512, .string = "abcd" }
};

undefine(`FN')
define(`FN',`
void
tcArgsValidOffset$1`'TOUPPER($2)(void)
{
	int error, fd, result;
	Elf *e;
	Elf_Scn *scn;
	Elf_Data *d;
	Elf$1_Shdr *sh;
	Elf$1_Ehdr *eh;
	char *r;
	struct refstr *rs;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: invalid offsets are rejected.");

	_TS_OPEN_FILE(e, "$3.$2$1", ELF_C_READ, fd, goto done;);

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_getehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_getscn(e, eh->e_shstrndx)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr(): failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	for (rs = refstr; rs < &refstr[NSTATIC]; rs++)
		if ((r = elf_strptr(e, eh->e_shstrndx, rs->offset)) == NULL ||
		    strcmp(r, rs->string) != 0) {
			TP_FAIL("r=\"%s\" rs=\"%s\" offset=%d error=\"%s\".",
			    r, rs->string, rs->offset, elf_errmsg(-1));
			goto done;
		}

	/* Add a new data descriptor to the section. */
	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_align  = 512;
	d->d_buf  = teststring;
	d->d_size = sizeof(teststring);

	/* Resync. */
	if (elf_update(e, ELF_C_NULL) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;

	/* first byte offset in the "hole". */
	if ((r = elf_strptr(e, eh->e_shstrndx, rs->offset)) == NULL ||
	    strcmp(r, rs->string) != 0)
		TP_FAIL("r=\"%s\" rs=\"%s\" offset=%d error=\"%s\".", r,
		    rs->string, rs->offset, elf_errmsg(error));

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(32,`lsb',`newscn')
FN(32,`msb',`newscn')
FN(64,`lsb',`newscn')
FN(64,`msb',`newscn')

/*
 * TODO: With the layout bit set, an out of bounds offset is detected.
 */

/*
 * TODO: With the layout bit set, strings are correctly retrieved.
 */

