/*-
 * Copyright (c) 2021 Ryan Libby
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

static int
make_files(void)
{
	int ret;

	assertMakeDir("in", 0755);
	assertMakeDir("out", 0755);
	assertMakeFile("in/a", 0644, "a");
	assertMakeFile("in/b", 0644, "b");
	assertMakeFile("in/c", 0644, "c");
	assertEqualInt(0, systemf("%s cf a.tar -C in a", testprog));
	assertEqualInt(0, systemf("%s cf b.tar -C in b", testprog));
	/* An archive formed with cat, and readable with --ignore-zeros. */
	ret = systemf("cat a.tar b.tar > ab-cat.tar");
	if (ret != 0) {
		skipping("This test requires a `cat` program");
		return (ret);
	}

	return (0);
}

DEFINE_TEST(test_option_ignore_zeros_mode_t)
{
	if (make_files())
		return;

	/* Generate expected t-mode output. */
	assertEqualInt(0, systemf(
	    "%s cf ab-norm.tar -C in a b > norm-c.out 2> norm-c.err",
	    testprog));
	assertEmptyFile("norm-c.err");
	assertEmptyFile("norm-c.out");
	assertEqualInt(0, systemf(
	    "%s tf ab-norm.tar > norm-t.out 2> norm-t.err",
	    testprog));
	assertEmptyFile("norm-t.err");

	/* Test output. */
	assertEqualInt(0, systemf(
	    "%s tf ab-cat.tar --ignore-zeros > test.out 2> test.err",
	    testprog));
	assertEmptyFile("test.err");

	assertEqualFile("test.out", "norm-t.out");
}

DEFINE_TEST(test_option_ignore_zeros_mode_x)
{
	if (make_files())
		return;

	assertEqualInt(0, systemf(
	    "%s xf ab-cat.tar --ignore-zeros -C out > test.out 2> test.err",
	    testprog));
	assertEmptyFile("test.err");
	assertEmptyFile("test.out");

	assertEqualFile("out/a", "in/a");
	assertEqualFile("out/b", "in/b");
}

DEFINE_TEST(test_option_ignore_zeros_mode_c)
{
	if (make_files())
		return;

	assertEqualInt(0, systemf(
	    "%s cf abc.tar --ignore-zeros @ab-cat.tar -C in c "
	    "> test-c.out 2> test-c.err",
	    testprog));
	assertEmptyFile("test-c.err");
	assertEmptyFile("test-c.out");

	assertEqualInt(0, systemf(
	    "%s xf abc.tar -C out > test-x.out 2> test-x.err",
	    testprog));
	assertEmptyFile("test-x.err");
	assertEmptyFile("test-x.out");

	assertEqualFile("out/a", "in/a");
	assertEqualFile("out/b", "in/b");
	assertEqualFile("out/c", "in/c");
}

static void
test_option_ignore_zeros_mode_ru(const char *mode)
{
	if (make_files())
		return;

	assertEqualInt(0, systemf(
	    "%s %sf ab-cat.tar --ignore-zeros -C in c "
	    "> test-ru.out 2> test-ru.err",
	    testprog, mode));
	assertEmptyFile("test-ru.err");
	assertEmptyFile("test-ru.out");

	assertEqualInt(0, systemf(
	    "%s xf ab-cat.tar --ignore-zeros -C out "
	    "> test-x.out 2> test-x.err",
	    testprog));
	assertEmptyFile("test-x.err");
	assertEmptyFile("test-x.out");

	assertEqualFile("out/a", "in/a");
	assertEqualFile("out/b", "in/b");
	assertEqualFile("out/c", "in/c");
}

DEFINE_TEST(test_option_ignore_zeros_mode_r)
{
	test_option_ignore_zeros_mode_ru("r");
}

DEFINE_TEST(test_option_ignore_zeros_mode_u)
{
	test_option_ignore_zeros_mode_ru("u");
}
