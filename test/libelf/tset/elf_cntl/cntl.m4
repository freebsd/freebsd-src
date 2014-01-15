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
 * $Id: cntl.m4 2191 2011-11-21 08:34:02Z jkoshy $
 */

#include <sys/types.h>

#include <libelf.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

/*
 * Test the `elf_cntl' API.
 */

static char elf_file[] = "\177ELF\001\001\001	\001\000\000\000\000"
	"\000\000\000\001\000\003\000\001\000\000\000\357\276\255\336"
	"\000\000\000\000\000\000\000\000\003\000\000\0004\000 \000"
	"\000\000(\000\000\000\000\000";

/*
 * A NULL elf parameter causes elf_cntl() to fail.
 */
void
tcInvalidNull(void)
{
	int error, result, ret;

	TP_ANNOUNCE("elf_cntl(NULL,...) fails with ELF_E_ARGUMENT.");

	TP_CHECK_INITIALIZATION();

	ret = error = 0;
	result = TET_PASS;
	if ((ret = elf_cntl(NULL, ELF_C_FDREAD)) != -1 ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("ret=%d, error=%d \"%s\".", ret, error);

	tet_result(result);
}

/* 
 * Invalid `cmd' values are rejected.
 */
void
tcInvalidInvalid(void)
{
	Elf *e;
	int c, error, result, ret;

	TP_ANNOUNCE("elf_cntl(e,[INVALID]) fails with ELF_E_ARGUMENT.");

	TP_CHECK_INITIALIZATION();

	TS_OPEN_MEMORY(e, elf_file);

	ret = error = 0;
	result = TET_PASS;
	for (c = ELF_C_FIRST-1; c <= ELF_C_LAST; c++) {
		if (c == ELF_C_FDDONE || c == ELF_C_FDREAD)
			continue;
		if ((ret = elf_cntl(e, c)) != -1 ||
		    (error = elf_errno()) != ELF_E_ARGUMENT) {
			TP_FAIL("returned (%d) / error (%d) for "
			    "c = %d.", ret, error, c);
			break;
		}
	}

	(void) elf_end(e);
	tet_result(result);
}

/*
 * Calling elf_cntl(FDREAD) for files opened in read mode.
 */
void
tcReadFDREAD(void)
{
	Elf *e;
	int result;

	TP_ANNOUNCE("elf_cntl(e,FDREAD) for a read-only descriptor succeeds.");

	TP_CHECK_INITIALIZATION();

	TS_OPEN_MEMORY(e, elf_file);

	result = TET_PASS;
	if (elf_cntl(e, ELF_C_FDREAD) != 0)
		TP_FAIL("elf_errmsg=\"%s\".", elf_errmsg(-1));

	(void) elf_end(e);
	tet_result(result);
}

static char pathname[PATH_MAX];

/*
 * elf_cntl(FDREAD) doesn't make sense for a descriptor opened
 * for writing.
 */
void
tcWriteFDREAD(void)
{
	Elf *e;
	int err, fd, result, ret;

	TP_ANNOUNCE("elf_cntl(e,FDREAD) for a descriptor opened for write "
	    "fails with ELF_E_MODE.");

	TP_CHECK_INITIALIZATION();

	(void) strncpy(pathname, "/tmp/TCXXXXXX", sizeof(pathname));
	pathname[sizeof(pathname) - 1] = '\0';

	if ((fd = mkstemp(pathname)) == -1 ||
	    (e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		TP_UNRESOLVED("elf_begin(%d,WRITE,NULL) failed.");
		goto done;
	}

	result = TET_PASS;
	if ((ret = elf_cntl(e, ELF_C_FDREAD)) != -1 ||
	    (err = elf_errno()) != ELF_E_MODE)
		TP_FAIL("ret (%d) error (%d).", ret, err);

 done:
	(void) elf_end(e);
	(void) unlink(pathname);
	tet_result(result);
}

/*
 * An elf_cntl(FDDONE) causes a subsequent elf_update(WRITE) to fail.
 */

void
tcWriteFDDONE(void)
{
	Elf *e;
	Elf32_Ehdr *eh;
	int err, fd, result;
	off_t ret;

	TP_ANNOUNCE("elf_cntl(e,FDDONE) makes a subsequent "
	    "elf_update(ELF_C_WRITE) fail with ELF_E_SEQUENCE.");

	TP_CHECK_INITIALIZATION();

	(void) strncpy(pathname, "/tmp/TCXXXXXX", sizeof(pathname));
	pathname[sizeof(pathname) - 1] = '\0';

	if ((fd = mkstemp(pathname)) == -1 ||
	    (e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		TP_UNRESOLVED("elf_begin(%d,WRITE,NULL) failed.");
		goto done;
	}

	if (elf_cntl(e, ELF_C_FDDONE) == -1) {
		TP_FAIL("elf_cntl(e,FDONE) failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if (elf_flagelf(e, ELF_C_SET, ELF_F_DIRTY) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf32_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf32_newehdr() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if ((ret = elf_update(e, ELF_C_WRITE)) != (off_t) -1 ||
	    (err = elf_errno()) != ELF_E_SEQUENCE)
		TP_FAIL("ret (%jd) err (%d).", (uint64_t) ret, err);

 done:
	tet_result(result);

	(void) elf_end(e);
	(void) unlink(pathname);
}
