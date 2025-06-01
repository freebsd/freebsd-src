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

DEFINE_TEST(test_extract_tar_absolute_paths)
{
	int r;

	// Create an absolute path for a test file inside testworkdir.
	char *entry_suffix = "/tar-noabs";
	size_t entry_suffix_length = strlen(entry_suffix);
	size_t testworkdir_length = strlen(testworkdir);
	size_t temp_absolute_file_name_length = testworkdir_length + entry_suffix_length;
	char *temp_absolute_file_name = calloc(1, temp_absolute_file_name_length + 1); // +1 for null character.
	assertEqualInt(snprintf(temp_absolute_file_name, temp_absolute_file_name_length + 1, "%s%s", testworkdir, entry_suffix),
		temp_absolute_file_name_length);

#if defined(_WIN32) && !defined(__CYGWIN__)
	// I'm unsure how to specify paths with spaces for the test invocation on windows.
	// Adding quotes doesn't seem to work. We should find a way to escape these paths,
	// but for now let's fail in a place that's obviously related to the test setup if
	// testworkdir contains spaces.
	for (char *p = temp_absolute_file_name; *p != '\0'; p++)
	{
		assert(*p != ' ');
		if (*p == ' ') break;
	}
#endif

	// Create the file.
	const char *sample_data = "test file from test_extract_tar_absolute_paths";
	assertMakeFile(temp_absolute_file_name, 0644, sample_data);

	// Create an archive with the test file, using an absolute path.
#if defined(_WIN32) && !defined(__CYGWIN__)
	r = systemf("%s --absolute-paths -cf test.tar %s", testprog, temp_absolute_file_name);
#else
	r = systemf("%s --absolute-paths -cf test.tar \"%s\"", testprog, temp_absolute_file_name);
#endif
	assertEqualInt(r, 0);

	UNLINK(temp_absolute_file_name);

	// Extracting the archive without -P / --absolute-paths should strip leading drive letter or slash
	r = systemf("%s -xf test.tar 2>test.err", testprog);
	assertEqualInt(r, 0);
	assertFileNotExists(temp_absolute_file_name);

	// Check that the mangled path exists.
#if defined(_WIN32) && !defined(__CYGWIN__)
	assertFileExists(temp_absolute_file_name + 3); // Skip the drive letter, colon and slash.
	UNLINK(temp_absolute_file_name + 3);
#else
	assertFileExists(temp_absolute_file_name + 1); // Skip the slash.
	UNLINK(temp_absolute_file_name + 1);
#endif

	// Extracting the archive with -P / --absolute-paths should create the file.
	r = systemf("%s --absolute-paths -xf test.tar", testprog);
	assertEqualInt(r, 0);
	assertFileExists(temp_absolute_file_name);

	// Check that the mangled path wasn't created.
#if defined(_WIN32) && !defined(__CYGWIN__)
	assertFileNotExists(temp_absolute_file_name + 3); // Skip the drive letter, colon and slash.
#else
	assertFileNotExists(temp_absolute_file_name + 1); // Skip the slash.
#endif
}
