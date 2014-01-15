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
 * $Id: errno.m4 2191 2011-11-21 08:34:02Z jkoshy $
 */

include(`elfts.m4')

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

/*
 * Test the `elf_errno' entry point.
 *
 * Specific errors expected from other elf_* APIs are tested in the test
 * cases for those APIs.
 *
 * The tests here only check the behaviour of the elf_errno() API.
 */

/*
 * Assertion: The initial value of the libraries error number is zero.
 */

void
tcInitialValue(void)
{
	int err;

	TP_ANNOUNCE("The initial error value must be zero.");
	err = elf_errno();
	tet_result(err == 0 ? TET_PASS : TET_FAIL);
}

/*
 * Assertion: an elf_errno() call resets the stored error number.
 */

void
tcReset(void)
{
	int err;

	TP_ANNOUNCE("A pending error number must be reset by elf_errno().");

	(void) elf_errno();	/* discard stored error */
	err = elf_errno();
	tet_result(err == 0 ? TET_PASS : TET_FAIL);
}

/*
 * Assertion: elf_begin with cmd == ELF_C_NULL does not reset the
 * error value.
 */

void
tcNonResetWithNull(void)
{
	int error, fd, result;
	Elf *e, *e1;

	result = TET_UNRESOLVED;
	fd = -1;
	e = e1 = NULL;

	TP_ANNOUNCE("a pending error number is not reset by "
	    "elf_begin(ELF_C_NULL).");

	TP_SET_VERSION();

	/* Force an error. */
	if ((fd = open(".", O_RDONLY)) < 0) {
		TP_UNRESOLVED("open(.) failed: %s", strerror(errno));
		goto done;
	}

	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) != NULL) {
		TP_UNRESOLVED("elf_begin(ELF_C_WRITE) succeeded "
		    "unexpectedly.");
		goto done;
	}

	/* Invoke an operation with ELF_C_NULL */
	if ((e1 = elf_begin(fd, ELF_C_NULL, NULL)) != NULL) {
		TP_FAIL("elf_begin(ELF_C_NULL) returned non-null (%p)",
		    (void *) e1);
		goto done;
	}

	/* Recheck the old error. */
	if ((error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("unexpected error %d \"%s\"", error,
			elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:

	if (e)
		elf_end(e);
	if (e1)
		elf_end(e1);
	if (fd != -1)
	       (void) close(fd);

	tet_result(result);
}

/*
 * Assertion: elf_errno() retrieves the expected error, when one is pending.
 */

void
tcExpectedErrorIsReturned(void)
{
	int error, fd, result;
	Elf *e;

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	TP_ANNOUNCE("A pending error number is correctly returned.");

	TP_SET_VERSION();

	/* Force an error. */
	if ((fd = open(".", O_RDONLY)) < 0) {
		TP_UNRESOLVED("open(.) failed: %s", strerror(errno));
		goto done;
	}

	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) != NULL) {
		TP_UNRESOLVED("elf_begin(ELF_C_WRITE) succeeded "
		    "unexpectedly.");
		goto done;
	}

	/* Check the current error. */
	if ((error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("unexpected error %d \"%s\"", error,
			elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:

	if (e)
		elf_end(e);
	if (fd != -1)
	       (void) close(fd);

	tet_result(result);
}
