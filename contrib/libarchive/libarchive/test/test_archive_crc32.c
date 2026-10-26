#include "test.h"
#define __LIBARCHIVE_TEST 1
#include "archive_private.h"

DEFINE_TEST(test_archive_crc32)
{
	unsigned char buf[8192];
	size_t i;
	unsigned long crc_lib, crc_ref;

	/* Initialize buffer with some data */
	for (i = 0; i < sizeof(buf); i++)
		buf[i] = (unsigned char)(i ^ (i >> 8));

	/* 1. Empty */
	crc_lib = __archive_crc32(0, NULL, 0);
	crc_ref = bitcrc32(0, NULL, 0);
	assertEqualInt(crc_lib, crc_ref);

	crc_lib = __archive_crc32(0, buf, 0);
	crc_ref = bitcrc32(0, buf, 0);
	assertEqualInt(crc_lib, crc_ref);

	/* 2. Basic */
	crc_lib = __archive_crc32(0, buf, 128);
	crc_ref = bitcrc32(0, buf, 128);
	assertEqualInt(crc_lib, crc_ref);

	/* 3. Incremental */
	crc_lib = __archive_crc32(0, buf, 33);
	crc_lib = __archive_crc32(crc_lib, buf + 33, 95);
	crc_ref = bitcrc32(0, buf, 33);
	crc_ref = bitcrc32(crc_ref, buf + 33, 95);
	assertEqualInt(crc_lib, crc_ref);

	/* 4. Unaligned */
	crc_lib = __archive_crc32(0, buf + 1, 127);
	crc_ref = bitcrc32(0, buf + 1, 127);
	assertEqualInt(crc_lib, crc_ref);

	crc_lib = __archive_crc32(0, buf + 3, 128);
	crc_ref = bitcrc32(0, buf + 3, 128);
	assertEqualInt(crc_lib, crc_ref);

	/* 5. Large */
	crc_lib = __archive_crc32(0, buf, sizeof(buf));
	crc_ref = bitcrc32(0, buf, sizeof(buf));
	assertEqualInt(crc_lib, crc_ref);
}
