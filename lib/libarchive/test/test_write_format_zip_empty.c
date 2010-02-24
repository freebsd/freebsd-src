/*-
 * Copyright (c) 2008 Anselm Strauss
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

/*
 * Development supported by Google Summer of Code 2008.
 */

#include "test.h"
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_write_format_zip_empty)
{
	struct archive *a;
	char buff[256];
	size_t used;

	/* Zip format: Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertA(0 == archive_write_set_format_zip(a));
	assertA(0 == archive_write_set_compression_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* Close out the archive without writing anything. */
	assertA(0 == archive_write_close(a));
	assertA(0 == archive_write_finish(a));

	/* Verify the correct format for an empy Zip archive. */
	assertEqualInt(used, 22);
	assertEqualMem(buff,
	    "PK\005\006\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
	    22);
}
