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

#include "ptunit_mkfile.h"

#include "intel-pt.h"

#include <windows.h>
#include <string.h>


int ptunit_mkfile(FILE **pfile, char **pfilename, const char *mode)
{
	char dirbuffer[MAX_PATH], buffer[MAX_PATH], *filename;
	const char *dirname;
	FILE *file;
	DWORD dirlen;
	UINT status;

	/* We only support char-based strings. */
	if (sizeof(TCHAR) != sizeof(char))
		return -pte_not_supported;

	dirname = dirbuffer;
	dirlen = GetTempPath(sizeof(dirbuffer), dirbuffer);
	if (!dirlen || dirlen >= sizeof(dirbuffer))
		dirname = ".";

	status = GetTempFileName(dirname, "ptunit-tmp-", 0, buffer);
	if (!status)
		return -pte_not_supported;

	file = fopen(buffer, mode);
	if (!file)
		return -pte_not_supported;

	filename = _strdup(buffer);
	if (!filename) {
		fclose(file);
		return -pte_nomem;
	}

	*pfile = file;
	*pfilename = filename;

	return 0;
}
