/*-
 * Copyright (c) 2026 Tim Kientzle
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

/*
 * A USER NFSv4 ACL entry with a NULL name and a large numeric qualifier
 * triggers a heap buffer overflow in archive_acl_to_text_l().
 *
 * archive_acl_text_len() only reserves space for the trailing ":id" digits
 * when ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID is set, but the serializer always
 * writes them for USER/GROUP entries when the name is NULL.  With a 7-digit
 * ID the allocated buffer is too short and append_id() writes past its end.
 *
 * Without the fix, libarchive's own post-write guard fires:
 *   "Fatal Internal Error: Buffer overrun"
 * (or under AddressSanitizer a heap-buffer-overflow is reported at
 * archive_acl.c in append_id / append_entry).
 */
DEFINE_TEST(test_acl_nfs4_null_id_overflow)
{
	struct archive_entry *ae;
	char *text;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);

	/* NULL name + 7-digit id: the missing ":9999999" overflows the buffer */
	archive_entry_acl_add_entry(ae,
	    ARCHIVE_ENTRY_ACL_TYPE_DENY,
	    0,
	    ARCHIVE_ENTRY_ACL_USER,
	    9999999,
	    NULL);

	text = archive_entry_acl_to_text(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assert(text != NULL);
	/* The numeric id must appear in the output as the user name */
	assert(strstr(text, "9999999") != NULL);
	free(text);

	archive_entry_free(ae);
}

/*
 * The wide (wchar_t) serializer has a related bug: when the name is NULL
 * it passes id=-1 to append_entry_w() instead of the actual numeric id,
 * so the id is never written and a garbage character appears in the name
 * slot instead.
 *
 * The wide estimator has the same missing-id-digits gap as the narrow one,
 * and both must be fixed together with the wide serializer.
 */
DEFINE_TEST(test_acl_nfs4_null_id_overflow_w)
{
	struct archive_entry *ae;
	wchar_t *wtext;
	ssize_t len;

	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "file");
	archive_entry_set_mode(ae, AE_IFREG | 0644);

	archive_entry_acl_add_entry(ae,
	    ARCHIVE_ENTRY_ACL_TYPE_DENY,
	    0,
	    ARCHIVE_ENTRY_ACL_USER,
	    9999999,
	    NULL);

	wtext = archive_entry_acl_to_text_w(ae, &len,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
	assert(wtext != NULL);
	/* The numeric id must appear in the output as the user name */
	assert(wcsstr(wtext, L"9999999") != NULL);
	free(wtext);

	archive_entry_free(ae);
}
