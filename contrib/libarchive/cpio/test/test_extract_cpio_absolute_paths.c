/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mostyn Bramley-Moore <mostyn@antipode.se>
 */

#include "test.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define UNLINK _unlink
#else
#define UNLINK unlink
#endif

DEFINE_TEST(test_extract_cpio_absolute_paths)
{
	int r;

	// Create an absolute path for a test file inside testworkdir.
	const char *entry_suffix = "/cpio-noabs";
	size_t entry_suffix_length = strlen(entry_suffix);
	size_t testworkdir_length = strlen(testworkdir);
	size_t temp_absolute_file_name_length = testworkdir_length + entry_suffix_length;
	char *temp_absolute_file_name = calloc(1, temp_absolute_file_name_length + 1); // +1 for null character.
	assertEqualInt(snprintf(temp_absolute_file_name, temp_absolute_file_name_length + 1, "%s%s", testworkdir, entry_suffix),
		temp_absolute_file_name_length);

	// Create the file.
	const char *sample_data = "test file from test_extract_cpio_absolute_paths";
	assertMakeFile(temp_absolute_file_name, 0644, sample_data);

	// Create an archive with the test file, using an absolute path.
	assertMakeFile("filelist", 0644, temp_absolute_file_name);
	r = systemf("%s -o < filelist > archive.cpio 2> stderr1.txt", testprog);
	assertEqualInt(r, 0);

	// Ensure that the temp file does not exist.
	UNLINK(temp_absolute_file_name);

	// We should refuse to create the absolute path without --insecure.
	r = systemf("%s -i < archive.cpio 2> stderr2.txt", testprog);
	assert(r != 0);
	assertFileNotExists(temp_absolute_file_name);
	UNLINK(temp_absolute_file_name); // Cleanup just in case.

	// But if we specify --insecure then the absolute path should be created.
	r = systemf("%s -i --insecure < archive.cpio 2> stderr3.txt", testprog);
	assert(r == 0);
	assertFileExists(temp_absolute_file_name);
}
