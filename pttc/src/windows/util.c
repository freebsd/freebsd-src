/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "errcode.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>


int run(const char *file, char *const argv[])
{
	int errcode;

	int i;
	size_t size;
	char *args;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD exit_code;

	DWORD dwret;
	BOOL bret;


	errcode = 0;

	if (bug_on(!file)) {
		errcode = -err_internal;
		goto out;
	}

	if (bug_on(!argv)) {
		errcode = -err_internal;
		goto out;
	}


	/* calculate length of command line - this is the cumulative length of
	 * all arguments, plus two quotation marks (to make it quoted strings
	 * and allow for spaces in file/path names), plus a space after each
	 * arguments as delimiter (after the last arguments it's a terminating
	 * zero-byte instead of the space).	 *
	 */
	size = 0;
	for (i = 0; argv[i]; ++i)
		size += strlen(argv[i]) + 3;

	/* allocate command line string */
	args = calloc(size, 1);
	if (!args)
		return -err_no_mem;

	/* construct command line string, putting quotation marks
	 * around every argument of the vector and a space after it
	 */
	size = 0;
	for (i = 0; argv[i]; ++i) {
		args[size++] = '"';
		strcpy(args + size, argv[i]);
		size += strlen(argv[i]);
		args[size++] = '"';
		args[size++] = ' ';
	}
	/* transform last space into a terminating zero-byte and fix up size */
	args[--size] = '\0';


	/* initialize process/startup info */
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	/* create process - since the first parameter is NULL, the
	 * second parameter represents a command as it would behave
	 * on a command shell
	 */
	bret = CreateProcess(NULL, args,
			     NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	if (!bret) {
		errcode = -err_other;
		goto out_args;
	}

	dwret = WaitForSingleObject(pi.hProcess, INFINITE);
	if (dwret == WAIT_FAILED) {
		errcode = -err_other;
		goto out_handles;
	}

	bret = GetExitCodeProcess(pi.hProcess, &exit_code);
	if (!bret) {
		errcode = -err_other;
		goto out_handles;
	}

	if (exit_code != 0)
		errcode = -err_run;


out_handles:
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
out_args:
	free(args);
out:
	return errcode;
}
