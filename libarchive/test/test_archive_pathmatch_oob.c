/*-
 * Copyright (c) 2025 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

#include <stdlib.h>
#include <string.h>

/*
 * Regression test: malformed bracket expressions in patterns caused
 * heap-buffer-overflow reads in pm() (BUG-003) and strchr OOB via
 * archive_match (BUG-007).
 *
 * To reproduce under ASan, we must allocate tight heap buffers for the
 * pattern and string (just like the fuzzer does), so that ASan's
 * redzones detect overreads.
 */

/* Declare the internal function we want to test. */
int __archive_pathmatch(const char *pattern, const char *string, int flags);

/*
 * Replay a fuzzer repro file through __archive_pathmatch().
 * Format: byte 0 = flags, then rest is split at first '\0' into
 * pattern and string, each copied into tight malloc'd buffers.
 */
static void
replay_pathmatch_repro(const char *refname)
{
	FILE *f;
	unsigned char buf[4096];
	size_t size, split, rest_len, str_start, str_len;
	const unsigned char *rest;
	char *pattern, *string;
	int flags;
	size_t i;

	extract_reference_file(refname);

	f = fopen(refname, "rb");
	if (!assert(f != NULL))
		return;
	size = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (!assert(size >= 4))
		return;

	flags = buf[0] % 8;
	rest = buf + 1;
	rest_len = size - 1;

	/* Find split point (first null byte in rest). */
	split = 0;
	for (i = 0; i < rest_len; i++) {
		if (rest[i] == 0) {
			split = i;
			break;
		}
	}
	if (split == 0)
		split = rest_len / 2;

	/* Allocate tight buffers so ASan can detect overreads. */
	pattern = (char *)malloc(split + 1);
	if (!assert(pattern != NULL))
		return;
	memcpy(pattern, rest, split);
	pattern[split] = '\0';

	str_start = (split < rest_len && rest[split] == 0)
	    ? split + 1 : split;
	str_len = rest_len - str_start;
	string = (char *)malloc(str_len + 1);
	if (!assert(string != NULL)) {
		free(pattern);
		return;
	}
	memcpy(string, rest + str_start, str_len);
	string[str_len] = '\0';

	/* This must not read past the allocated buffers. */
	__archive_pathmatch(pattern, string, flags);

	free(pattern);
	free(string);
}

DEFINE_TEST(test_archive_pathmatch_oob_bracket)
{
	replay_pathmatch_repro("test_archive_pathmatch_oob_bracket.bin");
}

DEFINE_TEST(test_archive_pathmatch_oob_strchr)
{
	replay_pathmatch_repro("test_archive_pathmatch_oob_strchr.bin");
}
