#include "test.h"

/*
 * Regression test for infinite loop when parse_block_header() returns
 * ARCHIVE_FAILED inside do_uncompress_file()'s while(1) loop.
 *
 * The loop only exited on ARCHIVE_EOF or ARCHIVE_FATAL.  After converting
 * per-entry block header errors from ARCHIVE_FATAL to ARCHIVE_FAILED, a
 * malformed block header caused the loop to spin forever.
 *
 * Fuzzer-generated reproducer (37 bytes).
 */
DEFINE_TEST(test_read_format_rar5_block_hdr_fail_loop)
{
	/* RAR5 archive with a malformed compressed block header */
	static const uint8_t data[] = {
		0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00,
		0x86,0x7e,0xfa,0xe7,0x03,0x02,0x56,0x20,
		0x00,0x20,0x15,0xae,0x21,0x00,0x01,0x08,
		0x00,0x00,0x01,0x00,0x00,0x15,0x00,0xbe,
		0xc0,0x80,0x00,0xff,0xf4
	};

	struct archive *a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_rar5(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a,
	    data, sizeof(data)));

	struct archive_entry *ae;
	char buf[4096];
	while (archive_read_next_header(a, &ae) == ARCHIVE_OK)
		while (archive_read_data(a, buf, sizeof(buf)) > 0)
			;

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
