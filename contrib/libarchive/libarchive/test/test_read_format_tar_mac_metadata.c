/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Zhaofeng Li
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_read_format_tar_mac_metadata)
{
	/*
	 This test tar file is crafted with two files in a specific order:

	 1. A ._-prefixed file with pax header containing the path attribute.
	 2. A file with a pax header but without the path attribute.

	 It's designed to trigger the case encountered in:
	 <https://github.com/libarchive/libarchive/pull/2636>

	 GNU tar is required to reproduce this tar file:

	 ```sh
	 NAME1="._101_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	 NAME2="goodname"
	 OUT="test_read_format_tar_mac_metadata_1.tar"

	 echo "content of badname" >"${NAME1}"
	 echo "content of goodname" >"${NAME2}"

	 rm -f "${OUT}"
	 gnutar \
	 	--mtime="@0" \
	 	--owner=0 --group=0 --numeric-owner \
	 	--pax-option=exthdr.name=%d/PaxHeaders/%f,atime:=0,ctime:=0,foo:=bar \
	 	--format=pax \
	 	-cf "${OUT}" \
	 	"${NAME1}" \
	 	"${NAME2}"
	 uuencode "${OUT}" "${OUT}" >"${OUT}.uu"

	 sha256sum "${OUT}"
	 sha256sum "${OUT}.uu"
	 ```
	*/
	const char *refname = "test_read_format_tar_mac_metadata_1.tar";
	char *p;
	size_t s;
	struct archive *a;
	struct archive_entry *ae;

	/*
	 * This is not a valid AppleDouble metadata file. It is merely to test that
	 * the correct bytes are read.
	 */
	const unsigned char appledouble[] = {
		0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x20, 0x6f, 0x66, 0x20, 0x62,
		0x61, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x0a
	};

	extract_reference_file(refname);
	p = slurpfile(&s, "%s", refname);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_set_option(a, "tar", "mac-ext", "1"));
	assertEqualIntA(a, ARCHIVE_OK, read_open_memory_seek(a, p, s, 1));

	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	/* Correct name and metadata bytes */
	assertEqualString("goodname", archive_entry_pathname(ae));

	const void *metadata = archive_entry_mac_metadata(ae, &s);
	if (assert(metadata != NULL)) {
		assertEqualMem(metadata, appledouble,
			sizeof(appledouble));
	}

	/* ... and nothing else */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_free(a));

	free(p);
}
