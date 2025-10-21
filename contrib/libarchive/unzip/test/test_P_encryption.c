/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test P arg - password protected */
DEFINE_TEST(test_P_encryption)
{
	const char *reffile = "test_encrypted.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -P password %s >test.out 2>test.err", testprog, reffile);
	if (r == 256) {
		assertTextFileContents("unzip: Decryption is unsupported due to lack of crypto library\n", "test.err");
	} else {
		assertEqualInt(0, r);
		assertNonEmptyFile("test.out");
		assertEmptyFile("test.err");

		assertTextFileContents("plaintext\n", "encrypted/file.txt");
	}
}
