/*-
 * Copyright (c) 2026 Kaif Khan
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

#define __LIBARCHIVE_TEST
#include "archive_acl_private.h"

/*
 * archive_acl_from_text_nl() takes a pointer and a length and does not
 * require the buffer to be NUL terminated.  An entry that is only
 * whitespace, or trailing whitespace after a valid entry, must not make
 * the parser read past the end of the buffer.
 *
 * Each input is copied into a buffer sized to exactly its length so that
 * a read past the end is out of bounds.
 */
static void
parse_nl(const char *text, int type)
{
	size_t len = strlen(text);
	char *buf = malloc(len);
	struct archive_acl acl;

	assert(buf != NULL);
	memcpy(buf, text, len);
	memset(&acl, 0, sizeof(acl));
	/* Must not read buf[len]. */
	archive_acl_from_text_nl(&acl, buf, len, type, NULL);
	archive_acl_clear(&acl);
	free(buf);
}

DEFINE_TEST(test_acl_from_text_overread)
{
	static const char *whitespace[] = {
		" ", "   ", "\t", "\n\n", "user::rwx,   "
	};
	size_t i;

	for (i = 0; i < sizeof(whitespace) / sizeof(whitespace[0]); i++) {
		parse_nl(whitespace[i], ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		parse_nl(whitespace[i], ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	}

	/*
	 * Trailing whitespace after a valid entry must not change the parse:
	 * the count matches the same text without the trailing spaces.
	 */
	{
		static const char clean[] = "user:100:rwx";
		static const char padded[] = "user:100:rwx   ";
		struct archive_acl acl1, acl2;
		char *b1 = malloc(sizeof(clean) - 1);
		char *b2 = malloc(sizeof(padded) - 1);

		assert(b1 != NULL);
		assert(b2 != NULL);
		memcpy(b1, clean, sizeof(clean) - 1);
		memcpy(b2, padded, sizeof(padded) - 1);
		memset(&acl1, 0, sizeof(acl1));
		memset(&acl2, 0, sizeof(acl2));
		archive_acl_from_text_nl(&acl1, b1, sizeof(clean) - 1,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, NULL);
		archive_acl_from_text_nl(&acl2, b2, sizeof(padded) - 1,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, NULL);
		assertEqualInt(
		    archive_acl_count(&acl1, ARCHIVE_ENTRY_ACL_TYPE_ACCESS),
		    archive_acl_count(&acl2, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
		assert(archive_acl_count(&acl2,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS) > 0);
		archive_acl_clear(&acl1);
		archive_acl_clear(&acl2);
		free(b1);
		free(b2);
	}
}
