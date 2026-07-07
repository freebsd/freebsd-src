#include "test.h"

/*
 * Regression test for OOB read/write in the CAB MSZIP decoder.
 *
 * A crafted CFDATA block carries a stored-block deflate header whose LEN
 * field (0x9000) is larger than the actual compressed payload.  Without the
 * fix, the decoder reads one byte past the end of the input buffer on the
 * second archive_read_next_header() call (after the first call returns
 * ARCHIVE_FATAL due to the truncated payload).
 */
DEFINE_TEST(test_read_format_cab_mszip_oob)
{
	const uint16_t LEN   = 0x9000;
	const uint16_t NLEN  = (uint16_t)~LEN;
	const uint8_t  first = 0xFF;

	/*
	 * cbData and cbUncomp are chosen so that the two-byte sequences they
	 * occupy inside the CFDATA header also form a valid deflate stored-block
	 * length / complement pair when the decoder's read pointer is positioned
	 * one field early.
	 */
	const uint16_t cbData   = (LEN  >> 8) | ((NLEN & 0xff) << 8);
	const uint16_t cbUncomp = (NLEN >> 8) | ((uint16_t)first << 8);

	const size_t payload = (size_t)LEN - 1;   /* one byte short of LEN */
	const size_t total   = 36 + 8 + 18 + 8 + payload;

	uint8_t *b = (uint8_t *)calloc(1, total);
	assert(b != NULL);

	/* CFHEADER (36 bytes) */
	memcpy(b, "MSCF", 4);
	b[8]  = (uint8_t)(total);
	b[9]  = (uint8_t)(total >> 8);
	b[10] = (uint8_t)(total >> 16);
	b[11] = (uint8_t)(total >> 24);
	b[16] = 44;                     /* coffFiles: first CFFILE at offset 44 */
	b[24] = 3;  b[25] = 1;          /* version 1.3 */
	b[26] = 1;                      /* cFolders = 1 */
	b[28] = 1;                      /* cFiles   = 1 */

	/* CFFOLDER (8 bytes at offset 36) */
	b[36] = 62;                     /* coffCabStart: first CFDATA at offset 62 */
	b[40] = 1;                      /* cCFData = 1 */
	b[42] = 0x01;                   /* typeCompress = MSZIP */

	/* CFFILE (18 bytes at offset 44) */
	b[44] = 0x00; b[45] = 0x01;     /* cbFile = 0x100 */
	b[54] = 0x21;                   /* date low */
	b[58] = 0x20;                   /* attribs low */
	b[60] = 'a';  b[61] = 0;        /* szName "a" */

	/* CFDATA header (8 bytes at offset 62) */
	b[62] = 0x43; b[63] = 0x4B;     /* checksum bytes (also MSZIP "CK" marker) */
	b[64] = 0x01;
	b[65] = (uint8_t)(LEN & 0xff);
	b[66] = (uint8_t)(cbData);
	b[67] = (uint8_t)(cbData >> 8);
	b[68] = (uint8_t)(cbUncomp);
	b[69] = (uint8_t)(cbUncomp >> 8);

	/* Compressed payload: one byte short of what the stored-block LEN claims */
	memset(b + 70, first, payload);

	/* Open the crafted archive from memory */
	struct archive *a = archive_read_new();
	assert(a != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_cab(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_memory(a, b, total));

	struct archive_entry *ae;
	char buf[4096];

	/* The single CFFILE header is well-formed and should read cleanly. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("a", archive_entry_pathname(ae));

	/*
	 * Reading data must fail: cbUncomp (0xFF6F) exceeds the decoder's
	 * uncompressed buffer (0x8000), so the bounds check returns FATAL
	 * rather than writing past the buffer end.
	 */
	assertEqualInt(ARCHIVE_FATAL, archive_read_data(a, buf, sizeof(buf)));

	/* The archive is in FATAL state and should remain that way. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(b);
}
