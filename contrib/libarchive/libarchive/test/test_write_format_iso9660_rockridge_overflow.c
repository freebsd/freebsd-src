/*-
 * Copyright (c) 2026 Microsoft Corporation
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

DEFINE_TEST(test_write_format_iso9660_rockridge_overflow)
{
	struct archive *a;
	struct archive_entry *ae;
	unsigned char *buff;
	char name[706];
	size_t buffsize = 256 * 2048;
	size_t used;

	buff = malloc(buffsize);
	assert(buff != NULL);
	if (buff == NULL)
		return;

	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_iso9660(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, buffsize, &used));

	
	name[0] = '.';
	name[1] = '/';
	/* Empirically, it seems it requires at least 7 iterations to line up the RR blocks and trigger the ASAN failure */
	for (int i = 0; i < 10; ++i) {
		/* AAAA... BBBB... CCCC... */
		memset(&name[2], 0x41 + i, 703);
		name[705] = '\0';

		assert((ae = archive_entry_new()) != NULL);
		archive_entry_set_atime(ae, 2, 0);
		archive_entry_set_ctime(ae, 4, 0);
		archive_entry_set_mtime(ae, 5, 0);
		archive_entry_copy_pathname(ae, name);
		archive_entry_set_mode(ae, S_IFREG | 0755);

		assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
		archive_entry_free(ae);
	}

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	free(buff);
}
