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

static int
touch(const char *fn)
{
	FILE *f = fopen(fn, "w");
	failure("Couldn't create file '%s', errno=%d (%s)\n",
	    fn, errno, strerror(errno));
	if (!assert(f != NULL))
		return (0); /* Failure. */
	fclose(f);
	return (1); /* Success */
}

DEFINE_TEST(test_strip_components)
{
	assertMakeDir("d0", 0755);
	assertChdir("d0");
	assertMakeDir("d1", 0755);
	assertMakeDir("d1/d2", 0755);
	assertMakeDir("d1/d2/d3", 0755);
	assertEqualInt(1, touch("d1/d2/f1"));
	assertMakeHardlink("l1", "d1/d2/f1");
	assertMakeHardlink("d1/l2", "d1/d2/f1");
	if (canSymlink()) {
		assertMakeSymlink("s1", "d1/d2/f1");
		assertMakeSymlink("d1/s2", "d2/f1");
	}
	assertChdir("..");

	assertEqualInt(0, systemf("%s -cf test.tar d0", testprog));

	assertMakeDir("target", 0755);
	assertEqualInt(0, systemf("%s -x -C target --strip-components 2 "
	    "-f test.tar", testprog));

	failure("d0/ is too short and should not get restored");
	assertFileNotExists("target/d0");
	failure("d0/d1/ is too short and should not get restored");
	assertFileNotExists("target/d1");
	failure("d0/d1/s2 is a symlink to something that won't be extracted");
	/* If platform supports symlinks, target/s2 is a broken symlink. */
	/* If platform does not support symlink, target/s2 doesn't exist. */
	assertFileNotExists("target/s2");
	if (canSymlink())
		assertIsSymlink("target/s2", "d2/f1");
	failure("d0/d1/d2 should be extracted");
	assertIsDir("target/d2", -1);

	/*
	 * This next is a complicated case.  d0/l1, d0/d1/l2, and
	 * d0/d1/d2/f1 are all hardlinks to the same file; d0/l1 can't
	 * be extracted with --strip-components=2 and the other two
	 * can.  Remember that tar normally stores the first file with
	 * a body and the other as hardlink entries to the first
	 * appearance.  So the final result depends on the order in
	 * which these three names get archived.  If d0/l1 is first,
	 * none of the three can be restored.  If either of the longer
	 * names are first, then the two longer ones can both be
	 * restored.
	 *
	 * The tree-walking code used by bsdtar always visits files
	 * before subdirectories, so bsdtar's behavior is fortunately
	 * deterministic:  d0/l1 will always get stored first and the
	 * other two will be stored as hardlinks to d0/l1.  Since
	 * d0/l1 can't be extracted, none of these three will be
	 * extracted.
	 *
	 * It may be worth extending this test to force a particular
	 * archiving order so as to exercise both of the cases described
	 * above.
	 *
	 * Of course, this is all totally different for cpio and newc
	 * formats because the hardlink management is different.
	 * TODO: Rename this to test_strip_components_tar and create
	 * parallel tests for cpio and newc formats.
	 */
	failure("d0/l1 is too short and should not get restored");
	assertFileNotExists("target/l1");
	failure("d0/d1/l2 is a hardlink to file whose name was too short");
	assertFileNotExists("target/l2");
	failure("d0/d1/d2/f1 is a hardlink to file whose name was too short");
	assertFileNotExists("target/d2/f1");
}
