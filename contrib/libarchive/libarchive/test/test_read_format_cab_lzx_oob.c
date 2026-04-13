#include "test.h"

/*
 * Regression test for Heap Out-of-Bounds Write in CAB LZX decoder.
 * This ensures that a malformed CFDATA uncompressed size does not
 * bypass physical buffer limits and cause memory corruption during skips.
 */
DEFINE_TEST(test_read_format_cab_lzx_oob)
{
	const char *refname = "test_read_format_cab_lzx_oob.cab";
	struct archive *a;
	struct archive_entry *ae;
	const void *buff;
	size_t size;
	int64_t offset;

	/* * The test framework will automatically find 'test_read_format_cab_lzx_oob.cab.uu',
	 * decode it, and place the binary '.cab' in the temporary test directory.
	 */
	extract_reference_file(refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));

	/* If it fails to open, there's a problem with the test setup/file */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));

	/* Read the header of the malformed entry */
	if (ARCHIVE_OK == archive_read_next_header(a, &ae)) {
			/* * We attempt to read a block to initialize the LZX state machine.
			* We do not assert the result because the file is intentionally malformed.
			* Regardless of success or failure, we force a skip to test state handling
			* and trigger the vulnerability.
			*/
			archive_read_data_block(a, &buff, &size, &offset);
			archive_read_data_skip(a);
		/* * Optional: We could assert that the error string contains our patch message, 
		 * but simply surviving without a segfault/ASAN violation is the primary goal 
		 * for fuzzing regression tests.
		 */
	}

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}