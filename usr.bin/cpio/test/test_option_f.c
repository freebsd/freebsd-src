/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

/*
 * Unpack the archive in a new dir.
 */
static void
unpack(const char *dirname, const char *option)
{
	int r;

	assertEqualInt(0, mkdir(dirname, 0755));
	assertEqualInt(0, chdir(dirname));
	extract_reference_file("test_option_f.cpio");
	r = systemf("%s -i %s < test_option_f.cpio > copy-no-a.out 2>copy-no-a.err", testprog, option);
	assertEqualInt(0, r);
	assertEqualInt(0, chdir(".."));
}

DEFINE_TEST(test_option_f)
{
	/* Calibrate:  No -f option, so everything should be extracted. */
	unpack("t0", "");
	assertEqualInt(0, access("t0/a123", F_OK));
	assertEqualInt(0, access("t0/a234", F_OK));
	assertEqualInt(0, access("t0/b123", F_OK));
	assertEqualInt(0, access("t0/b234", F_OK));

	/* Don't extract 'a*' files. */
	unpack("t1", "-f 'a*'");
	assert(0 != access("t1/a123", F_OK));
	assert(0 != access("t1/a234", F_OK));
	assertEqualInt(0, access("t1/b123", F_OK));
	assertEqualInt(0, access("t1/b234", F_OK));

	/* Don't extract 'b*' files. */
	unpack("t2", "-f 'b*'");
	assertEqualInt(0, access("t2/a123", F_OK));
	assertEqualInt(0, access("t2/a234", F_OK));
	assert(0 != access("t2/b123", F_OK));
	assert(0 != access("t2/b234", F_OK));
}
