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
 * $Id: begin.m4 2933 2013-03-30 01:33:02Z jkoshy $
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

#include "elfts.h"

include(`elfts.m4')

define(`TS_ARFILE_BSD',`"a-bsd.ar"')
define(`TS_ARFILE_SVR4',`"a.ar"')

/*
 * Test the `elf_begin' entry point.
 */

/*
 * Calling elf_begin() before elf_version() results in ELF_E_SEQUENCE.
 * Note that these test cases should run as a separate invocation than
 * the others since they need to be run before elf_version() is called.
 */
undefine(`FN')
define(`FN',`
void
tcSequenceUninitialized$1(void)
{
	Elf *e;
	int error, result;

	TP_ANNOUNCE("elf_version() needs to be set before "
	    "using the elf_begin($1) API.");

	result = TET_PASS;
	if ((e = elf_begin(-1, ELF_C_$1, NULL)) != NULL ||
	    (error = elf_errno()) != ELF_E_SEQUENCE)
		TP_FAIL("ELF_C_$1: e=%p error=%d \"%s\".", (void *) e, error,
		    elf_errmsg(error));

	tet_result(result);
}')

FN(`NULL')
FN(`READ')
FN(`WRITE')
FN(`RDWR')

void
tcCmdInvalid(void)
{
	Elf *e;
	int c, error, result;

	TP_ANNOUNCE("An invalid cmd value returns ELF_E_ARGUMENT.");

	TP_SET_VERSION();

	result = TET_PASS;
	for (c = ELF_C_NULL-1; c <= ELF_C_NUM; c++) {
		if (c == ELF_C_READ || c == ELF_C_WRITE || c == ELF_C_RDWR ||
		    c == ELF_C_NULL)
			continue;
		if ((e = elf_begin(-1, c, NULL)) != NULL ||
		    (error = elf_errno()) != ELF_E_ARGUMENT) {
			TP_FAIL("cmd=%d: e=%p error=%d .", c,
			    (void *) e, error);
			break;
		}
	}

 done:
	tet_result(result);
}

void
tcCmdNull(void)
{
	Elf *e;
	int result;

	TP_ANNOUNCE("cmd == ELF_C_NULL returns NULL.");

	TP_SET_VERSION();

	result = (e = elf_begin(-1, ELF_C_NULL, NULL)) != NULL ? TET_FAIL :
	    TET_PASS;

 done:
	tet_result(result);
}


/*
 * Verify that opening non-regular files fail with ELF_E_ARGUMENT
 */
undefine(`FN')
define(`FN',`
void
tcNonRegular$1(void)
{
	Elf *e;
	int error, fd, result;

	e = NULL;
	fd = -1;
	result = TET_FAIL;

	TP_ANNOUNCE("opening a $3 fails with ELF_E_ARGUMENT.");

	TP_SET_VERSION();

	if ((fd = open("$2", O_RDONLY)) < 0) {
		TP_UNRESOLVED("open \"$2\" failed: %s", strerror(errno));
		goto done;
	}

	e = elf_begin(fd, ELF_C_READ, NULL);

	if (e == NULL && (error = elf_errno()) == ELF_E_ARGUMENT)
		result = TET_PASS;	/* Verify the error. */

 done:
	if (e)
		elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(`DeviceFile', `/dev/null', `device file')
FN(`Directory', `.', `directory')

/*
 * Verify that for command modes ELF_C_READ and ELF_C_RDWR, opening
 * a zero sized regular file fails with ELF_E_ARGUMENT.
 */

undefine(`FN',`ZERO')
define(`ZERO',`"zero"')
define(`FN',`
void
tcZero$1(void)
{
	Elf *e;
	int error, fd, result;

	e = NULL;
	fd = -1;
	result = TET_FAIL;

	TP_ANNOUNCE("opening an zero-sized file in mode ELF_C_$1 fails "
	            "with ELF_E_ARGUMENT.");

	TP_SET_VERSION();

	if ((fd = open(ZERO, O_RDONLY)) < 0) {
		TP_UNRESOLVED("open \"$2\" failed: %s", strerror(errno));
		goto done;
	}

	e = elf_begin(fd, ELF_C_$1, NULL);

	if (e == NULL && (error = elf_errno()) == ELF_E_ARGUMENT)
		result = TET_PASS;	/* Verify the error. */

 done:
	if (e)
		elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(`READ')
FN(`RDWR')

#define	TEMPLATE	"TCXXXXXX"
#define	FILENAME_SIZE	16
char	filename[FILENAME_SIZE];

int
setup_tempfile(void)
{
	int fd;

	(void) strncpy(filename, TEMPLATE, sizeof(filename));
	filename[sizeof(filename) - 1] = '\0';

	if ((fd = mkstemp(filename)) < 0 ||
	    write(fd, TEMPLATE, sizeof(TEMPLATE)) < 0)
		return 0;

	(void) close(fd);

	return 1;

}

void
cleanup_tempfile(void)
{
	(void) unlink(filename);
}


define(`FN',`
void
tcCmdWriteFdRead_$1(void)
{
	Elf *e;
	Elf$1_Ehdr *eh;
	int error, fd, result;

	TP_ANNOUNCE("($1): cmd == ELF_C_WRITE fails with a non-writable FD.");

	TP_SET_VERSION();

	if (setup_tempfile() == 0 ||
	    (fd = open(filename, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("setup failed: %s", strerror(errno));
		goto done;
	}

	result = TET_PASS;
	error = -1;

	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		TP_UNRESOLVED("elf_begin() failed: %s", elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_getehdr() failed: %s", elf_errmsg(-1));
		goto done;
	}

	/* Verify that elf_update() fails with the appropriate error. */
	if (elf_update(e, ELF_C_WRITE) >= 0) {
		TP_FAIL("fn=%s, elf_update() succeeded unexpectedly.",
		    filename);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_IO)
		TP_FAIL("fn=%s, error=%d \"%s\".", filename, error,
		    elf_errmsg(error));

 done:
	cleanup_tempfile();
	tet_result(result);
}')

FN(32)
FN(64)

void
tcCmdWriteFdRdwr(void)
{
	Elf *e;
	int error, fd, result;

	TP_ANNOUNCE("cmd == ELF_C_WRITE on an 'rdwr' FD passes.");

	TP_SET_VERSION();

	if (setup_tempfile() == 0 ||
	    (fd = open(filename, O_RDWR, 0)) < 0) {
		TP_UNRESOLVED("setup failed: %s", strerror(errno));
		goto done;
	}

	result = TET_PASS;
	error = -1;
	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		error = elf_errno();
		TP_FAIL("fn=%s, error=%d \"%s\"", filename, error,
		    elf_errmsg(error));
	}

 done:
	cleanup_tempfile();
	tet_result(result);
}

void
tcCmdWriteFdWrite(void)
{
	Elf *e;
	int error, fd, result;

	TP_ANNOUNCE("cmd == ELF_C_WRITE on write-only FD passes.");

	TP_SET_VERSION();

	if (setup_tempfile() == 0 ||
	    (fd = open(filename, O_WRONLY, 0)) < 0) {
		TP_UNRESOLVED("setup failed: %s", strerror(errno));
		goto done;
	}

	result = TET_PASS;
	error = -1;
	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		error = elf_errno();
		TP_FAIL("fn=%s, error=%d \"%s\"", filename,
		    error, elf_errmsg(error));
	}

 done:
	cleanup_tempfile();
	tet_result(result);
}

void
tcCmdWriteParamIgnored(void)
{
	Elf *e, *t;
	int error, fd, fd1, result;

	TP_ANNOUNCE("cmd == ELF_C_WRITE ignores the last parameter.");

	TP_SET_VERSION();

	if (setup_tempfile() == 0 ||
	    (fd = open(filename, O_WRONLY, 0)) < 0 ||
	    (fd1 = open(filename, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("setup failed: %s", strerror(errno));
		goto done;
	}

	if ((t = elf_begin(fd1, ELF_C_READ, NULL)) == NULL) {
		TP_UNRESOLVED("elf_begin() failed unexpectedly: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	error = -1;
	if ((e = elf_begin(fd, ELF_C_WRITE, t)) == NULL) {
		TP_FAIL("elf_begin() failed: \"%s\".", elf_errmsg(-1));
	}

 done:
	cleanup_tempfile();
	tet_result(result);
}


/*
 * Check that opening various classes/endianness of ELF files
 * passes.
 */
undefine(`FN')
define(`FN',`
void
tcElfOpen$1$2(void)
{
	Elf *e;
	int fd, result;
	char *p;

	TP_ANNOUNCE("open(ELFCLASS$1,ELFDATA2`'TOUPPER($2)) succeeds.");

	TP_SET_VERSION();

	fd = -1;
	e = NULL;
	result = TET_UNRESOLVED;

	if ((fd = open ("check_elf.$2$1", O_RDONLY)) < 0) {
		TP_UNRESOLVED("open() failed: %s.", strerror(errno));
		goto done;
	}

	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		TP_FAIL("elf_begin() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((p = elf_getident(e, NULL)) == NULL) {
		TP_FAIL("elf_getident() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if (p[EI_CLASS] != ELFCLASS$1 ||
	    p[EI_DATA] != ELFDATA2`'TOUPPER($2))
		TP_FAIL("class %d expected %d, data %d expected %d.",
		    p[EI_CLASS], ELFCLASS$1, p[EI_DATA], ELFDATA2`'TOUPPER($2));
	else
		result = TET_PASS;

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
 * Check an `fd' mismatch is detected.
 */
void
tcFdMismatch(void)
{
	Elf *e, *e2;
	int error, fd, result;

	TP_ANNOUNCE("an fd mismatch is detected.");

	TP_SET_VERSION();

	e = e2 = NULL;
	fd = -1;

	if ((fd = open("check_elf.msb32", O_RDONLY)) < 0 ||
	    (e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		TP_UNRESOLVED("open(check_elf) failed: fd=%d.", fd);
		goto done;
	}

	result = TET_PASS;

	if ((e2 = elf_begin(fd+1, ELF_C_READ, e)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("elf_begin(%d+1) -> %p, error=%d \"%s\".", fd,
		    (void *) e2, error, elf_errmsg(error));
 done:
	if (e)
		(void) elf_end(e);
	if (e2)
		(void) elf_end(e2);
	if (fd >= 0)
		(void) close(fd);
	tet_result(result);
}

undefine(`ARFN')
define(`ARFN',`
/*
 * Check that an $1-style AR archive detects a cmd mismatch.
 */
void
tcArCmdMismatchRDWR_$1(void)
{
	Elf *e, *e2;
	int error, fd, result;

	TP_ANNOUNCE("($1): a cmd mismatch is detected.");

	TP_SET_VERSION();

	result = TET_UNRESOLVED;
	e = e2 = NULL;
	fd = -1;

	/* Open the archive with ELF_C_READ. */
	_TS_OPEN_FILE(e, TS_ARFILE_$1, ELF_C_READ, fd, goto done;);

	/* Attempt to iterate through it with ELF_C_RDWR. */
	result = TET_PASS;
	if ((e2 = elf_begin(fd, ELF_C_RDWR, e)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("e2=%p error=%d \"%s\".", (void *) e2,
		    error, elf_errmsg(error));
 done:
	if (e)
		(void) elf_end(e);
	if (e2)
		(void) elf_end(e2);
	if (fd >= 0)
		(void) close(fd);
	tet_result(result);
}

/*
 * Check that a member is correctly retrieved for $1-style archives.
 */
void
tcArRetrieval_$1(void)
{
	Elf *e, *e1;
	int fd, result;
	Elf_Kind k;

	TP_ANNOUNCE("($1): an archive member is correctly retrieved.");

	TP_SET_VERSION();

	e = e1 = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, TS_ARFILE_$1, ELF_C_READ, fd, goto done;);

	result = TET_PASS;
	if ((e1 = elf_begin(fd, ELF_C_READ, e)) == NULL) {
		TP_FAIL("elf_begin() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((k = elf_kind(e1)) != ELF_K_ELF)
		TP_FAIL("kind %d, expected %d.", k, ELF_K_ELF);

 done:
	if (e1)
		(void) elf_end(e1);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}

/*
 * Check opening of ar(1) archives opened with elf_memory().
 */

void
tcArMemoryFdIgnored_$1(void)
{
	Elf *e, *e1;
	int fd, result;
	Elf_Kind k;
	struct stat sb;
	char *b;

	TP_ANNOUNCE("($1): The fd value is ignored for archives opened "
	    "with elf_memory().");

	TP_SET_VERSION();

	e = e1 = NULL;
	b = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	/*
	 * First, populate a memory area with the contents of
	 * an ar(1) archive.
	 */

	if ((fd = open(TS_ARFILE_$1, O_RDONLY)) < 0) {
		TP_UNRESOLVED("open of \"" TS_ARFILE_$1 "\" failed: %s",
		    strerror(errno));
		goto done;
	}

	if (fstat(fd, &sb) < 0) {
		TP_UNRESOLVED("fstat failed: %s", strerror(errno));
		goto done;
	}

	if ((b = malloc(sb.st_size)) == NULL) {
		TP_UNRESOLVED("malloc failed: %s", strerror(errno));
		goto done;
	}

	if (read(fd, b, sb.st_size) != sb.st_size) {
		/* Deal with ERESTART? */
		TP_UNRESOLVED("read failed: %s", strerror(errno));
		goto done;
	}

	if ((e = elf_memory(b, sb.st_size)) == NULL) {
		TP_FAIL("elf_memory failed: %s", elf_errmsg(-1));
		goto done;
	}

	/*
	 * Verify that the fd value is ignored for this case.
	 */
	if ((e1 = elf_begin(-2, ELF_C_READ, e)) == NULL) {
		TP_FAIL("elf_begin() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((k = elf_kind(e1)) != ELF_K_ELF)
		TP_FAIL("kind %d, expected %d.", k, ELF_K_ELF);

	result = TET_PASS;

 done:
	if (b)
		free(b);
	if (e1)
		(void) elf_end(e1);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}
')

ARFN(`BSD')
ARFN(`SVR4')
