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
 * $Id: ehdr.m4 3174 2015-03-27 17:13:41Z emaste $
 */

#include <gelf.h>
#include <libelf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

#include "elfts.h"

#include "gelf_ehdr_template.h"

include(`elfts.m4')

/*
 * A NULL `Elf *' argument returns an ELF_E_ARGUMENT error.
 */
undefine(`FN')dnl
define(`FN',`
void
tcGelfGetNullElf$1(void)
{
	void *eh;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr(NULL,ELFCLASS$2) fails with "
	    "ELF_E_ARGUMENT.");

	result = TET_PASS;
	if ((eh = gelf_newehdr(NULL,ELFCLASS$1)) != NULL ||
	   (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("eh=%p, error=\"%s\".", eh, elf_errmsg(error));

	tet_result(result);
}')

FN(`NONE')
FN(`32')
FN(`64')

/*
 * For a non-NULL but non Elf descriptor, the function should fail
 * with ELF_E_ARGUMENT.
 */
undefine(`FN')dnl
define(`FN',`
void
tcDataNonElfDesc$1(void)
{
	int error, result;
	void *eh;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr(E,ELFCLASS$1) for non-ELF (E) "
	    "fails with ELF_E_ARGUMENT.");

	TS_OPEN_MEMORY(e, data);

	result = TET_PASS;
	if ((eh = gelf_newehdr(e, ELFCLASS$1)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("eh=%p error=%d \"%s\".", (void *) eh,
		    error, elf_errmsg(error));

	(void) elf_end(e);
	tet_result(result);
}')

FN(`NONE')
FN(`32')
FN(`64')

/*
 * A valid Elf descriptor with of an unsupported version should
 * return an ELF_E_VERSION error.
 */
undefine(`FN')dnl
define(`FN',`
void
tcBadElfVersion$1$2(void)
{
	int err, result;
	Elf *e;
	void *eh;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr(E,ELFCLASS$1) with an unsupported "
	    "version fails with ELF_E_VERSION.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));

	badelf[EI_VERSION] = EV_NONE;
	badelf[EI_CLASS] = ELFCLASS$2;
	badelf[EI_DATA]  = ELFDATA2$1;

	TS_OPEN_MEMORY(e, badelf);

	result = TET_PASS;
	if ((eh = gelf_newehdr(e, ELFCLASS$2)) != NULL ||
	    (err = elf_errno()) != ELF_E_VERSION)
		TP_FAIL("eh=%p error=\"%s\".", (void *) eh, elf_errmsg(err));

	(void) elf_end(e);
	tet_result(result);
}')

FN(`LSB',`32')
FN(`LSB',`64')
FN(`MSB',`32')
FN(`MSB',`64')

/*
 * A malformed ELF descriptor should return an ELF_E_HEADER error.
 */
undefine(`FN')dnl
define(`FN',`
void
tcMalformedElf$1$2(void)
{
	int err, result;
	Elf *e;
	void *eh;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr($2,ELFCLASS$1) on a malformed "
	    "ELF header fails with ELF_E_HEADER.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));
	badelf[EI_VERSION] = EV_CURRENT;
	badelf[EI_CLASS]   = ELFCLASS$1;
	badelf[EI_DATA]	   = ELFDATA2$2;

	TS_OPEN_MEMORY(e, badelf);

	result = TET_PASS;
	if ((eh = gelf_newehdr(e, ELFCLASS$1)) != NULL ||
	    (err = elf_errno()) != ELF_E_HEADER)
		TP_FAIL("eh=%p error=\"%s\".", (void *) eh, elf_errmsg(err));

	(void) elf_end(e);
	tet_result(result);
}')

FN(`32',`LSB')
FN(`32',`MSB')
FN(`64',`LSB')
FN(`64',`MSB')

/*
 * Attempting to open pre-existing ELF file of the wrong class
 * should fail with ELF_E_CLASS.
 */
undefine(`FN')dnl
define(`FN',`
void
tcWrongElfClass$1$2(void)
{
	int error, fd, result;
	Elf$2_Ehdr *eh;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("A call to gelf_newehdr(ehdr.$1$3,ELFCLASS$2) "
	    "fails with ELF_E_CLASS");

	TS_OPEN_FILE(e, "ehdr.$1$3", ELF_C_READ, fd);

	result = TET_PASS;
	error = 0;
	eh = NULL;

	result = TET_PASS;
	if ((eh = (Elf$2_Ehdr *) gelf_newehdr(e, ELFCLASS$2)) != NULL ||
	    (error = elf_errno()) != ELF_E_CLASS)
		TP_FAIL("eh=%p, error=\"%s\".", (void *) eh, elf_errmsg(error));

	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}')

FN(`lsb',`32',`64')
FN(`lsb',`64',`32')
FN(`msb',`32',`64')
FN(`msb',`64',`32')

/*
 * Attempting to open a pre-existing ELF file of the correct class
 * should succeed.
 */
undefine(`FN')
define(`FN',`
void
tcElfValidClass$1$2(void)
{
	int fd, result;
	Elf$2_Ehdr *eh;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("a successful gelf_newehdr(ehdr.$1$2,"
	    "ELFCLASS$2) call returns the correct contents.");

	TS_OPEN_FILE(e, "ehdr.$1$2", ELF_C_READ, fd);

	result = TET_PASS;

	result = TET_PASS;
	if ((eh = (Elf$2_Ehdr *) gelf_newehdr(e, ELFCLASS$2)) == NULL) {
		TP_FAIL("gelf_newehdr(ehdr.$1$2) failed: %s.", elf_errmsg(-1));
		goto done;
	}

	CHECK_EHDR(eh, ELFDATA2`'TOUPPER($1), ELFCLASS$2);

 done:
	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}')

FN(`lsb',`32')
FN(`lsb',`64')
FN(`msb',`32')
FN(`msb',`64')

define(`TS_NEWELF',`"new.elf"')
define(`CHECK_NEWEHDR',`	do {
	if (($1)->e_ident[EI_MAG0] != ELFMAG0 ||
	    ($1)->e_ident[EI_MAG1] != ELFMAG1 ||
	    ($1)->e_ident[EI_MAG2] != ELFMAG2 ||
	    ($1)->e_ident[EI_MAG3] != ELFMAG3 ||
	    ($1)->e_ident[EI_CLASS] != ELFCLASS$2 ||
	    ($1)->e_ident[EI_DATA] != ELFDATANONE ||
	    ($1)->e_ident[EI_VERSION] != EV_CURRENT ||
	    ($1)->e_machine != EM_NONE ||
	    ($1)->e_type != ELF_K_NONE ||
	    ($1)->e_version != EV_CURRENT)
		TP_FAIL("gelf_getnewehdr(ELFCLASS$2) "
		    "header mismatch.");
} while (0)
')

/*
 * Retrieving the header from a new ELF file should return the
 * correct values.
 */

undefine(`FN')
define(`FN',`
void
tcNewElfExpected$1(void)
{
	int fd, result;
	Elf$1_Ehdr *eh;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr(new.elf,ELFCLASS$1) returns "
	  "the correct header.");

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	result = TET_PASS;
	if ((eh = (Elf$1_Ehdr *) gelf_newehdr(e, ELFCLASS$1)) == NULL) {
		TP_FAIL("gelf_newehdr("TS_NEWELF",ELFCLASS$1) failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	CHECK_NEWEHDR(eh, $1);

 done:
	(void) elf_end(e);
	(void) close(fd);
	(void) unlink(TS_NEWELF);
	tet_result(result);
}')

FN(`32')
FN(`64')

/*
 * Allocating a new Ehdr should mark it as dirty.
 */

undefine(`FN')
define(`FN',`
void
tcNewElfFlagDirty$1(void)
{
	Elf *e;
	Elf$1_Ehdr *eh;
	int fd, flags, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_newehdr("TS_NEWELF",ELFCLASS$1) marks "
	   "the header as dirty.");

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	if ((eh = (Elf$1_Ehdr *) gelf_newehdr(e, ELFCLASS$1)) == NULL) {
		TP_FAIL("gelf_newehdr("TS_NEWELF",ELFCLASS$1) failed: %s.",
		    elf_errmsg(-1));
		goto done;
	}

	flags = elf_flagehdr(e, ELF_C_CLR, 0);

	result = (flags & ELF_F_DIRTY) == 0 ? TET_FAIL : TET_PASS;

 done:
	(void) elf_end(e);
	(void) close(fd);
	(void) unlink(TS_NEWELF);

	tet_result(result);
}')

FN(`32')
FN(`64')

/*
 * Allocating and updating an Elf_Ehdr works correctly.
 */

define(`TS_REFELF',`"newehdr."')

undefine(`FN')
define(`FN',`
void
tcUpdateElf$1$2(void)
{
	Elf$2_Ehdr *eh;
	Elf *e;
	int fd, reffd, result;
	off_t offset;
	size_t fsz;
	void *t, *tref;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("The contents of the $1$2 Ehdr structure are "
	    "correctly updated.");

	t = tref = NULL;
	fd = reffd = -1;

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	result = TET_UNRESOLVED;

	if ((eh = (Elf$2_Ehdr *) gelf_newehdr(e, ELFCLASS$2)) == NULL) {
		TP_UNRESOLVED("gelf_newehdr(ELFCLASS$2) failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($1);

	if ((offset = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	fsz = gelf_fsize(e, ELF_T_EHDR, 1, EV_CURRENT);

	if (offset != fsz) {
		TP_FAIL("elf_update() -> %d, expected %d.", offset, fsz);
		goto done;
	}

	(void) elf_end(e);	e = NULL;
	(void) close(fd);	fd = -1;

	if ((t = malloc(fsz)) == NULL ||
	    (tref = malloc(fsz)) == NULL) {
		TP_UNRESOLVED("malloc(%d) failed.", fsz);
		goto done;
	}

	if ((fd = open(TS_NEWELF, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open("TS_NEWELF") failed: %s", strerror(errno));
		goto done;
	}

	if (read(fd, t, fsz) != fsz) {
		TP_UNRESOLVED("read(%d) failed: %s", fsz, strerror(errno));
		goto done;
	}

	if ((reffd = open(TS_REFELF "$1$2", O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open("TS_REFELF"$1$2) failed: %s.",
		    strerror(errno));
		goto done;
	}

	if (read(reffd, tref, fsz) != fsz) {
		TP_UNRESOLVED("read(%d) failed: %s.", fsz, strerror(errno));
		goto done;
	}

	result = TET_PASS;
	if (memcmp(t, tref, fsz) != 0)
		TP_FAIL("memcmp("TS_NEWELF","TS_REFELF"$1$2) failed.");

 done:
	(void) unlink(TS_NEWELF);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (reffd != -1)
		(void) close(reffd);
	if (t)
		free(t);
	if (tref)
		free(tref);
	tet_result(result);
}')

FN(`lsb',`32')
FN(`lsb',`64')
FN(`msb',`32')
FN(`msb',`64')
