/*-
 * Copyright (c) 2006,2010,2011 Joseph Koshy
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
 * $Id: errmsg.m4 2191 2011-11-21 08:34:02Z jkoshy $
 */

include(`elfts.m4')

#include <libelf.h>
#include <string.h>

#include "tet_api.h"

/*
 * Test the `elf_errmsg' entry point.
 */

/*
 * Assertion: the function returns NULL if the argument is zero and
 * there is no pending error in the library.
 */
void
tcZeroNoerror(void)
{

	TP_ANNOUNCE("returns NULL with zero & no current error");

	(void) elf_errno();	/* discard current error number */

	if (elf_errmsg(0) == NULL)
		tet_result(TET_PASS);
	else
		tet_result(TET_FAIL);
}

/*
 * An error value of -1 should return non-NULL
 */

define(`NO_ERROR_MESSAGE',`"No Error"')dnl Needs to match the string in "libelf/elf_errmsg.c".

void
tcMinusoneNoerror(void)
{
	int result;
	const char *msg;

	TP_ANNOUNCE("returns non-null for arg -1 & no current error");

	(void) elf_errno();	/* discard stored error */

	result = TET_UNRESOLVED;

	msg = elf_errmsg(-1);
	if (msg == NULL) {
		TP_FAIL("null return from elf_errmsg()");
		goto done;
	}

	if (strcmp(msg, NO_ERROR_MESSAGE)) {
		TP_FAIL("unexpected message \"%s\"", msg);
		goto done;
	}

	result = TET_PASS;

done:
	tet_result(result);
}

/*
 * Assertion: All error numbers from 1..NUM return a non-null string.
 */

void
tcCheckAllValidErrorMessages(void)
{
	int n, result;
	const char *msg;

	TP_ANNOUNCE("returns non-null for all valid error numbers");

	(void) elf_errno();	/* discard stored error */

	result = TET_UNRESOLVED;

	for (n = ELF_E_NONE+1; n < ELF_E_NUM; n++) {
		if ((msg = elf_errmsg(n)) == NULL) {
			TP_FAIL("null return from elf_errmsg()");
			goto done;
		}
	}

	result = TET_PASS;

done:
	tet_result(result);

}

/*
 * Assertion: with an error pending, elf_errmsg(0) returns a non-NULL
 * pointer.
 */

void
tcNonNullWithErrorPending(void)
{
	int result, version;
	const char *msg;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("non null error message is returned for a pending error");

	/* Generate an error, e.g., ELF_E_VERSION. */
	if ((version = elf_version(EV_CURRENT+1)) != EV_NONE) {
		TP_UNRESOLVED("elf_version() returned %d", version);
		goto done;
	}

	if ((msg = elf_errmsg(0)) == NULL) {
		TP_FAIL("elf_errmsg() returned NULL");
		goto done;
	}

	result = TET_PASS;

done:
	tet_result(result);
}
