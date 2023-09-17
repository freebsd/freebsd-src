/*-
 * Copyright (c) 2019 Martin Matuska
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

static struct archive_entry*
create_archive_entry(void) {
	struct archive_entry *ae;

	assert((ae = archive_entry_new()) != NULL);
        archive_entry_set_atime(ae, 2, 20);
        archive_entry_set_ctime(ae, 4, 40);
        archive_entry_set_mtime(ae, 5, 50);
        archive_entry_copy_pathname(ae, "file");
        archive_entry_set_mode(ae, AE_IFREG | 0755);
        archive_entry_set_nlink(ae, 2);
        archive_entry_set_size(ae, 8);
        archive_entry_xattr_add_entry(ae, "user.data1", "ABCDEFG", 7);
        archive_entry_xattr_add_entry(ae, "user.data2", "XYZ", 3);

	return (ae);
}

DEFINE_TEST(test_pax_xattr_header)
{
	static const char *reffiles[] = {
	    "test_pax_xattr_header_all.tar",
	    "test_pax_xattr_header_libarchive.tar",
	    "test_pax_xattr_header_schily.tar",
	    NULL
	};
	struct archive *a;
	struct archive_entry *ae;

	extract_reference_files(reffiles);

	/* First archive, no options */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_pax(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	assertEqualInt(0,
	    archive_write_open_filename(a, "test1.tar"));
	ae = create_archive_entry();
        assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
        archive_entry_free(ae);
        assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assertEqualFile("test1.tar","test_pax_xattr_header_all.tar");

	/* Second archive, xattrheader=SCHILY */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_pax(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	assertEqualIntA(a, 0, archive_write_set_options(a,
	    "xattrheader=SCHILY")); 
	assertEqualInt(0,
	    archive_write_open_filename(a, "test2.tar"));

	ae = create_archive_entry();
        assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
        archive_entry_free(ae);
        assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assertEqualFile("test2.tar","test_pax_xattr_header_schily.tar");

	/* Third archive, xattrheader=LIBARCHIVE */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_pax(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	assertEqualIntA(a, 0, archive_write_set_options(a,
	    "xattrheader=LIBARCHIVE")); 
	assertEqualInt(0,
	    archive_write_open_filename(a, "test3.tar"));

	ae = create_archive_entry();
        assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
        archive_entry_free(ae);
        assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assertEqualFile("test3.tar","test_pax_xattr_header_libarchive.tar");

	/* Fourth archive, xattrheader=ALL */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, 0, archive_write_set_format_pax(a));
	assertEqualIntA(a, 0, archive_write_add_filter_none(a));
	assertEqualIntA(a, 0, archive_write_set_options(a, "xattrheader=ALL")); 
	assertEqualInt(0,
	    archive_write_open_filename(a, "test4.tar"));

	ae = create_archive_entry();
        assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
        archive_entry_free(ae);
        assertEqualIntA(a, 8, archive_write_data(a, "12345678", 9));

	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_free(a));

	assertEqualFile("test4.tar","test_pax_xattr_header_all.tar");
}
