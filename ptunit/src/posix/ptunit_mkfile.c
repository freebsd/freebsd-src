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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int ptunit_mkfile(FILE **pfile, char **pfilename, const char *mode)
{
	FILE *file;
	const char *tmpdir;
	const char *tmpfile;
	char template[256], *filename;
	int fd, len;

	tmpfile = "ptunit-tmp-XXXXXX";
	tmpdir = getenv("TMP");
	if (!tmpdir || !tmpdir[0])
		tmpdir = "/tmp";

	len = snprintf(template, sizeof(template), "%s/%s", tmpdir, tmpfile);
	if (len < 0)
		return -pte_not_supported;

	/* We must not truncate the template. */
	if (sizeof(template) <= (size_t) len)
		return -pte_not_supported;

	fd = mkstemp(template);
	if (fd == -1)
		return -pte_not_supported;

	file = fdopen(fd, mode);
	if (!file) {
		close(fd);
		return -pte_not_supported;
	}

	filename = strdup(template);
	if (!filename) {
		fclose(file);
		return -pte_nomem;
	}

	*pfile = file;
	*pfilename = filename;

	return 0;
}
