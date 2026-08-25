/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Zhaofeng Li
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_mtime)
{
	/* Create three files with different mtimes. */
	assertMakeDir("in", 0755);
	assertChdir("in");
	assertMakeFile("new_mtime", 0666, "new");
	assertUtimes("new_mtime", 100000, 0, 100000, 0);
	assertMakeFile("mid_mtime", 0666, "mid");
	assertUtimes("mid_mtime", 10000, 0, 10000, 0);
	assertMakeFile("old_mtime", 0666, "old");
	// assertion_utimes silently discards 0 :(
	assertUtimes("old_mtime", 1, 0, 1, 0);

	/* Archive with --mtime 86400 */
	assertEqualInt(0,
		systemf("%s --format pax -cf ../noclamp.tar "
			"--mtime \"1970/1/2 0:0:0 UTC\" .",
			testprog));
	assertChdir("..");

	assertMakeDir("out.noclamp", 0755);
	assertChdir("out.noclamp");
	assertEqualInt(0, systemf("%s xf ../noclamp.tar", testprog));
	assertFileMtime("new_mtime", 86400, 0);
	assertFileMtime("mid_mtime", 86400, 0);
	assertFileMtime("old_mtime", 86400, 0);
	assertChdir("..");

	/* Archive with --mtime 86400 --clamp-mtime */
	assertChdir("in");
	assertEqualInt(0,
		systemf("%s --format pax -cf ../clamp.tar "
			"--mtime \"1970/1/2 0:0:0 UTC\" --clamp-mtime .",
			testprog));
	assertChdir("..");

	assertMakeDir("out.clamp", 0755);
	assertChdir("out.clamp");
	assertEqualInt(0, systemf("%s xf ../clamp.tar", testprog));
	assertFileMtime("new_mtime", 86400, 0);
	assertFileMtime("mid_mtime", 10000, 0);
	assertFileMtime("old_mtime", 1, 0);
	assertChdir("..");

	/* Archive-to-archive copy with --mtime 0 */
	assertEqualInt(0,
		systemf("%s --format pax -cf ./archive2archive.tar "
			"--mtime \"1970/1/1 0:0:0 UTC\" @noclamp.tar",
			testprog));
	assertMakeDir("out.archive2archive", 0755);
	assertChdir("out.archive2archive");
	assertEqualInt(0, systemf("%s xf ../archive2archive.tar", testprog));
	assertFileMtime("new_mtime", 0, 0);
	assertFileMtime("mid_mtime", 0, 0);
	assertFileMtime("old_mtime", 0, 0);
	assertChdir("..");
}
