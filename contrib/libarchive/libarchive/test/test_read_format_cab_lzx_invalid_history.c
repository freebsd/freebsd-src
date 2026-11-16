#include "test.h"

DEFINE_TEST(test_read_format_cab_lzx_invalid_history)
{
	const char *invalid = "test_read_format_cab_lzx_invalid_history.cab";
	const char *control = "test_read_format_cab_lzx_literal_control.cab";
	struct archive *a;
	struct archive_entry *ae;
	char buff[2];

	extract_reference_file(invalid);
	extract_reference_file(control);

	/* A match cannot refer to history that has not been produced yet. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, invalid, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("x", archive_entry_pathname(ae));
	assertEqualInt(2, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_FATAL,
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualInt(-1, archive_errno(a));
	assertEqualString("LZX decompression failed (-25)",
	    archive_error_string(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* The corresponding literal-only stream remains valid. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, control, 10240));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("x", archive_entry_pathname(ae));
	assertEqualInt(2, archive_entry_size(ae));
	assertEqualIntA(a, sizeof(buff),
	    archive_read_data(a, buff, sizeof(buff)));
	assertEqualMem("\0\0", buff, sizeof(buff));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
