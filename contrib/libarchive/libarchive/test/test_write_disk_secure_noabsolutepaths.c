/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mostyn Bramley-Moore <mostyn@antipode.se>
 */

#include "test.h"

#include <stdlib.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define UNLINK _unlink
#else
#include <unistd.h>
#define UNLINK unlink
#endif

/*
 * Exercise security checks that should prevent writing absolute paths
 * when extracting archives.
 */
DEFINE_TEST(test_write_disk_secure_noabsolutepaths)
{
	struct archive *a, *ad;
	struct archive_entry *ae;

	char buff[10000];

	size_t used;

	// Create an archive_write object.
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_open_memory(a, buff, sizeof(buff), &used));

	// Create an absolute path for a test file inside testworkdir.
	const char *entry_suffix = "/badfile";
	size_t entry_suffix_length = strlen(entry_suffix);
	size_t testworkdir_length = strlen(testworkdir);
	size_t temp_absolute_file_name_length = testworkdir_length + entry_suffix_length;
	char *temp_absolute_file_name = calloc(1, temp_absolute_file_name_length + 1); // +1 for null character.
	assertEqualInt(snprintf(temp_absolute_file_name, temp_absolute_file_name_length + 1, "%s%s", testworkdir, entry_suffix),
		temp_absolute_file_name_length);

	// Convert to a unix-style path, so we can compare it to the entry
	// path when reading back the archive.
	for (char *p = temp_absolute_file_name; *p != '\0'; p++)
		if (*p == '\\') *p = '/';

	// Add a regular file entry with an absolute path.
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_copy_pathname(ae, temp_absolute_file_name);
	archive_entry_set_mode(ae, S_IFREG | 0777);
	archive_entry_set_size(ae, 6);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualInt(6, archive_write_data(a, "hello", 6));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	// Now try to extract the data.
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, buff, used));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString(temp_absolute_file_name, archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));
	assertEqualInt(AE_IFREG | 0777, archive_entry_mode(ae));
	assertEqualInt(6, archive_entry_size(ae));

	// This should succeed.
	assertEqualInt(ARCHIVE_OK, archive_read_extract(a, ae, 0));
	UNLINK(temp_absolute_file_name);

	// This should fail, since the archive entry has an absolute path.
	assert(ARCHIVE_OK != archive_read_extract(a, ae, ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS));

	// This should also fail.
	assert((ad = archive_write_new()) != NULL);
	assertEqualInt(ARCHIVE_OK, archive_write_disk_set_options(a, ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS));
	assert(ARCHIVE_OK != archive_read_extract2(a, ae, ad));

	assertEqualInt(ARCHIVE_OK, archive_write_free(ad));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
