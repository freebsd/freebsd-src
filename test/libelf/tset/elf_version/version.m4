/*-
 * Copyright (c) 2006 Joseph Koshy
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
 * $Id: version.m4 1339 2010-12-31 15:42:52Z jkoshy $
 */

include(`elfts.m4')

#include <libelf.h>

#include "tet_api.h"

/*
 * Test the `elf_version' entry point.
 *
 * Each test case requires a separate invocation of the test
 * executable because the first call of elf_version() sets private
 * state in the library.  Consequently these tests are organized as
 * one test purpose per test case.
 */

/*
 * Test version number retrieval.
 */

void
tcParamNoneReturnsCurrentVersion(void)
{
	TP_ANNOUNCE("Param EV_NONE returns version == EV_CURRENT");
	if (elf_version(EV_NONE) != EV_CURRENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}


/*
 * Test that an unsupported version number is not accepted.
 */

void
tcValueTooLarge(void)
{
	TP_ANNOUNCE("calling elf_version() with an unsupported "
	    "(too-large) value fails and sets the error to ELF_E_VERSION.");

	if (elf_version(EV_CURRENT+1) != EV_NONE) {
		tet_result(TET_FAIL);
		return;
	}

	if (elf_errno() != ELF_E_VERSION) {
		tet_result(TET_FAIL);
		return;
	}

	tet_result(TET_PASS);
}

/*
 * Test that a reject version number does not cause the internal
 * version number to change.
 */

void
tcValueErrorNoChange(void)
{
	TP_ANNOUNCE("the library's current version should not be "
	    "changed by a failing call to elf_version().");

	if (elf_version(EV_CURRENT+1) != EV_NONE) {
		tet_infoline("unresolved: illegal elf_version() call did not "
		    "fail as expected.");
		tet_result(TET_UNRESOLVED);
		return;
	}

	if (elf_version(EV_NONE) != EV_CURRENT) {
		tet_result(TET_FAIL);
		return;
	}

	tet_result(TET_PASS);
}

/*
 * Test that setting the library version to a legal value should
 * succeed.
 *
 * Currently, EV_CURRENT (== 1) is the only legal version.  When more
 * ELF versions are defined, this test should be changed to iterate
 * over all of them.
 */

void
tcValidValuesAreOk(void)
{
	int result;
	unsigned int old_version, new_version;

	TP_ANNOUNCE("setting the ELF version to a legal value"
	    "passes");

	result = TET_UNRESOLVED;
	old_version = elf_version(EV_NONE);

	if (old_version == EV_NONE) {
		TP_UNRESOLVED("unknown current elf version");
		goto done;
	}

	new_version = EV_CURRENT;
	if (elf_version(new_version) != old_version) {
		TP_FAIL("unexpected return value from "
		    "elf_version(new_version)");
		goto done;
	}

	/* retrieve the version that was set and check */
	if (elf_version(EV_NONE) != new_version) {
		TP_FAIL("the new ELF version was not succesfully "
		    "set. ");
		goto done;
	}

	result = TET_PASS;

done:
	tet_result(result);
}

/*
 * Other APIs that shouldn't have elf_version() called.
 */

void
tcSequenceErrorElfMemory(void)
{
	Elf *e;

	TP_ANNOUNCE("elf_memory() before elf_version() "
	    "fails with ELF_E_SEQUENCE.");

	if ((e = elf_memory(NULL, 0)) != NULL ||
	    elf_errno() != ELF_E_SEQUENCE)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}

void
tcSequenceErrorElfKind(void)
{
	TP_ANNOUNCE("assertion: calling elf_kind() before elf_version() "
	    "fails with ELF_E_SEQUENCE.");

	/* Note: no elf_version() call */
	if (elf_kind(NULL) != ELF_K_NONE &&
	    elf_errno() != ELF_E_SEQUENCE)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}
