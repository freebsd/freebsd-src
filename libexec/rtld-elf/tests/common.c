/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2014 Jonathan Anderson.
 * Copyright 2021 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <atf-c.h>
#include <fcntl.h>
#include <stdio.h>

#include "common.h"

void
expect_success(int binary, char *senv)
{
	char * const env[] = { senv, NULL };

	try_to_run(binary, 0, env, "the hypotenuse of 3 and 4 is 5\n", "");
}

void
expect_missing_library(int binary, char *senv)
{
	char * const env[] = { senv, NULL };

	try_to_run(binary, 1, env, "",
	   "ld-elf.so.1: Shared object \"libpythagoras.so.0\" not found,"
	    " required by \"target\"\n");
}

void
try_to_run(int binary, int exit_status, char * const *env,
        const char *expected_out, const char *expected_err)
{
	pid_t child = atf_utils_fork();

	if (child == 0) {
		char * const args[] = { "target", NULL };

		fexecve(binary, args, env);
		atf_tc_fail("fexecve() failed");
	}

	atf_utils_wait(child, exit_status, expected_out, expected_err);
}

int
opendir_fd(const char *name)
{

	return open(name, O_RDONLY | O_DIRECTORY);
}

int
opendirat(int parent, const char *name)
{

	return openat(parent, name, O_RDONLY | O_DIRECTORY);
}
