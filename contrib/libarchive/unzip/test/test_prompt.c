/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Akash Deshmukh
 * All rights reserved.
 */
#include "test.h"

/* Test that a long answer to the overwrite prompt is read as one line */
DEFINE_TEST(test_prompt_long_answer)
{
	const char *reffile = "test_prompt.zip";
	int r;

	assertMakeDir("test_prompt", 0755);
	assertMakeFile("test_prompt/a", 0644, "orig a\n");
	assertMakeFile("test_prompt/b", 0644, "orig b\n");
	assertMakeFile("answers", 0644, "yes\nno\n");

	extract_reference_file(reffile);
	r = systemf("%s %s <answers >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertTextFileContents(
	    "replace test_prompt/a? [y]es, [n]o, [A]ll, [N]one, [r]ename: "
	    "replace test_prompt/b? [y]es, [n]o, [A]ll, [N]one, [r]ename: ",
	    "test.err");

	assertTextFileContents("contents a\n", "test_prompt/a");
	assertTextFileContents("orig b\n", "test_prompt/b");
}
