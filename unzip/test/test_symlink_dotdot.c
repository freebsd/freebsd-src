/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kaif Khan
 * All rights reserved.
 */
#include "test.h"

/*
 * A symlink whose target is a bare ".." (or a path ending in "/..")
 * escapes the extraction directory just like "../" and "/../" do, so it
 * must be rejected as insecure.
 */
DEFINE_TEST(test_symlink_dotdot)
{
	const char *reffile = "test_symlink_dotdot.zip";
	int r;

	if (!canSymlink()) {
		skipping("System cannot create symlinks.");
		return;
	}

	assertUmask(0);
	extract_reference_file(reffile);

	r = systemf("%s %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);

	/* The two insecure symlinks must be skipped, not created. */
	assertFileNotExists("dotdot");
	assertFileNotExists("traildot");

	/* A regular entry in the same archive is still extracted. */
	assertIsReg("safe.txt", 0644);
}
