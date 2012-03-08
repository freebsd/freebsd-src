/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
__FBSDID("$FreeBSD$");

/*
 * Exercise support for reading Solaris-style ACL data
 * from tar archives.
 *
 * This should work on all systems, regardless of whether local
 * filesystems support ACLs or not.
 */

DEFINE_TEST(test_compat_solaris_tar_acl)
{
	struct archive *a;
	struct archive_entry *ae;
	const char *reference1 = "test_compat_solaris_tar_acl.tar";
	int type, permset, tag, qual;
	const char *name;

	/* Sample file generated on Solaris 10 */
	extract_reference_file(reference1);
	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_open_filename(a, reference1, 512));

	/* Archive has 1 entry with some ACLs set on it. */
	assertA(0 == archive_read_next_header(a, &ae));
	failure("Basic ACLs should set mode to 0644, not %04o",
	    archive_entry_mode(ae)&0777);
	assertEqualInt((archive_entry_mode(ae) & 0777), 0644);
	assertEqualInt(7, archive_entry_acl_reset(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(006, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER_OBJ, tag);
	assertEqualInt(-1, qual);
	assert(name == NULL);

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(004, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_GROUP_OBJ, tag);
	assertEqualInt(-1, qual);
	assert(name == NULL);

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(004, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_OTHER, tag);
	assertEqualInt(-1, qual);
	assert(name == NULL);

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(001, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualInt(71, qual);
	assertEqualString(name, "lp");

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(004, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualInt(666, qual);
	assertEqualString(name, "666");

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(007, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_USER, tag);
	assertEqualInt(1000, qual);
	assertEqualString(name, "trasz");

	assertEqualInt(ARCHIVE_OK, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));
	assertEqualInt(ARCHIVE_ENTRY_ACL_TYPE_ACCESS, type);
	assertEqualInt(004, permset);
	assertEqualInt(ARCHIVE_ENTRY_ACL_MASK, tag);
	assertEqualInt(-1, qual);
	assertEqualString(name, NULL);

	assertEqualInt(ARCHIVE_EOF, archive_entry_acl_next(ae,
		ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
		&type, &permset, &tag, &qual, &name));

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
