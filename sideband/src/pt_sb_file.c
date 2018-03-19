/*
 * Copyright (c) 2017-2018, Intel Corporation
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

#include "pt_sb_file.h"

#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "intel-pt.h"


int pt_sb_file_load(void **pbuffer, size_t *psize, const char *filename,
		    size_t begin, size_t end)
{
	size_t size;
	FILE *file;
	void *content;
	long fsize, fbegin, fend;
	int errcode;

	if (!pbuffer || !psize || !filename)
		return -pte_invalid;

	if (end && end <= begin)
		return -pte_invalid;

	if (LONG_MAX < begin || LONG_MAX < end)
		return -pte_invalid;

	file = fopen(filename, "rb");
	if (!file)
		return -pte_bad_file;

	errcode = fseek(file, 0, SEEK_END);
	if (errcode)
		goto out_file;

	fsize = ftell(file);
	if (fsize < 0)
		goto out_file;

	fbegin = (long) begin;
	if (!end)
		fend = fsize;
	else {
		fend = (long) end;
		if (fsize < fend)
			fend = fsize;
	}

	size = (size_t) (fend - fbegin);

	errcode = fseek(file, fbegin, SEEK_SET);
	if (errcode)
		goto out_file;

	content = malloc(size);
	if (!content) {
		fclose(file);
		return -pte_nomem;
	}

	*psize = fread(content, 1, size, file);
	*pbuffer = content;

	fclose(file);
	return 0;

out_file:
	fclose(file);
	return -pte_bad_file;
}
